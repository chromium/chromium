// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/crash/crashpad_linux.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/command_line.h"
#include "base/files/scoped_file.h"
#include "base/process/process.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "remoting/base/crash/crash_reporting_crashpad.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace remoting {

TEST(CrashpadLinuxTest, InitializeClientWithInvalidFd) {
  EXPECT_FALSE(CrashpadLinux::InitializeClient(base::ScopedFD(), getpid()));
}

TEST(CrashpadLinuxTest, GetHandlerSocketBeforeInit) {
  base::ScopedFD sock;
  pid_t pid = -1;
  EXPECT_FALSE(CrashpadLinux::GetHandlerSocket(sock, pid));
  EXPECT_FALSE(sock.is_valid());
}

MULTIPROCESS_TEST_MAIN(InitializeClientAndGettersChild) {
  int fds[2];
  if (socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fds) != 0) {
    return 1;
  }
  base::ScopedFD client_fd(fds[0]);
  base::ScopedFD server_fd(fds[1]);

  pid_t test_pid = 12345;
  if (!InitializeCrashpadClient(std::move(client_fd), test_pid)) {
    return 2;
  }

  base::ScopedFD retrieved_sock;
  pid_t retrieved_pid = -1;
  if (!GetCrashpadHandlerSocket(retrieved_sock, retrieved_pid) ||
      !retrieved_sock.is_valid() || retrieved_pid != test_pid) {
    return 3;
  }

  return 0;
}

TEST(CrashpadLinuxTest, InitializeClientAndGetters) {
  base::CommandLine command_line(
      base::GetMultiProcessTestChildBaseCommandLine());
  base::Process test_child_process = base::SpawnMultiProcessTestChild(
      "InitializeClientAndGettersChild", command_line, base::LaunchOptions());
  int rv = -1;
  ASSERT_TRUE(base::WaitForMultiprocessTestChildExit(
      test_child_process, TestTimeouts::action_timeout(), &rv));
  EXPECT_EQ(0, rv);
}

}  // namespace remoting
