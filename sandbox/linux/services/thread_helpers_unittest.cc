// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/services/thread_helpers.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/check_op.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/process_metrics.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "sandbox/linux/tests/unit_tests.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::PlatformThread;

namespace sandbox {

namespace {

// These tests fail under ThreadSanitizer, see http://crbug.com/342305
#if !defined(THREAD_SANITIZER)

const int kRaceTestIterations = 1000;

class ScopedProc {
 public:
  ScopedProc() : fd_(-1) {
    fd_ = open("/proc/", O_RDONLY | O_DIRECTORY);
    CHECK_LE(0, fd_);
  }

  ScopedProc(const ScopedProc&) = delete;
  ScopedProc& operator=(const ScopedProc&) = delete;

  ~ScopedProc() { PCHECK(0 == IGNORE_EINTR(close(fd_))); }

  int fd() { return fd_; }

 private:
  int fd_;
};

TEST(ThreadHelpers, IsSingleThreadedBasic) {
  ScopedProc proc_fd;
  ASSERT_TRUE(ThreadHelpers::IsSingleThreaded(proc_fd.fd()));
  ASSERT_TRUE(ThreadHelpers::IsSingleThreaded());

  base::Thread thread("sandbox_tests");
  ASSERT_TRUE(ThreadHelpers::StartThreadAndWatchProcFS(proc_fd.fd(), &thread));
  ASSERT_FALSE(ThreadHelpers::IsSingleThreaded(proc_fd.fd()));
  ASSERT_FALSE(ThreadHelpers::IsSingleThreaded());
  // Explicitly stop the thread here to not pollute the next test.
  ASSERT_TRUE(ThreadHelpers::StopThreadAndWatchProcFS(proc_fd.fd(), &thread));
}

SANDBOX_TEST(ThreadHelpers, AssertSingleThreaded) {
  ScopedProc proc_fd;
  SANDBOX_ASSERT(ThreadHelpers::IsSingleThreaded(proc_fd.fd()));
  SANDBOX_ASSERT(ThreadHelpers::IsSingleThreaded());

  ThreadHelpers::AssertSingleThreaded(proc_fd.fd());
  ThreadHelpers::AssertSingleThreaded();
}

TEST(ThreadHelpers, IsSingleThreadedIterated) {
  ScopedProc proc_fd;
  ASSERT_TRUE(ThreadHelpers::IsSingleThreaded(proc_fd.fd()));

  // Iterate to check for race conditions.
  for (int i = 0; i < kRaceTestIterations; ++i) {
    base::Thread thread("sandbox_tests");
    ASSERT_TRUE(
        ThreadHelpers::StartThreadAndWatchProcFS(proc_fd.fd(), &thread));
    ASSERT_FALSE(ThreadHelpers::IsSingleThreaded(proc_fd.fd()));
    // Explicitly stop the thread here to not pollute the next test.
    ASSERT_TRUE(ThreadHelpers::StopThreadAndWatchProcFS(proc_fd.fd(), &thread));
  }
}

TEST(ThreadHelpers, IsSingleThreadedStartAndStop) {
  ScopedProc proc_fd;
  ASSERT_TRUE(ThreadHelpers::IsSingleThreaded(proc_fd.fd()));

  base::Thread thread("sandbox_tests");
  // This is testing for a race condition, so iterate.
  // Manually, this has been tested with more that 1M iterations.
  for (int i = 0; i < kRaceTestIterations; ++i) {
    ASSERT_TRUE(
        ThreadHelpers::StartThreadAndWatchProcFS(proc_fd.fd(), &thread));
    ASSERT_FALSE(ThreadHelpers::IsSingleThreaded(proc_fd.fd()));

    ASSERT_TRUE(ThreadHelpers::StopThreadAndWatchProcFS(proc_fd.fd(), &thread));
    ASSERT_TRUE(ThreadHelpers::IsSingleThreaded(proc_fd.fd()));
    ASSERT_EQ(1, base::GetNumberOfThreads(base::GetCurrentProcessHandle()));
  }
}

SANDBOX_TEST(ThreadHelpers, AssertSingleThreadedAfterThreadStopped) {
  ScopedProc proc_fd;
  SANDBOX_ASSERT(ThreadHelpers::IsSingleThreaded());

  base::Thread thread1("sandbox_tests");
  base::Thread thread2("sandbox_tests");

  for (int i = 0; i < kRaceTestIterations; ++i) {
    SANDBOX_ASSERT(
        ThreadHelpers::StartThreadAndWatchProcFS(proc_fd.fd(), &thread1));
    SANDBOX_ASSERT(
        ThreadHelpers::StartThreadAndWatchProcFS(proc_fd.fd(), &thread2));
    SANDBOX_ASSERT(!ThreadHelpers::IsSingleThreaded());

    thread1.Stop();
    thread2.Stop();
    // This will wait on /proc/ to reflect the state of threads in the
    // process.
    ThreadHelpers::AssertSingleThreaded();
    SANDBOX_ASSERT(ThreadHelpers::IsSingleThreaded());
  }
}

// Only run this test in Debug mode, where AssertSingleThreaded() will return
// in less than 64ms.
#if !defined(NDEBUG)
SANDBOX_DEATH_TEST(
    ThreadHelpers,
    AssertSingleThreadedDies,
    DEATH_MESSAGE(
        ThreadHelpers::GetAssertSingleThreadedErrorMessageForTests())) {
  base::Thread thread1("sandbox_tests");
  SANDBOX_ASSERT(thread1.Start());
  ThreadHelpers::AssertSingleThreaded();
}
#endif  // !defined(NDEBUG)

#endif  // !defined(THREAD_SANITIZER)

}  // namespace

}  // namespace sandbox
