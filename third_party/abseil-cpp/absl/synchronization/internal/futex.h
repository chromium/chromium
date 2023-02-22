// Copyright 2020 The Abseil Authors.
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
#ifndef ABSL_SYNCHRONIZATION_INTERNAL_FUTEX_H_
#define ABSL_SYNCHRONIZATION_INTERNAL_FUTEX_H_

#include "absl/base/config.h"

#ifndef _WIN32
#include <sys/time.h>
#include <unistd.h>
#endif

#ifdef __linux__
#include <linux/futex.h>
#include <sys/syscall.h>
#endif

#include <errno.h>
#include <stdio.h>
#include <time.h>

#include <atomic>
#include <cstdint>

#include "absl/base/optimization.h"
#include "absl/synchronization/internal/kernel_timeout.h"

#ifdef ABSL_INTERNAL_HAVE_FUTEX
#error ABSL_INTERNAL_HAVE_FUTEX may not be set on the command line
#elif defined(__BIONIC__)
// Bionic supports all the futex operations we need even when some of the futex
// definitions are missing.
#define ABSL_INTERNAL_HAVE_FUTEX
#elif defined(__linux__) && defined(FUTEX_CLOCK_REALTIME)
// FUTEX_CLOCK_REALTIME requires Linux >= 2.6.28.
#define ABSL_INTERNAL_HAVE_FUTEX
#endif

#ifdef ABSL_INTERNAL_HAVE_FUTEX

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace synchronization_internal {

// Some Android headers are missing these definitions even though they
// support these futex operations.
#ifdef __BIONIC__
#ifndef SYS_futex
#define SYS_futex __NR_futex
#endif
#ifndef FUTEX_WAIT_BITSET
#define FUTEX_WAIT_BITSET 9
#endif
#ifndef FUTEX_PRIVATE_FLAG
#define FUTEX_PRIVATE_FLAG 128
#endif
#ifndef FUTEX_CLOCK_REALTIME
#define FUTEX_CLOCK_REALTIME 256
#endif
#ifndef FUTEX_BITSET_MATCH_ANY
#define FUTEX_BITSET_MATCH_ANY 0xFFFFFFFF
#endif
#endif

#if defined(__NR_futex_time64) && !defined(SYS_futex_time64)
#define SYS_futex_time64 __NR_futex_time64
#endif

#if defined(SYS_futex_time64) && !defined(SYS_futex)
#define SYS_futex SYS_futex_time64
#endif

class FutexImpl {
 public:
  // Atomically check that `*v == val`, and if it is, then sleep until the
  // timeout `t` has been reached, or until woken by `Wake()`.
  static int WaitUntil(std::atomic<int32_t>* v, int32_t val,
                       KernelTimeout t) {
    if (!t.has_timeout()) {
      return Wait(v, val);
    } else if (t.is_absolute_timeout()) {
      auto abs_timespec = t.MakeAbsTimespec();
      return WaitAbsoluteTimeout(v, val, &abs_timespec);
    } else {
      auto rel_timespec = t.MakeRelativeTimespec();
      return WaitRelativeTimeout(v, val, &rel_timespec);
    }
  }

  // Atomically check that `*v == val`, and if it is, then sleep until the until
  // woken by `Wake()`.
  static int Wait(std::atomic<int32_t>* v, int32_t val) {
    return WaitAbsoluteTimeout(v, val, nullptr);
  }

  // Atomically check that `*v == val`, and if it is, then sleep until
  // CLOCK_REALTIME reaches `*abs_timeout`, or until woken by `Wake()`.
  static int WaitAbsoluteTimeout(std::atomic<int32_t>* v, int32_t val,
                                 const struct timespec* abs_timeout) {
    // https://locklessinc.com/articles/futex_cheat_sheet/
    // Unlike FUTEX_WAIT, FUTEX_WAIT_BITSET uses absolute time.
    auto err =
        syscall(SYS_futex, reinterpret_cast<int32_t*>(v),
                FUTEX_WAIT_BITSET | FUTEX_PRIVATE_FLAG | FUTEX_CLOCK_REALTIME,
                val, abs_timeout, nullptr, FUTEX_BITSET_MATCH_ANY);
    if (err != 0) {
      return -errno;
    }
    return 0;
  }

  // Atomically check that `*v == val`, and if it is, then sleep until
  // `*rel_timeout` has elapsed, or until woken by `Wake()`.
  static int WaitRelativeTimeout(std::atomic<int32_t>* v, int32_t val,
                                 const struct timespec* rel_timeout) {
    // Atomically check that the futex value is still 0, and if it
    // is, sleep until abs_timeout or until woken by FUTEX_WAKE.
    auto err = syscall(SYS_futex, reinterpret_cast<int32_t*>(v),
                       FUTEX_PRIVATE_FLAG, val, rel_timeout);
    if (err != 0) {
      return -errno;
    }
    return 0;
  }

  // Wakes at most `count` waiters that have entered the sleep state on `v`.
  static int Wake(std::atomic<int32_t>* v, int32_t count) {
    auto err = syscall(SYS_futex, reinterpret_cast<int32_t*>(v),
                       FUTEX_WAKE | FUTEX_PRIVATE_FLAG, count);
    if (ABSL_PREDICT_FALSE(err < 0)) {
      return -errno;
    }
    return 0;
  }
};

class Futex : public FutexImpl {};

}  // namespace synchronization_internal
ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_INTERNAL_HAVE_FUTEX

#endif  // ABSL_SYNCHRONIZATION_INTERNAL_FUTEX_H_
