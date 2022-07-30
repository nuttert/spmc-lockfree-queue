#pragma once
 
#include <array>
#include <atomic>
 
#include <folly/Likely.h>
#include <folly/lang/Align.h>
#include <folly/lang/Bits.h>
 
namespace trading {
 
//
// can read, can write: read_index | write_index
// if read_index == write_index, than readers overtook writer and must wait until new writes will happen
// can't move write_index forward: write_index | read_index
 
template <typename T, std::size_t N>
class SPMCLockfreeQueue {
public:
    bool Push(T val) {
        auto write_index = write_index_.load(std::memory_order_acquire);
        auto next_write_index = (write_index + 1) & (N - 1);
 
        auto read_index = read_index_.load(std::memory_order_acquire);
        if (next_write_index == read_index) {
            return false;
        }
 
        elems_[write_index] = val;
 
        write_index_.store(next_write_index, std::memory_order_release);
        return true;
    }
 
    bool Pop(T& val) {
        std::size_t read_index = read_index_.load(std::memory_order_acquire);
 
        // Can fail only if readers overtook writer
        if (read_index != write_index_.load(std::memory_order_acquire)) {
            auto next_read_index = (1 + read_index) & (N - 1);
            if (read_index_.compare_exchange_strong(read_index, next_read_index, std::memory_order_acq_rel,
                                                    std::memory_order_acquire)) {
                val = elems_[read_index];
                return true;
            } else {
                return false;
            }
        }
        return false;
    }
 
    bool Empty() const {
        return read_index_.load(std::memory_order_acquire) == write_index_.load(std::memory_order_acquire);
    }
 
private:
    static_assert(folly::isPowTwo(N), "SPMCLockfreeQueue size N must be power of 2!");
 
    // static_assert(std::atomic<T>::is_lock_free(),
    //               "Underlying type for SPMCLockfreeQueue must be trivial and lock_free for std::atomic!");
 
    static constexpr std::size_t kCommonPageSize = 4096;
 
    alignas(kCommonPageSize) std::array<T, N> elems_{T{}};
 
    alignas(folly::hardware_destructive_interference_size) std::atomic<std::size_t> write_index_{0};
    alignas(folly::hardware_destructive_interference_size) std::atomic<std::size_t> read_index_{0};
};
 
}  // namespace trading
 