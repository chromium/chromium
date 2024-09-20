// Copyright 2023 The Abseil Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "absl/synchronization/internal/waiter.h"

#include <ctime>
#include <iostream>
#include <ostream>

#include "absl/base/config.h"
#include "absl/random/random.h"
#include "absl/synchronization/internal/create_thread_identity.h"
#include "absl/synchronization/internal/futex_waiter.h"
#include "absl/synchronization/internal/kernel_timeout.h"
#include "absl/synchronization/internal/pthread_waiter.h"
#include "absl/synchronization/internal/sem_waiter.h"
#include "absl/synchronization/internal/stdcpp_waiter.h"
#include "absl/synchronization/internal/thread_pool.h"
#include "absl/synchronization/internal/win32_waiter.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "gtest/gtest.h"

#ifdef ABSL_INTERNAL_HAVE_WIN32_WAITER
#include <windows.h>
#endif

// Test go/btm support by randomizing the value of clock_gettime() for
// CLOCK_MONOTONIC. This works by overriding a weak symbol in glibc.
// We should be resistant to this randomization when !SupportsSteadyClock().
#if defined(__GOOGLE_GRTE_VERSION__) &&      \
    !defined(ABSL_HAVE_ADDRESS_SANITIZER) && \
    !defined(ABSL_HAVE_MEMORY_SANITIZER) &&  \
    !defined(ABSL_HAVE_THREAD_SANITIZER)
extern "C" int __clock_gettime(clockid_t c, struct timespec* ts);

extern "C" int clock_gettime(clockid_t c, struct timespec* ts) {
  if (c == CLOCK_MONOTONIC &&
      !absl::synchronization_internal::KernelTimeout::SupportsSteadyClock()) {
    thread_local absl::BitGen gen;  // NOLINT
    ts->tv_sec = absl::Uniform(gen, 0, 1'000'000'000);
    ts->tv_nsec = absl::Uniform(gen, 0, 1'000'000'000);
    return 0;
  }
  return __clock_gettime(c, ts);
}
#endif

#ifdef ABSL_INTERNAL_HAVE_WIN32_WAITER
// Returns the "interrupt time bias" from KUSER_SHARED_DATA, which is in units
// of 100ns.
static uint64_t GetSuspendTime() {
  return *reinterpret_cast<uint64_t volatile*>(
      0x7FFE0000 /* KUSER_SHARED_DATA */ + 0x3B0);
}

// Like GetTickCount(), but excludes suspend time.
static unsigned int GetTickCountExcludingSuspend() {
  unsigned int result;
  uint64_t prev_bias;
  uint64_t bias = GetSuspendTime();
  do {
    prev_bias = bias;
    result = GetTickCount();
    bias = GetSuspendTime();
  } while (bias != prev_bias);
  return result - bias / 10000;
}
#endif

struct BenchmarkTime {
  absl::Time time;
  absl::Time vtime;
};

static BenchmarkTime BenchmarkNow() {
  absl::Time now = absl::Now();
  absl::Time vnow = now;
#ifdef ABSL_INTERNAL_HAVE_WIN32_WAITER
  vnow = absl::UnixEpoch() + absl::Milliseconds(GetTickCountExcludingSuspend());
#endif
  return {now, vnow};
}

namespace {

TEST(Waiter, PrintPlatformImplementation) {
  // Allows us to verify that the platform is using the expected implementation.
  std::cout << absl::synchronization_internal::Waiter::kName << std::endl;
}

template <typename T>
class WaiterTest : public ::testing::Test {
 public:
  // Waiter implementations assume that a ThreadIdentity has already been
  // created.
  WaiterTest() {
    absl::synchronization_internal::GetOrCreateCurrentThreadIdentity();
  }
};

TYPED_TEST_SUITE_P(WaiterTest);

absl::Duration WithTolerance(absl::Duration d) { return d * 0.95; }

TYPED_TEST_P(WaiterTest, WaitNoTimeout) {
  absl::synchronization_internal::ThreadPool tp(1);
  TypeParam waiter;
  tp.Schedule([&]() {
    // Include some `Poke()` calls to ensure they don't cause `waiter` to return
    // from `Wait()`.
    waiter.Poke();
    absl::SleepFor(absl::Seconds(1));
    waiter.Poke();
    absl::SleepFor(absl::Seconds(1));
    waiter.Post();
  });
  BenchmarkTime start = BenchmarkNow();
  EXPECT_TRUE(
      waiter.Wait(absl::synchronization_internal::KernelTimeout::Never()));
  absl::Duration waited = BenchmarkNow().vtime - start.vtime;
  EXPECT_GE(waited, WithTolerance(absl::Seconds(2)));
}

TYPED_TEST_P(WaiterTest, WaitDurationWoken) {
  absl::synchronization_internal::ThreadPool tp(1);
  TypeParam waiter;
  tp.Schedule([&]() {
    // Include some `Poke()` calls to ensure they don't cause `waiter` to return
    // from `Wait()`.
    waiter.Poke();
    absl::SleepFor(absl::Milliseconds(500));
    waiter.Post();
  });
  BenchmarkTime start = BenchmarkNow();
  EXPECT_TRUE(waiter.Wait(
      absl::synchronization_internal::KernelTimeout(absl::Seconds(10))));
  absl::Duration waited = BenchmarkNow().vtime - start.vtime;
  EXPECT_GE(waited, WithTolerance(absl::Milliseconds(500)));
  EXPECT_LT(waited, absl::Seconds(2));
}

TYPED_TEST_P(WaiterTest, WaitTimeWoken) {
  absl::synchronization_internal::ThreadPool tp(1);
  TypeParam waiter;
  tp.Schedule([&]() {
    // Include some `Poke()` calls to ensure they don't cause `waiter` to return
    // from `Wait()`.
    waiter.Poke();
    absl::SleepFor(absl::Milliseconds(500));
    waiter.Post();
  });
  BenchmarkTime start = BenchmarkNow();
  EXPECT_TRUE(waiter.Wait(absl::synchronization_internal::KernelTimeout(
      start.time + absl::Seconds(10))));
  absl::Duration waited = BenchmarkNow().vtime - start.vtime;
  EXPECT_GE(waited, WithTolerance(absl::Milliseconds(500)));
  EXPECT_LT(waited, absl::Seconds(2));
}

TYPED_TEST_P(WaiterTest, WaitDurationReached) {
  TypeParam waiter;
  BenchmarkTime start = BenchmarkNow();
  EXPECT_FALSE(waiter.Wait(
      absl::synchronization_internal::KernelTimeout(absl::Milliseconds(500))));
  absl::Duration waited = BenchmarkNow().vtime - start.vtime;
  EXPECT_GE(waited, WithTolerance(absl::Milliseconds(500)));
  EXPECT_LT(waited, absl::Seconds(1));
}

TYPED_TEST_P(WaiterTest, WaitTimeReached) {
  TypeParam waiter;
  BenchmarkTime start = BenchmarkNow();
  EXPECT_FALSE(waiter.Wait(absl::synchronization_internal::KernelTimeout(
      start.time + absl::Milliseconds(500))));
  absl::Duration waited = BenchmarkNow().vtime - start.vtime;
  EXPECT_GE(waited, WithTolerance(absl::Milliseconds(500)));
  EXPECT_LT(waited, absl::Seconds(1));
}

REGISTER_TYPED_TEST_SUITE_P(WaiterTest,
                            WaitNoTimeout,
                            WaitDurationWoken,
                            WaitTimeWoken,
                            WaitDurationReached,
                            WaitTimeReached);

#ifdef ABSL_INTERNAL_HAVE_FUTEX_WAITER
INSTANTIATE_TYPED_TEST_SUITE_P(Futex, WaiterTest,
                               absl::synchronization_internal::FutexWaiter);
#endif
#ifdef ABSL_INTERNAL_HAVE_PTHREAD_WAITER
INSTANTIATE_TYPED_TEST_SUITE_P(Pthread, WaiterTest,
                               absl::synchronization_internal::PthreadWaiter);
#endif
#ifdef ABSL_INTERNAL_HAVE_SEM_WAITER
INSTANTIATE_TYPED_TEST_SUITE_P(Sem, WaiterTest,
                               absl::synchronization_internal::SemWaiter);
#endif
#ifdef ABSL_INTERNAL_HAVE_WIN32_WAITER
INSTANTIATE_TYPED_TEST_SUITE_P(Win32, WaiterTest,
                               absl::synchronization_internal::Win32Waiter);
#endif
#ifdef ABSL_INTERNAL_HAVE_STDCPP_WAITER
INSTANTIATE_TYPED_TEST_SUITE_P(Stdcpp, WaiterTest,
                               absl::synchronization_internal::StdcppWaiter);
#endif

}  // namespace
