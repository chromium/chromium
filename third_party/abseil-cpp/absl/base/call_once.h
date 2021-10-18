// Copyright 2017 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// -----------------------------------------------------------------------------
// File: call_once.h
// -----------------------------------------------------------------------------
//
// This header file provides an Abseil version of `std::call_once` for invoking
// a given function at most once, across all threads. This Abseil version is
// faster than the C++11 version and incorporates the C++17 argument-passing
// fix, so that (for example) non-const references may be passed to the invoked
// function.

#ifndef ABSL_BASE_CALL_ONCE_H_
#define ABSL_BASE_CALL_ONCE_H_

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <type_traits>
#include <utility>

#include "absl/base/internal/invoke.h"
#include "absl/base/internal/low_level_scheduling.h"
#include "absl/base/internal/raw_logging.h"
#include "absl/base/internal/scheduling_mode.h"
#include "absl/base/internal/spinlock_wait.h"
#include "absl/base/macros.h"
#include "absl/base/optimization.h"
#include "absl/base/port.h"

namespace absl {
ABSL_NAMESPACE_BEGIN

class once_flag;

namespace base_internal {
std::atomic<uint32_t>* ControlWord(absl::once_flag* flag);
}  // namespace base_internal

// call_once()
// 
// For all invocations using a given `once_flag`, invokes a given `fn` exactly
// once across all threads. The first call to `call_once()` with a particular
// `once_flag` argument (that does not throw an exception) will run the
// specified function with the provided `args`; other calls with the same
// `once_flag` argument will not run the function, but will wait
// for the provided function to finish running (if it is still running).
//
// This mechanism provides a safe, simple, and fast mechanism for one-time
// initialization in a multi-threaded process.
// 对于使用给定 `once_flag` 的所有调用，在所有线程中只调用给定的 `fn` 一次。 第一次使用特
// 定的 `once_flag` 参数（不会抛出异常）调用 `call_once()` 将使用提供的 `args` 运行指定
// 的函数； 具有相同 `once_flag` 参数的其他调用将不会运行该函数，而是会等待提供的函数完成运
// 行（如果它仍在运行）。 这种机制为多线程进程中的一次性初始化提供了一种安全、简单、快速的机制。
// Example:
//
// class MyInitClass {
//  public:
//   ...
//   mutable absl::once_flag once_;
//
//   MyInitClass* init() const {
//     absl::call_once(once_, &MyInitClass::Init, this);
//     return ptr_;
//   }
// 这个模板函数，可以传递的回调函数的形参是任意的：任意参数类型和个数
template <typename Callable, typename... Args>
void call_once(absl::once_flag& flag, Callable&& fn, Args&&... args);

// once_flag
//
// Objects of this type are used to distinguish calls to `call_once()` and
// ensure the provided function is only invoked once across all threads. This
// type is not copyable or movable. However, it has a `constexpr`
// constructor, and is safe to use as a namespace-scoped global variable.
// 这种类型的对象用于区分对 `call_once()` 的调用，并确保提供的函数在所有线程中只被调用一次。
// 这种类型不可复制或移动。 但是，它有一个 `constexpr` 构造函数，可以安全地用作命名空间范
// 围的全局变量。
class once_flag {
 public:
  constexpr once_flag() : control_(0) {} // 初始化为一个无效值
  once_flag(const once_flag&) = delete; // 不能复制
  once_flag& operator=(const once_flag&) = delete; // 不能赋值

 private:
  friend std::atomic<uint32_t>* base_internal::ControlWord(once_flag* flag);
  std::atomic<uint32_t> control_;
};

//------------------------------------------------------------------------------
// End of public interfaces.
// Implementation details follow.
// 上面是对外的公共接口，下面是不对外的、内部使用的细节
//------------------------------------------------------------------------------

namespace base_internal {

// Like call_once, but uses KERNEL_ONLY scheduling. Intended to be used to
// initialize entities used by the scheduler implementation.
// 类似于 call_once，但使用 KERNEL_ONLY 调度。 旨在用于初始化调度程序实现使用的实体。
template <typename Callable, typename... Args>
void LowLevelCallOnce(absl::once_flag* flag, Callable&& fn, Args&&... args);

// Disables scheduling while on stack when scheduling mode is non-cooperative.
// No effect for cooperative scheduling modes.
// 当调度模式为非合作时，在栈上禁用调度。
// 对协作调度模式没有影响。
class SchedulingHelper {
 public:
  explicit SchedulingHelper(base_internal::SchedulingMode mode) : mode_(mode) {
    if (mode_ == base_internal::SCHEDULE_KERNEL_ONLY) {
      guard_result_ = base_internal::SchedulingGuard::DisableRescheduling();
    }
  }

  ~SchedulingHelper() {
    if (mode_ == base_internal::SCHEDULE_KERNEL_ONLY) {
      base_internal::SchedulingGuard::EnableRescheduling(guard_result_);
    }
  }

 private:
  base_internal::SchedulingMode mode_;
  bool guard_result_;
};

// Bit patterns for call_once state machine values.  Internal implementation
// detail, not for use by clients.
// call_once 状态机值的位模式。 内部实现细节，不供客户使用。

// The bit patterns are arbitrarily chosen from unlikely values, to aid in
// debugging.  However, kOnceInit must be 0, so that a zero-initialized
// once_flag will be valid for immediate use.
// 位模式是从不太可能的值中任意选择的，以帮助调试。 但是，kOnceInit 必须为 0，以便零初
// 始化的 once_flag 将立即使用有效。
enum {
  kOnceInit = 0,
  kOnceRunning = 0x65C2937B,
  kOnceWaiter = 0x05A308D2,
  // A very small constant is chosen for kOnceDone so that it fit in a single
  // compare with immediate instruction for most common ISAs.  This is verified
  // for x86, POWER and ARM.
  // 为 kOnceDone 选择了一个非常小的常量，以便它适合与大多数常见 ISA 的立即指令进行单个比
  // 较。 这已针对 x86、POWER 和 ARM 进行了验证。
  kOnceDone = 221,    // Random Number
};

template <typename Callable, typename... Args>
ABSL_ATTRIBUTE_NOINLINE
void CallOnceImpl(std::atomic<uint32_t>* control,
                  base_internal::SchedulingMode scheduling_mode,
                  Callable&& fn, Args&&... args) {

#ifndef NDEBUG
  {
    // load(): 原子地获得原子对象的值
    uint32_t old_control = control->load(std::memory_order_relaxed);
    if (old_control != kOnceInit &&
        old_control != kOnceRunning &&
        old_control != kOnceWaiter &&
        old_control != kOnceDone) {
      // 原子量状态非法
      ABSL_RAW_LOG(FATAL, "Unexpected value for control word: 0x%lx",
                   static_cast<unsigned long>(old_control));  // NOLINT
    }
  }
#endif  // NDEBUG
  // 状态机: from : to : done状态，Transition本意是 "过渡"。
  // 自旋锁-等待-过渡
  static const base_internal::SpinLockWaitTransition trans[] = {
    {kOnceInit, kOnceRunning, true},    // 初始状态 -> 执行状态：执行第1次
    {kOnceRunning, kOnceWaiter, false}, // 自旋等待
    {kOnceDone, kOnceDone, true} // 表示已经执行过，且完成：不阻塞跳出不执行，执行其他的
};

  // Must do this before potentially modifying control word's state.
  // 必须在可能修改控制字的状态之前执行此操作。
  base_internal::SchedulingHelper maybe_disable_scheduling(scheduling_mode);
  // Short circuit the simplest case to avoid procedure call overhead.
  // The base_internal::SpinLockWait() call returns either kOnceInit or
  // kOnceDone. If it returns kOnceDone, it must have loaded the control word
  // with std::memory_order_acquire and seen a value of kOnceDone.
  // 短路最简单的情况，以避免过程调用开销。base_internal::SpinLockWait()调用返回 
  // kOnceInit 或 kOnceDone。如果它返回 kOnceDone，它一定已经用 
  // std::memory_order_acquire 加载了控制字并且看到了 kOnceDone 的值。
  uint32_t old_control = kOnceInit;
  // 第1次(当前线程)：control的默认状态是0，即:kOnceInit，在
  // compare_exchange_strong()中被交换为kOnceRunning，并返回为true，则该thread会
  // 继续执行invoke()，即执行call_once()的回调函数，完成仅有的1次调用。
  // >=第2次(其他线程)：分两种情况：(1)如果第1条线程是kOnceRunning状态，则会命中
  // trans[1].to(因为多条线程共同访问的是状态是std::once_flag的参数w)，即
  // kOnceWaiter，且done=false，所以会自旋阻塞；(2)如果第1条线程已经执行完毕，即状态是
  // kOnceDone，则其他线程会执行trans[2].to，即是kOnceDone，并且done=true，直接
  // 返回，且返回的是kOnceDone，因此是跳过下面的if语句。
  // 综上：只有一条线程会执行到call_once()的callback函数，其他线程要么阻塞等待第1条
  // 线程执行完，要么直接跳过，因此正如其名："call once"。
  // 
  // 【C++内存栅栏】
  // std::memory_order_relaxed 只保证当前操作的原子性，不考虑线程间的同步，其他线程
  // 可能读到新值，也可能读到旧值。
  // 
  if (control->compare_exchange_strong(old_control, kOnceRunning,
                                       std::memory_order_relaxed) ||
      base_internal::SpinLockWait(control, ABSL_ARRAYSIZE(trans), trans,
                                  scheduling_mode) == kOnceInit) {

    base_internal::invoke(std::forward<Callable>(fn),
                          std::forward<Args>(args)...);

    // exchange(): 原子地替换原子对象的值并返回它先前持有的值
    old_control = control->exchange(base_internal::kOnceDone, 
                                    std::memory_order_release);
    if (old_control == base_internal::kOnceWaiter) {
      base_internal::SpinLockWake(control, true); // 唤醒等待状态的其他线程
    }
  }  // else *control is already kOnceDone
}

inline std::atomic<uint32_t>* ControlWord(once_flag* flag) {
  return &flag->control_;
}

template <typename Callable, typename... Args>
void LowLevelCallOnce(absl::once_flag* flag, Callable&& fn, Args&&... args) {
  std::atomic<uint32_t>* once = base_internal::ControlWord(flag);
  uint32_t s = once->load(std::memory_order_acquire);
  if (ABSL_PREDICT_FALSE(s != base_internal::kOnceDone)) {
    base_internal::CallOnceImpl(once, base_internal::SCHEDULE_KERNEL_ONLY,
                                std::forward<Callable>(fn),
                                std::forward<Args>(args)...);
  }
}

}  // namespace base_internal

template <typename Callable, typename... Args>
void call_once(absl::once_flag& flag, Callable&& fn, Args&&... args) {
  std::atomic<uint32_t>* once = base_internal::ControlWord(&flag);
  uint32_t s = once->load(std::memory_order_acquire);
  if (ABSL_PREDICT_FALSE(s != base_internal::kOnceDone)) {
    base_internal::CallOnceImpl(
        once, base_internal::SCHEDULE_COOPERATIVE_AND_KERNEL,
        std::forward<Callable>(fn), std::forward<Args>(args)...);
  }
}

ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_BASE_CALL_ONCE_H_
