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

#ifndef ABSL_SYNCHRONIZATION_INTERNAL_KERNEL_TIMEOUT_H_
#define ABSL_SYNCHRONIZATION_INTERNAL_KERNEL_TIMEOUT_H_

#include <algorithm>
#include <chrono>  // NOLINT(build/c++11)
#include <cstdint>
#include <ctime>
#include <limits>

#include "absl/base/config.h"
#include "absl/base/internal/raw_logging.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace synchronization_internal {

class Waiter;

// An optional timeout, with nanosecond granularity.
//
// This is a private low-level API for use by a handful of low-level
// components. Higher-level components should build APIs based on
// absl::Time and absl::Duration.
class KernelTimeout {
 public:
  // Construct an absolute timeout that should expire at `t`.
  explicit KernelTimeout(absl::Time t);

  // Construct a relative timeout that should expire after `d`.
  explicit KernelTimeout(absl::Duration d);

  // Infinite timeout.
  constexpr KernelTimeout() : rep_(kNoTimeout) {}

  // A more explicit factory for those who prefer it.
  // Equivalent to `KernelTimeout()`.
  static constexpr KernelTimeout Never() { return KernelTimeout(); }

  // Returns true if there is a timeout that will eventually expire.
  // Returns false if the timeout is infinite.
  bool has_timeout() const { return rep_ != kNoTimeout; }

  // If `has_timeout()` is true, returns true if the timeout was provided as an
  // `absl::Time`. The return value is undefined if `has_timeout()` is false
  // because all indefinite timeouts are equivalent.
  bool is_absolute_timeout() const { return (rep_ & 1) == 0; }

  // If `has_timeout()` is true, returns true if the timeout was provided as an
  // `absl::Duration`. The return value is undefined if `has_timeout()` is false
  // because all indefinite timeouts are equivalent.
  bool is_relative_timeout() const { return (rep_ & 1) == 1; }

  // Convert to `struct timespec` for interfaces that expect an absolute
  // timeout. If !has_timeout() or is_relative_timeout(), attempts to convert to
  // a reasonable absolute timeout, but callers should to test has_timeout() and
  // is_relative_timeout() and prefer to use a more appropriate interface.
  struct timespec MakeAbsTimespec() const;

  // Convert to `struct timespec` for interfaces that expect a relative
  // timeout. If !has_timeout() or is_absolute_timeout(), attempts to convert to
  // a reasonable relative timeout, but callers should to test has_timeout() and
  // is_absolute_timeout() and prefer to use a more appropriate interface.
  struct timespec MakeRelativeTimespec() const;

  // Convert to unix epoch nanos for interfaces that expect an absolute timeout
  // in nanoseconds. If !has_timeout() or is_relative_timeout(), attempts to
  // convert to a reasonable absolute timeout, but callers should to test
  // has_timeout() and is_relative_timeout() and prefer to use a more
  // appropriate interface.
  int64_t MakeAbsNanos() const;

  // Converts to milliseconds from now, or INFINITE when
  // !has_timeout(). For use by SleepConditionVariableSRW on
  // Windows. Callers should recognize that the return value is a
  // relative duration (it should be recomputed by calling this method
  // in the case of a spurious wakeup).
  // This header file may be included transitively by public header files,
  // so we define our own DWORD and INFINITE instead of getting them from
  // <intsafe.h> and <WinBase.h>.
  typedef unsigned long DWord;  // NOLINT
  DWord InMillisecondsFromNow() const;

  // Convert to std::chrono::time_point for interfaces that expect an absolute
  // timeout, like std::condition_variable::wait_until(). If !has_timeout() or
  // is_relative_timeout(), attempts to convert to a reasonable absolute
  // timeout, but callers should test has_timeout() and is_relative_timeout()
  // and prefer to use a more appropriate interface.
  std::chrono::time_point<std::chrono::system_clock> ToChronoTimePoint() const;

  // Convert to std::chrono::time_point for interfaces that expect a relative
  // timeout, like std::condition_variable::wait_for(). If !has_timeout() or
  // is_absolute_timeout(), attempts to convert to a reasonable relative
  // timeout, but callers should test has_timeout() and is_absolute_timeout()
  // and prefer to use a more appropriate interface.
  std::chrono::nanoseconds ToChronoDuration() const;

 private:
  // Internal representation.
  //   - If the value is kNoTimeout, then the timeout is infinite, and
  //     has_timeout() will return true.
  //   - If the low bit is 0, then the high 63 bits is number of nanoseconds
  //     after the unix epoch.
  //   - If the low bit is 1, then the high 63 bits is a relative duration in
  //     nanoseconds.
  uint64_t rep_;

  // Returns the number of nanoseconds stored in the internal representation.
  // Together with is_absolute_timeout() and is_relative_timeout(), the return
  // value is used to compute when the timeout should occur.
  int64_t RawNanos() const { return static_cast<int64_t>(rep_ >> 1); }

  // A value that represents no timeout (or an infinite timeout).
  static constexpr uint64_t kNoTimeout = (std::numeric_limits<uint64_t>::max)();

  // The maximum value that can be stored in the high 63 bits.
  static constexpr int64_t kMaxNanos = (std::numeric_limits<int64_t>::max)();
};

}  // namespace synchronization_internal
ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_SYNCHRONIZATION_INTERNAL_KERNEL_TIMEOUT_H_
