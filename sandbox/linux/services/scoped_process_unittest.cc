// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/services/scoped_process.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "sandbox/linux/tests/unit_tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

namespace {

void DoExit() { _exit(0); }

void ExitWithCode(int exit_code) { _exit(exit_code); }

void RaiseAndExit(int signal) {
  PCHECK(0 == raise(signal));
  _exit(0);
}

TEST(ScopedProcess, ScopedProcessNormalExit) {
  const int kCustomExitCode = 12;
  ScopedProcess process(base::BindOnce(&ExitWithCode, kCustomExitCode));
  bool got_signaled = true;
  int exit_code = process.WaitForExit(&got_signaled);
  EXPECT_FALSE(got_signaled);
  EXPECT_EQ(kCustomExitCode, exit_code);

  // Verify that WaitForExit() can be called multiple times on the same
  // process.
  bool got_signaled2 = true;
  int exit_code2 = process.WaitForExit(&got_signaled2);
  EXPECT_FALSE(got_signaled2);
  EXPECT_EQ(kCustomExitCode, exit_code2);
}

// Disable this test on Android, SIGABRT is funky there.
TEST(ScopedProcess, DISABLE_ON_ANDROID(ScopedProcessAbort)) {
  PCHECK(SIG_ERR != signal(SIGABRT, SIG_DFL));
  ScopedProcess process(base::BindOnce(&RaiseAndExit, SIGABRT));
  bool got_signaled = false;
  int exit_code = process.WaitForExit(&got_signaled);
  EXPECT_TRUE(got_signaled);
  EXPECT_EQ(SIGABRT, exit_code);
}

TEST(ScopedProcess, ScopedProcessSignaled) {
  ScopedProcess process{base::DoNothing()};
  bool got_signaled = false;
  ASSERT_EQ(0, kill(process.GetPid(), SIGKILL));
  int exit_code = process.WaitForExit(&got_signaled);
  EXPECT_TRUE(got_signaled);
  EXPECT_EQ(SIGKILL, exit_code);
}

TEST(ScopedProcess, DiesForReal) {
  int pipe_fds[2];
  ASSERT_EQ(0, pipe(pipe_fds));
  base::ScopedFD read_end_closer(pipe_fds[0]);
  base::ScopedFD write_end_closer(pipe_fds[1]);

  { ScopedProcess process(base::BindOnce(&DoExit)); }

  // Close writing end of the pipe.
  write_end_closer.reset();
  pipe_fds[1] = -1;

  ASSERT_EQ(0, fcntl(pipe_fds[0], F_SETFL, O_NONBLOCK));
  char c;
  // If the child process is dead for real, there will be no writing end
  // for this pipe left and read will EOF instead of returning EWOULDBLOCK.
  ASSERT_EQ(0, read(pipe_fds[0], &c, 1));
}

TEST(ScopedProcess, SynchronizationBasic) {
  ScopedProcess process1{base::DoNothing()};
  EXPECT_TRUE(process1.WaitForClosureToRun());

  ScopedProcess process2(base::BindOnce(&DoExit));
  // The closure didn't finish running normally. This case is simple enough
  // that process.WaitForClosureToRun() should return false, even though the
  // API does not guarantees that it will return at all.
  EXPECT_FALSE(process2.WaitForClosureToRun());
}

void SleepInMsAndWriteOneByte(int time_to_sleep, int fd) {
  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(time_to_sleep));
  CHECK(1 == write(fd, "1", 1));
}

TEST(ScopedProcess, SynchronizationWorks) {
  int pipe_fds[2];
  ASSERT_EQ(0, pipe(pipe_fds));
  base::ScopedFD read_end_closer(pipe_fds[0]);
  base::ScopedFD write_end_closer(pipe_fds[1]);

  // Start a process with a closure that takes a little bit to run.
  ScopedProcess process(
      base::BindOnce(&SleepInMsAndWriteOneByte, 100, pipe_fds[1]));
  EXPECT_TRUE(process.WaitForClosureToRun());

  // Verify that the closure did, indeed, run.
  ASSERT_EQ(0, fcntl(pipe_fds[0], F_SETFL, O_NONBLOCK));
  char c = 0;
  EXPECT_EQ(1, read(pipe_fds[0], &c, 1));
  EXPECT_EQ('1', c);
}

}  // namespace

}  // namespace sandbox
