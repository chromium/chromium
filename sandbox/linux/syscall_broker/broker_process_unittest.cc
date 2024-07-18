// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "sandbox/linux/syscall_broker/broker_process.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stddef.h>
#include <sys/inotify.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/posix/unix_domain_socket.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "sandbox/linux/syscall_broker/broker_client.h"
#include "sandbox/linux/syscall_broker/broker_command.h"
#include "sandbox/linux/tests/scoped_temporary_file.h"
#include "sandbox/linux/tests/test_utils.h"
#include "sandbox/linux/tests/unit_tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {
namespace syscall_broker {

using BrokerType = BrokerProcess::BrokerType;

class BrokerProcessTestHelper {
 public:
  static void CloseChannel(BrokerProcess* broker) { broker->CloseChannel(); }
};

namespace {

// Our fake errno must be less than 255 or various libc implementations will
// not accept this as a valid error number. E.g. bionic accepts up to 255, glibc
// and musl up to 4096.
constexpr int kFakeErrnoSentinel = 254;

bool NoOpCallback(const BrokerSandboxConfig&) {
  return true;
}

}  // namespace

TEST(BrokerProcess, CreateAndDestroy) {
  {
    std::vector<BrokerFilePermission> permissions = {
        BrokerFilePermission::ReadOnly("/proc/cpuinfo")};
    auto policy = std::make_optional<BrokerSandboxConfig>(
        BrokerCommandSet(), permissions, kFakeErrnoSentinel);
    BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED);

    ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));
    ASSERT_TRUE(TestUtils::CurrentProcessHasChildren());
  }
  // Destroy the broker and check it has exited properly.
  ASSERT_FALSE(TestUtils::CurrentProcessHasChildren());
}

TEST(BrokerProcess, TestOpenAccessNull) {
  BrokerCommandSet command_set =
      MakeBrokerCommandSet({COMMAND_ACCESS, COMMAND_OPEN});

  std::vector<BrokerFilePermission> empty;
  auto policy = std::make_optional<BrokerSandboxConfig>(command_set, empty,
                                                        kFakeErrnoSentinel);
  BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED);

  ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));

  int fd = open_broker.GetBrokerClientSignalBased()->Open(nullptr, O_RDONLY);
  ASSERT_EQ(fd, -EFAULT);

  int ret = open_broker.GetBrokerClientSignalBased()->Access(nullptr, F_OK);
  ASSERT_EQ(ret, -EFAULT);
}

void TestOpenFilePerms(bool fast_check_in_client, int denied_errno) {
  const char kR_AllowListed[] = "/proc/DOESNOTEXIST1";
  // We can't debug the init process, and shouldn't be able to access
  // its auxv file.
  const char kR_AllowListedButDenied[] = "/proc/1/auxv";
  const char kW_AllowListed[] = "/proc/DOESNOTEXIST2";
  const char kRW_AllowListed[] = "/proc/DOESNOTEXIST3";
  const char k_NotAllowListed[] = "/proc/DOESNOTEXIST4";

  BrokerCommandSet command_set =
      MakeBrokerCommandSet({COMMAND_ACCESS, COMMAND_OPEN});

  std::vector<BrokerFilePermission> permissions = {
      BrokerFilePermission::ReadOnly(kR_AllowListed),
      BrokerFilePermission::ReadOnly(kR_AllowListedButDenied),
      BrokerFilePermission::WriteOnly(kW_AllowListed),
      BrokerFilePermission::ReadWrite(kRW_AllowListed)};
  auto policy = std::make_optional<BrokerSandboxConfig>(
      command_set, permissions, denied_errno);
  BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                            fast_check_in_client);

  ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));

  int fd = -1;
  fd = open_broker.GetBrokerClientSignalBased()->Open(kR_AllowListed, O_RDONLY);
  ASSERT_EQ(fd, -ENOENT);
  fd = open_broker.GetBrokerClientSignalBased()->Open(kR_AllowListed, O_WRONLY);
  ASSERT_EQ(fd, -denied_errno);
  fd = open_broker.GetBrokerClientSignalBased()->Open(kR_AllowListed, O_RDWR);
  ASSERT_EQ(fd, -denied_errno);
  int ret = -1;
  ret = open_broker.GetBrokerClientSignalBased()->Access(kR_AllowListed, F_OK);
  ASSERT_EQ(ret, -ENOENT);
  ret = open_broker.GetBrokerClientSignalBased()->Access(kR_AllowListed, R_OK);
  ASSERT_EQ(ret, -ENOENT);
  ret = open_broker.GetBrokerClientSignalBased()->Access(kR_AllowListed, W_OK);
  ASSERT_EQ(ret, -denied_errno);
  ret = open_broker.GetBrokerClientSignalBased()->Access(kR_AllowListed,
                                                         R_OK | W_OK);
  ASSERT_EQ(ret, -denied_errno);
  ret = open_broker.GetBrokerClientSignalBased()->Access(kR_AllowListed, X_OK);
  ASSERT_EQ(ret, -denied_errno);
  ret = open_broker.GetBrokerClientSignalBased()->Access(kR_AllowListed,
                                                         R_OK | X_OK);
  ASSERT_EQ(ret, -denied_errno);

  // Android sometimes runs tests as root.
  // This part of the test requires a process that doesn't have
  // CAP_DAC_OVERRIDE. We check against a root euid as a proxy for that.
  if (geteuid()) {
    fd = open_broker.GetBrokerClientSignalBased()->Open(kR_AllowListedButDenied,
                                                        O_RDONLY);
    // The broker process will allow this, but the normal permission system
    // won't.
    ASSERT_EQ(fd, -EACCES);
    fd = open_broker.GetBrokerClientSignalBased()->Open(kR_AllowListedButDenied,
                                                        O_WRONLY);
    ASSERT_EQ(fd, -denied_errno);
    fd = open_broker.GetBrokerClientSignalBased()->Open(kR_AllowListedButDenied,
                                                        O_RDWR);
    ASSERT_EQ(fd, -denied_errno);
    ret = open_broker.GetBrokerClientSignalBased()->Access(
        kR_AllowListedButDenied, F_OK);
    // The normal permission system will let us check that the file exists.
    ASSERT_EQ(ret, 0);
    ret = open_broker.GetBrokerClientSignalBased()->Access(
        kR_AllowListedButDenied, R_OK);
    ASSERT_EQ(ret, -EACCES);
    ret = open_broker.GetBrokerClientSignalBased()->Access(
        kR_AllowListedButDenied, W_OK);
    ASSERT_EQ(ret, -denied_errno);
    ret = open_broker.GetBrokerClientSignalBased()->Access(
        kR_AllowListedButDenied, R_OK | W_OK);
    ASSERT_EQ(ret, -denied_errno);
    ret = open_broker.GetBrokerClientSignalBased()->Access(
        kR_AllowListedButDenied, X_OK);
    ASSERT_EQ(ret, -denied_errno);
    ret = open_broker.GetBrokerClientSignalBased()->Access(
        kR_AllowListedButDenied, R_OK | X_OK);
    ASSERT_EQ(ret, -denied_errno);
  }

  fd = open_broker.GetBrokerClientSignalBased()->Open(kW_AllowListed, O_RDONLY);
  ASSERT_EQ(fd, -denied_errno);
  fd = open_broker.GetBrokerClientSignalBased()->Open(kW_AllowListed, O_WRONLY);
  ASSERT_EQ(fd, -ENOENT);
  fd = open_broker.GetBrokerClientSignalBased()->Open(kW_AllowListed, O_RDWR);
  ASSERT_EQ(fd, -denied_errno);
  ret = open_broker.GetBrokerClientSignalBased()->Access(kW_AllowListed, F_OK);
  ASSERT_EQ(ret, -ENOENT);
  ret = open_broker.GetBrokerClientSignalBased()->Access(kW_AllowListed, R_OK);
  ASSERT_EQ(ret, -denied_errno);
  ret = open_broker.GetBrokerClientSignalBased()->Access(kW_AllowListed, W_OK);
  ASSERT_EQ(ret, -ENOENT);
  ret = open_broker.GetBrokerClientSignalBased()->Access(kW_AllowListed,
                                                         R_OK | W_OK);
  ASSERT_EQ(ret, -denied_errno);
  ret = open_broker.GetBrokerClientSignalBased()->Access(kW_AllowListed, X_OK);
  ASSERT_EQ(ret, -denied_errno);
  ret = open_broker.GetBrokerClientSignalBased()->Access(kW_AllowListed,
                                                         R_OK | X_OK);
  ASSERT_EQ(ret, -denied_errno);

  fd =
      open_broker.GetBrokerClientSignalBased()->Open(kRW_AllowListed, O_RDONLY);
  ASSERT_EQ(fd, -ENOENT);
  fd =
      open_broker.GetBrokerClientSignalBased()->Open(kRW_AllowListed, O_WRONLY);
  ASSERT_EQ(fd, -ENOENT);
  fd = open_broker.GetBrokerClientSignalBased()->Open(kRW_AllowListed, O_RDWR);
  ASSERT_EQ(fd, -ENOENT);
  ret = open_broker.GetBrokerClientSignalBased()->Access(kRW_AllowListed, F_OK);
  ASSERT_EQ(ret, -ENOENT);
  ret = open_broker.GetBrokerClientSignalBased()->Access(kRW_AllowListed, R_OK);
  ASSERT_EQ(ret, -ENOENT);
  ret = open_broker.GetBrokerClientSignalBased()->Access(kRW_AllowListed, W_OK);
  ASSERT_EQ(ret, -ENOENT);
  ret = open_broker.GetBrokerClientSignalBased()->Access(kRW_AllowListed,
                                                         R_OK | W_OK);
  ASSERT_EQ(ret, -ENOENT);
  ret = open_broker.GetBrokerClientSignalBased()->Access(kRW_AllowListed, X_OK);
  ASSERT_EQ(ret, -denied_errno);
  ret = open_broker.GetBrokerClientSignalBased()->Access(kRW_AllowListed,
                                                         R_OK | X_OK);
  ASSERT_EQ(ret, -denied_errno);

  fd = open_broker.GetBrokerClientSignalBased()->Open(k_NotAllowListed,
                                                      O_RDONLY);
  ASSERT_EQ(fd, -denied_errno);
  fd = open_broker.GetBrokerClientSignalBased()->Open(k_NotAllowListed,
                                                      O_WRONLY);
  ASSERT_EQ(fd, -denied_errno);
  fd = open_broker.GetBrokerClientSignalBased()->Open(k_NotAllowListed, O_RDWR);
  ASSERT_EQ(fd, -denied_errno);
  ret =
      open_broker.GetBrokerClientSignalBased()->Access(k_NotAllowListed, F_OK);
  ASSERT_EQ(ret, -denied_errno);
  ret =
      open_broker.GetBrokerClientSignalBased()->Access(k_NotAllowListed, R_OK);
  ASSERT_EQ(ret, -denied_errno);
  ret =
      open_broker.GetBrokerClientSignalBased()->Access(k_NotAllowListed, W_OK);
  ASSERT_EQ(ret, -denied_errno);
  ret = open_broker.GetBrokerClientSignalBased()->Access(k_NotAllowListed,
                                                         R_OK | W_OK);
  ASSERT_EQ(ret, -denied_errno);
  ret =
      open_broker.GetBrokerClientSignalBased()->Access(k_NotAllowListed, X_OK);
  ASSERT_EQ(ret, -denied_errno);
  ret = open_broker.GetBrokerClientSignalBased()->Access(k_NotAllowListed,
                                                         R_OK | X_OK);
  ASSERT_EQ(ret, -denied_errno);

  // We have some extra sanity check for clearly wrong values.
  fd = open_broker.GetBrokerClientSignalBased()->Open(
      kRW_AllowListed, O_RDONLY | O_WRONLY | O_RDWR);
  ASSERT_EQ(fd, -denied_errno);

  // It makes no sense to allow O_CREAT in a 2-parameters open. Ensure this
  // is denied.
  fd = open_broker.GetBrokerClientSignalBased()->Open(kRW_AllowListed,
                                                      O_RDWR | O_CREAT);
  ASSERT_EQ(fd, -denied_errno);
}

// Run the same thing twice. The second time, we make sure that no security
// check is performed on the client.
TEST(BrokerProcess, OpenFilePermsWithClientCheck) {
  TestOpenFilePerms(true /* fast_check_in_client */, EPERM);
  // Don't do anything here, so that ASSERT works in the subfunction as
  // expected.
}

TEST(BrokerProcess, OpenOpenFilePermsNoClientCheck) {
  TestOpenFilePerms(false /* fast_check_in_client */, EPERM);
  // Don't do anything here, so that ASSERT works in the subfunction as
  // expected.
}

// Run the same twice again, but with ENOENT instead of EPERM.
TEST(BrokerProcess, OpenFilePermsWithClientCheckNoEnt) {
  TestOpenFilePerms(true /* fast_check_in_client */, ENOENT);
  // Don't do anything here, so that ASSERT works in the subfunction as
  // expected.
}

TEST(BrokerProcess, OpenOpenFilePermsNoClientCheckNoEnt) {
  TestOpenFilePerms(false /* fast_check_in_client */, ENOENT);
  // Don't do anything here, so that ASSERT works in the subfunction as
  // expected.
}

void TestBadPaths(bool fast_check_in_client) {
  const char kFileCpuInfo[] = "/proc/cpuinfo";
  const char kNotAbsPath[] = "proc/cpuinfo";
  const char kDotDotStart[] = "/../proc/cpuinfo";
  const char kDotDotMiddle[] = "/proc/self/../cpuinfo";
  const char kDotDotEnd[] = "/proc/..";
  const char kTrailingSlash[] = "/proc/";

  BrokerCommandSet command_set =
      MakeBrokerCommandSet({COMMAND_ACCESS, COMMAND_OPEN});

  std::vector<BrokerFilePermission> permissions = {
      BrokerFilePermission::ReadOnlyRecursive("/proc/")};
  auto policy = std::make_optional<BrokerSandboxConfig>(
      command_set, permissions, kFakeErrnoSentinel);
  BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                            fast_check_in_client);

  ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));

  // Open cpuinfo via the broker.
  int cpuinfo_fd =
      open_broker.GetBrokerClientSignalBased()->Open(kFileCpuInfo, O_RDONLY);
  base::ScopedFD cpuinfo_fd_closer(cpuinfo_fd);
  ASSERT_GE(cpuinfo_fd, 0);

  int fd = -1;
  int can_access;

  can_access =
      open_broker.GetBrokerClientSignalBased()->Access(kNotAbsPath, R_OK);
  ASSERT_EQ(can_access, -kFakeErrnoSentinel);
  fd = open_broker.GetBrokerClientSignalBased()->Open(kNotAbsPath, O_RDONLY);
  ASSERT_EQ(fd, -kFakeErrnoSentinel);

  can_access =
      open_broker.GetBrokerClientSignalBased()->Access(kDotDotStart, R_OK);
  ASSERT_EQ(can_access, -kFakeErrnoSentinel);
  fd = open_broker.GetBrokerClientSignalBased()->Open(kDotDotStart, O_RDONLY);
  ASSERT_EQ(fd, -kFakeErrnoSentinel);

  can_access =
      open_broker.GetBrokerClientSignalBased()->Access(kDotDotMiddle, R_OK);
  ASSERT_EQ(can_access, -kFakeErrnoSentinel);
  fd = open_broker.GetBrokerClientSignalBased()->Open(kDotDotMiddle, O_RDONLY);
  ASSERT_EQ(fd, -kFakeErrnoSentinel);

  can_access =
      open_broker.GetBrokerClientSignalBased()->Access(kDotDotEnd, R_OK);
  ASSERT_EQ(can_access, -kFakeErrnoSentinel);
  fd = open_broker.GetBrokerClientSignalBased()->Open(kDotDotEnd, O_RDONLY);
  ASSERT_EQ(fd, -kFakeErrnoSentinel);

  can_access =
      open_broker.GetBrokerClientSignalBased()->Access(kTrailingSlash, R_OK);
  ASSERT_EQ(can_access, -kFakeErrnoSentinel);
  fd = open_broker.GetBrokerClientSignalBased()->Open(kTrailingSlash, O_RDONLY);
  ASSERT_EQ(fd, -kFakeErrnoSentinel);
}

TEST(BrokerProcess, BadPathsClientCheck) {
  TestBadPaths(true /* fast_check_in_client */);
  // Don't do anything here, so that ASSERT works in the subfunction as
  // expected.
}

TEST(BrokerProcess, BadPathsNoClientCheck) {
  TestBadPaths(false /* fast_check_in_client */);
  // Don't do anything here, so that ASSERT works in the subfunction as
  // expected.
}

void TestOpenCpuinfo(bool fast_check_in_client, bool recursive) {
  const char kFileCpuInfo[] = "/proc/cpuinfo";
  const char kDirProc[] = "/proc/";

  {
    BrokerCommandSet command_set =
        MakeBrokerCommandSet({COMMAND_ACCESS, COMMAND_OPEN});

    std::vector<BrokerFilePermission> permissions;
    permissions.push_back(
        recursive ? BrokerFilePermission::ReadOnlyRecursive(kDirProc)
                  : BrokerFilePermission::ReadOnly(kFileCpuInfo));

    auto policy = std::make_optional<BrokerSandboxConfig>(
        command_set, permissions, kFakeErrnoSentinel);
    BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                              fast_check_in_client);

    ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));

    int fd =
        open_broker.GetBrokerClientSignalBased()->Open(kFileCpuInfo, O_RDWR);
    ASSERT_EQ(fd, -kFakeErrnoSentinel);

    // Check we can read /proc/cpuinfo.
    int can_access =
        open_broker.GetBrokerClientSignalBased()->Access(kFileCpuInfo, R_OK);
    EXPECT_EQ(can_access, 0);
    can_access =
        open_broker.GetBrokerClientSignalBased()->Access(kFileCpuInfo, W_OK);
    EXPECT_EQ(can_access, -kFakeErrnoSentinel);
    // Check we can not write /proc/cpuinfo.

    // Open cpuinfo via the broker.
    int cpuinfo_fd =
        open_broker.GetBrokerClientSignalBased()->Open(kFileCpuInfo, O_RDONLY);
    base::ScopedFD cpuinfo_fd_closer(cpuinfo_fd);
    EXPECT_GE(cpuinfo_fd, 0);
    char buf[3];
    memset(buf, 0, sizeof(buf));
    int read_len1 = read(cpuinfo_fd, buf, sizeof(buf));
    EXPECT_GT(read_len1, 0);

    // Open cpuinfo directly.
    int cpuinfo_fd2 = open(kFileCpuInfo, O_RDONLY);
    base::ScopedFD cpuinfo_fd2_closer(cpuinfo_fd2);
    EXPECT_GE(cpuinfo_fd2, 0);
    char buf2[3];
    memset(buf2, 1, sizeof(buf2));
    int read_len2 = read(cpuinfo_fd2, buf2, sizeof(buf2));
    EXPECT_GT(read_len1, 0);

    // The following is not guaranteed true, but will be in practice.
    EXPECT_EQ(read_len1, read_len2);
    // Compare the cpuinfo as returned by the broker with the one we opened
    // ourselves.
    EXPECT_EQ(memcmp(buf, buf2, read_len1), 0);

    ASSERT_TRUE(TestUtils::CurrentProcessHasChildren());
  }

  ASSERT_FALSE(TestUtils::CurrentProcessHasChildren());
}

// Run this test 4 times. With and without the check in client
// and using a recursive path.
TEST(BrokerProcess, OpenCpuinfoWithClientCheck) {
  TestOpenCpuinfo(true /* fast_check_in_client */, false /* not recursive */);
  // Don't do anything here, so that ASSERT works in the subfunction as
  // expected.
}

TEST(BrokerProcess, OpenCpuinfoNoClientCheck) {
  TestOpenCpuinfo(false /* fast_check_in_client */, false /* not recursive */);
  // Don't do anything here, so that ASSERT works in the subfunction as
  // expected.
}

TEST(BrokerProcess, OpenCpuinfoWithClientCheckRecursive) {
  TestOpenCpuinfo(true /* fast_check_in_client */, true /* recursive */);
  // Don't do anything here, so that ASSERT works in the subfunction as
  // expected.
}

TEST(BrokerProcess, OpenCpuinfoNoClientCheckRecursive) {
  TestOpenCpuinfo(false /* fast_check_in_client */, true /* recursive */);
  // Don't do anything here, so that ASSERT works in the subfunction as
  // expected.
}

TEST(BrokerProcess, OpenFileRW) {
  ScopedTemporaryFile tempfile;
  const char* tempfile_name = tempfile.full_file_name();

  BrokerCommandSet command_set =
      MakeBrokerCommandSet({COMMAND_ACCESS, COMMAND_OPEN});

  std::vector<BrokerFilePermission> permissions = {
      BrokerFilePermission::ReadWrite(tempfile_name)};
  auto policy = std::make_optional<BrokerSandboxConfig>(
      command_set, permissions, kFakeErrnoSentinel);
  BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED);

  ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));

  // Check we can access that file with read or write.
  int can_access = open_broker.GetBrokerClientSignalBased()->Access(
      tempfile_name, R_OK | W_OK);
  ASSERT_EQ(can_access, 0);

  int tempfile2 = -1;
  tempfile2 =
      open_broker.GetBrokerClientSignalBased()->Open(tempfile_name, O_RDWR);
  ASSERT_GE(tempfile2, 0);

  // Write to the descriptor opened by the broker.
  char test_text[] = "TESTTESTTEST";
  ssize_t len = write(tempfile2, test_text, sizeof(test_text));
  ASSERT_EQ(len, static_cast<ssize_t>(sizeof(test_text)));

  // Read back from the original file descriptor what we wrote through
  // the descriptor provided by the broker.
  char buf[1024];
  len = read(tempfile.fd(), buf, sizeof(buf));

  ASSERT_EQ(len, static_cast<ssize_t>(sizeof(test_text)));
  ASSERT_EQ(memcmp(test_text, buf, sizeof(test_text)), 0);

  ASSERT_EQ(close(tempfile2), 0);
}

// SANDBOX_TEST because the process could die with a SIGPIPE
// and we want this to happen in a subprocess.
SANDBOX_TEST(BrokerProcess, BrokerDied) {
  const char kCpuInfo[] = "/proc/cpuinfo";

  BrokerCommandSet command_set =
      MakeBrokerCommandSet({COMMAND_ACCESS, COMMAND_OPEN});

  std::vector<BrokerFilePermission> permissions = {
      BrokerFilePermission::ReadOnly(kCpuInfo)};
  auto policy = std::make_optional<BrokerSandboxConfig>(
      command_set, permissions, kFakeErrnoSentinel);
  BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                            true /* fast_check_in_client */,
                            true /* quiet_failures_for_tests */);

  SANDBOX_ASSERT(open_broker.Fork(base::BindOnce(&NoOpCallback)));
  const pid_t broker_pid = open_broker.broker_pid();
  SANDBOX_ASSERT(kill(broker_pid, SIGKILL) == 0);

  // Now we check that the broker has been signaled, but do not reap it.
  siginfo_t process_info;
  SANDBOX_ASSERT(HANDLE_EINTR(waitid(P_PID, broker_pid, &process_info,
                                     WEXITED | WNOWAIT)) == 0);
  SANDBOX_ASSERT(broker_pid == process_info.si_pid);
  SANDBOX_ASSERT(CLD_KILLED == process_info.si_code);
  SANDBOX_ASSERT(SIGKILL == process_info.si_status);

  // Check that doing Open with a dead broker won't SIGPIPE us.
  SANDBOX_ASSERT(open_broker.GetBrokerClientSignalBased()->Open(
                     kCpuInfo, O_RDONLY) == -ENOMEM);
  SANDBOX_ASSERT(open_broker.GetBrokerClientSignalBased()->Access(
                     kCpuInfo, O_RDONLY) == -ENOMEM);
}

void TestOpenComplexFlags(bool fast_check_in_client) {
  const char kCpuInfo[] = "/proc/cpuinfo";

  BrokerCommandSet command_set =
      MakeBrokerCommandSet({COMMAND_ACCESS, COMMAND_OPEN});

  std::vector<BrokerFilePermission> permissions = {
      BrokerFilePermission::ReadOnly(kCpuInfo)};
  auto policy = std::make_optional<BrokerSandboxConfig>(
      command_set, permissions, kFakeErrnoSentinel);
  BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                            fast_check_in_client);

  ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));

  // Test that we do the right thing for O_CLOEXEC and O_NONBLOCK.
  int fd = -1;
  int ret = 0;
  fd = open_broker.GetBrokerClientSignalBased()->Open(kCpuInfo, O_RDONLY);
  ASSERT_GE(fd, 0);
  ret = fcntl(fd, F_GETFL);
  ASSERT_NE(-1, ret) << errno;
  // The description shouldn't have the O_CLOEXEC attribute, nor O_NONBLOCK.
  ASSERT_EQ(0, ret & (O_CLOEXEC | O_NONBLOCK));
  ret = fcntl(fd, F_GETFD);
  ASSERT_NE(-1, ret) << errno;
  // The descriptor also should not have FD_CLOEXEC.
  ASSERT_EQ(FD_CLOEXEC & ret, 0);
  ASSERT_EQ(0, close(fd));

  fd = open_broker.GetBrokerClientSignalBased()->Open(kCpuInfo,
                                                      O_RDONLY | O_CLOEXEC);
  ASSERT_GE(fd, 0);
  ret = fcntl(fd, F_GETFD);
  ASSERT_NE(-1, ret);
  // Important: use F_GETFD, not F_GETFL. The O_CLOEXEC flag in F_GETFL
  // is actually not used by the kernel.
  ASSERT_TRUE(FD_CLOEXEC & ret);
  ASSERT_EQ(0, close(fd));

  fd = open_broker.GetBrokerClientSignalBased()->Open(kCpuInfo,
                                                      O_RDONLY | O_NONBLOCK);
  ASSERT_GE(fd, 0);
  ret = fcntl(fd, F_GETFL);
  ASSERT_NE(-1, ret);
  ASSERT_TRUE(O_NONBLOCK & ret);
  ASSERT_EQ(0, close(fd));
}

TEST(BrokerProcess, OpenComplexFlagsWithClientCheck) {
  TestOpenComplexFlags(true /* fast_check_in_client */);
  // Don't do anything here, so that ASSERT works in the subfunction as
  // expected.
}

TEST(BrokerProcess, OpenComplexFlagsNoClientCheck) {
  TestOpenComplexFlags(false /* fast_check_in_client */);
  // Don't do anything here, so that ASSERT works in the subfunction as
  // expected.
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// Flaky on Linux NG bots: https://crbug.com/595199.
#define MAYBE_RecvMsgDescriptorLeak DISABLED_RecvMsgDescriptorLeak
#else
#define MAYBE_RecvMsgDescriptorLeak RecvMsgDescriptorLeak
#endif

// We need to allow noise because the broker will log when it receives our
// bogus IPCs.
SANDBOX_TEST_ALLOW_NOISE(BrokerProcess, MAYBE_RecvMsgDescriptorLeak) {
  // Android creates a socket on first use of the LOG call.
  // We need to ensure this socket is open before we
  // begin the test.
  LOG(INFO) << "Ensure Android LOG socket is allocated";

  // Find the four lowest available file descriptors.
  int available_fds[4];
  SANDBOX_ASSERT(0 == pipe(available_fds));
  SANDBOX_ASSERT(0 == pipe(available_fds + 2));

  // Save one FD to send to the broker later, and close the others.
  base::ScopedFD message_fd(available_fds[0]);
  for (size_t i = 1; i < std::size(available_fds); i++) {
    SANDBOX_ASSERT(0 == IGNORE_EINTR(close(available_fds[i])));
  }

  // Lower our file descriptor limit to just allow three more file descriptors
  // to be allocated.  (N.B., RLIMIT_NOFILE doesn't limit the number of file
  // descriptors a process can have: it only limits the highest value that can
  // be assigned to newly-created descriptors allocated by the process.)
  const rlim_t fd_limit =
      1 + *std::max_element(available_fds,
                            available_fds + std::size(available_fds));

  struct rlimit rlim;
  SANDBOX_ASSERT(0 == getrlimit(RLIMIT_NOFILE, &rlim));
  SANDBOX_ASSERT(fd_limit <= rlim.rlim_cur);
  rlim.rlim_cur = fd_limit;
  SANDBOX_ASSERT(0 == setrlimit(RLIMIT_NOFILE, &rlim));

  static const char kCpuInfo[] = "/proc/cpuinfo";

  BrokerCommandSet command_set =
      MakeBrokerCommandSet({COMMAND_ACCESS, COMMAND_OPEN});

  std::vector<BrokerFilePermission> permissions = {
      BrokerFilePermission::ReadOnly(kCpuInfo)};
  auto policy = std::make_optional<BrokerSandboxConfig>(
      command_set, permissions, kFakeErrnoSentinel);
  BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED);

  SANDBOX_ASSERT(open_broker.Fork(base::BindOnce(&NoOpCallback)));

  const int ipc_fd =
      open_broker.GetBrokerClientSignalBased()->GetIPCDescriptorForTesting();
  SANDBOX_ASSERT(ipc_fd >= 0);

  static const char kBogus[] = "not a pickle";
  std::vector<int> fds;
  fds.push_back(message_fd.get());

  // The broker process should only have a couple spare file descriptors
  // available, but for good measure we send it fd_limit bogus IPCs anyway.
  for (rlim_t i = 0; i < fd_limit; ++i) {
    SANDBOX_ASSERT(
        base::UnixDomainSocket::SendMsg(ipc_fd, kBogus, sizeof(kBogus), fds));
  }

  const int fd =
      open_broker.GetBrokerClientSignalBased()->Open(kCpuInfo, O_RDONLY);
  SANDBOX_ASSERT(fd >= 0);
  SANDBOX_ASSERT(0 == IGNORE_EINTR(close(fd)));
}

bool CloseFD(int fd, const syscall_broker::BrokerSandboxConfig&) {
  PCHECK(0 == IGNORE_EINTR(close(fd)));
  return true;
}

// Return true if the other end of the |reader| pipe was closed,
// false if |timeout_in_seconds| was reached or another event
// or error occured.
bool WaitForClosedPipeWriter(int reader, int timeout_in_ms) {
  struct pollfd poll_fd = {reader, POLLIN | POLLRDHUP, 0};
  const int num_events = HANDLE_EINTR(poll(&poll_fd, 1, timeout_in_ms));
  if (1 == num_events && poll_fd.revents | POLLHUP)
    return true;
  return false;
}

// Closing the broker client's IPC channel should terminate the broker
// process.
TEST(BrokerProcess, BrokerDiesOnClosedChannel) {
  // Get the writing end of a pipe into the broker (child) process so
  // that we can reliably detect when it dies.
  int lifeline_fds[2];
  PCHECK(0 == pipe(lifeline_fds));

  BrokerCommandSet command_set =
      MakeBrokerCommandSet({COMMAND_ACCESS, COMMAND_OPEN});

  std::vector<BrokerFilePermission> permissions = {
      BrokerFilePermission::ReadOnly("/proc/cpuinfo")};
  auto policy = std::make_optional<BrokerSandboxConfig>(
      command_set, permissions, kFakeErrnoSentinel);
  BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                            true /* fast_check_in_client */,
                            false /* quiet_failures_for_tests */);

  ASSERT_TRUE(open_broker.Fork(base::BindOnce(&CloseFD, lifeline_fds[0])));

  // Make sure the writing end only exists in the broker process.
  close(lifeline_fds[1]);
  base::ScopedFD reader(lifeline_fds[0]);

  const pid_t broker_pid = open_broker.broker_pid();

  // This should cause the broker process to exit.
  BrokerProcessTestHelper::CloseChannel(&open_broker);

  const int kTimeoutInMilliseconds = 5000;
  const bool broker_lifeline_closed =
      WaitForClosedPipeWriter(reader.get(), kTimeoutInMilliseconds);
  // If the broker exited, its lifeline fd should be closed.
  ASSERT_TRUE(broker_lifeline_closed);
  // Now check that the broker has exited, but do not reap it.
  siginfo_t process_info;
  ASSERT_EQ(0, HANDLE_EINTR(waitid(P_PID, broker_pid, &process_info,
                                   WEXITED | WNOWAIT)));
  EXPECT_EQ(broker_pid, process_info.si_pid);
  EXPECT_EQ(CLD_EXITED, process_info.si_code);
  EXPECT_EQ(1, process_info.si_status);
}

void TestRewriteProcSelfHelper(bool fast_check_in_client) {
  std::vector<BrokerFilePermission> proc_status_permissions = {
      BrokerFilePermission::ReadOnly("/proc/self/status")};

  BrokerCommandSet command_set = MakeBrokerCommandSet({COMMAND_OPEN});

  {
    // Nonexistent file with no permissions to see file.
    auto policy = std::make_optional<BrokerSandboxConfig>(
        command_set, proc_status_permissions, kFakeErrnoSentinel);
    BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                              fast_check_in_client);

    ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));
    int fd = open_broker.GetBrokerClientSignalBased()->Open(
        "/proc/self/status", O_RDONLY | O_NONBLOCK);
    // This should open /proc/[pid]/status even though it wasn't the exact
    // string that Open() requested.
    ASSERT_GE(fd, 0);

    // Reading /proc/self/status should return the same PID as the current
    // process's PID, not the broker's.
    char buf[4096];

    ssize_t num_read = HANDLE_EINTR(read(fd, buf, sizeof(buf)));
    ASSERT_GE(IGNORE_EINTR(close(fd)), 0);

    ASSERT_GT(num_read, 0);

    std::string_view status(buf, static_cast<size_t>(num_read));
    std::string_view tracer("Pid:\t");

    std::string_view::size_type pid_index = status.find(tracer);
    ASSERT_NE(pid_index, std::string_view::npos);
    pid_index += tracer.size();
    std::string_view::size_type pid_end_index = status.find('\n', pid_index);
    ASSERT_NE(pid_end_index, std::string_view::npos);

    std::string_view pid_str(buf + pid_index, pid_end_index - pid_index);
    int pid = 0;
    ASSERT_TRUE(base::StringToInt(pid_str, &pid));

    ASSERT_EQ(pid, getpid());

    // TODO(mpdenton): maybe also test in a different pid namespace?
  }
}

TEST(BrokerProcess, RewriteProcSelfClient) {
  TestRewriteProcSelfHelper(true);
}

TEST(BrokerProcess, RewriteProcSelfHost) {
  TestRewriteProcSelfHelper(false);
}

TEST(BrokerProcess, CreateFile) {
  // Create two temporary files, grab their file names, and then delete the
  // files themselves.
  std::string temp_str;
  std::string perm_str;
  {
    ScopedTemporaryFile temp_file;
    ScopedTemporaryFile perm_file;
    temp_str = temp_file.full_file_name();
    perm_str = perm_file.full_file_name();
  }
  const char* tempfile_name = temp_str.c_str();
  const char* permfile_name = perm_str.c_str();

  BrokerCommandSet command_set =
      MakeBrokerCommandSet({COMMAND_ACCESS, COMMAND_OPEN});

  std::vector<BrokerFilePermission> permissions = {
      BrokerFilePermission::ReadWriteCreateTemporary(tempfile_name),
      BrokerFilePermission::ReadWriteCreate(permfile_name),
  };

  auto policy = std::make_optional<BrokerSandboxConfig>(
      command_set, permissions, kFakeErrnoSentinel);
  BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED);

  ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));

  int fd = -1;

  // Opening a temp file using O_CREAT but not O_EXCL must not be allowed
  // by the broker so as to prevent spying on any pre-existing files.
  fd = open_broker.GetBrokerClientSignalBased()->Open(tempfile_name,
                                                      O_RDWR | O_CREAT);
  ASSERT_EQ(fd, -kFakeErrnoSentinel);

  // Opening a temp file in a normal way must not be allowed by the broker,
  // either.
  fd = open_broker.GetBrokerClientSignalBased()->Open(tempfile_name, O_RDWR);
  ASSERT_EQ(fd, -kFakeErrnoSentinel);

  // Opening a temp file with both O_CREAT and O_EXCL is allowed since the
  // file is known not to exist outside the scope of ScopedTemporaryFile.
  fd = open_broker.GetBrokerClientSignalBased()->Open(
      tempfile_name, O_RDWR | O_CREAT | O_EXCL);
  ASSERT_GE(fd, 0);
  close(fd);

  // Manually create a conflict for the temp filename.
  fd = open(tempfile_name, O_RDWR | O_CREAT, 0600);
  ASSERT_GE(fd, 0);
  close(fd);

  // Opening a temp file with both O_CREAT and O_EXCL is allowed but fails
  // per the OS when there is a conflict with a pre-existing file.
  fd = open_broker.GetBrokerClientSignalBased()->Open(
      tempfile_name, O_RDWR | O_CREAT | O_EXCL);
  ASSERT_EQ(fd, -EEXIST);

  // Opening a new permanent file without specifying O_EXCL is allowed.
  fd = open_broker.GetBrokerClientSignalBased()->Open(permfile_name,
                                                      O_RDWR | O_CREAT);
  ASSERT_GE(fd, 0);
  close(fd);

  // Opening an existing permanent file without specifying O_EXCL is allowed.
  fd = open_broker.GetBrokerClientSignalBased()->Open(permfile_name,
                                                      O_RDWR | O_CREAT);
  ASSERT_GE(fd, 0);
  close(fd);

  // Opening an existing file with O_EXCL is allowed but fails per the OS.
  fd = open_broker.GetBrokerClientSignalBased()->Open(
      permfile_name, O_RDWR | O_CREAT | O_EXCL);
  ASSERT_EQ(fd, -EEXIST);

  const char kTestText[] = "TESTTESTTEST";

  fd = open_broker.GetBrokerClientSignalBased()->Open(permfile_name, O_RDWR);
  ASSERT_GE(fd, 0);
  {
    // Write to the descriptor opened by the broker and close.
    base::ScopedFD scoped_fd(fd);
    ssize_t len = HANDLE_EINTR(write(fd, kTestText, sizeof(kTestText)));
    ASSERT_EQ(len, static_cast<ssize_t>(sizeof(kTestText)));
  }

  int fd_check = open(permfile_name, O_RDONLY);
  ASSERT_GE(fd_check, 0);
  {
    base::ScopedFD scoped_fd(fd_check);
    char buf[1024];
    ssize_t len = HANDLE_EINTR(read(fd_check, buf, sizeof(buf)));
    ASSERT_EQ(len, static_cast<ssize_t>(sizeof(kTestText)));
    ASSERT_EQ(memcmp(kTestText, buf, sizeof(kTestText)), 0);
  }

  // Cleanup.
  unlink(tempfile_name);
  unlink(permfile_name);
}

void TestStatHelper(bool fast_check_in_client, bool follow_links) {
  ScopedTemporaryFile tmp_file;
  EXPECT_EQ(12, write(tmp_file.fd(), "blahblahblah", 12));

  std::string temp_str = tmp_file.full_file_name();
  const char* tempfile_name = temp_str.c_str();
  const char* nonesuch_name = "/mbogo/fictitious/nonesuch";
  const char* leading_path1 = "/mbogo/fictitious";
  const char* leading_path2 = "/mbogo";
  const char* leading_path3 = "/";
  const char* bad_leading_path1 = "/mbog";
  const char* bad_leading_path2 = "/mboga";
  const char* bad_leading_path3 = "/mbogos";
  const char* bad_leading_path4 = "/mbogo/fictitiou";
  const char* bad_leading_path5 = "/mbogo/fictitioux";
  const char* bad_leading_path6 = "/mbogo/fictitiousa";

  default_stat_struct sb;

  {
    // Actual file with permissions to see file but command not allowed.
    std::vector<BrokerFilePermission> permissions = {
        BrokerFilePermission::ReadOnly(tempfile_name)};
    auto policy = std::make_optional<BrokerSandboxConfig>(
        BrokerCommandSet(), permissions, kFakeErrnoSentinel);
    BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                              fast_check_in_client);

    ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));

    memset(&sb, 0, sizeof(sb));
    EXPECT_EQ(-kFakeErrnoSentinel,
              open_broker.GetBrokerClientSignalBased()->DefaultStatForTesting(
                  tempfile_name, follow_links, &sb));
  }

  BrokerCommandSet command_set;
  command_set.set(COMMAND_STAT);

  {
    // Nonexistent file with no permissions to see file.
    std::vector<BrokerFilePermission> permissions;
    auto policy = std::make_optional<BrokerSandboxConfig>(
        command_set, permissions, kFakeErrnoSentinel);
    BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                              fast_check_in_client);

    ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));

    memset(&sb, 0, sizeof(sb));
    EXPECT_EQ(-kFakeErrnoSentinel,
              open_broker.GetBrokerClientSignalBased()->DefaultStatForTesting(
                  nonesuch_name, follow_links, &sb));
  }
  {
    // Actual file with no permission to see file.
    std::vector<BrokerFilePermission> permissions;
    auto policy = std::make_optional<BrokerSandboxConfig>(
        command_set, permissions, kFakeErrnoSentinel);
    BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                              fast_check_in_client);

    ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));

    memset(&sb, 0, sizeof(sb));
    EXPECT_EQ(-kFakeErrnoSentinel,
              open_broker.GetBrokerClientSignalBased()->DefaultStatForTesting(
                  tempfile_name, follow_links, &sb));
  }
  {
    // Nonexistent file with permissions to see file.
    std::vector<BrokerFilePermission> permissions = {
        BrokerFilePermission::ReadOnly(nonesuch_name)};
    auto policy = std::make_optional<BrokerSandboxConfig>(
        command_set, permissions, kFakeErrnoSentinel);
    BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                              fast_check_in_client);

    ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));

    memset(&sb, 0, sizeof(sb));
    EXPECT_EQ(-ENOENT,
              open_broker.GetBrokerClientSignalBased()->DefaultStatForTesting(
                  nonesuch_name, follow_links, &sb));

    // Gets denied all the way back to root since no create permission.
    EXPECT_EQ(-kFakeErrnoSentinel,
              open_broker.GetBrokerClientSignalBased()->DefaultStatForTesting(
                  leading_path1, follow_links, &sb));
    EXPECT_EQ(-kFakeErrnoSentinel,
              open_broker.GetBrokerClientSignalBased()->DefaultStatForTesting(
                  leading_path2, follow_links, &sb));
    EXPECT_EQ(-kFakeErrnoSentinel,
              open_broker.GetBrokerClientSignalBased()->DefaultStatForTesting(
                  leading_path3, follow_links, &sb));

    // Not fooled by substrings.
    EXPECT_EQ(-kFakeErrnoSentinel,
              open_broker.GetBrokerClientSignalBased()->DefaultStatForTesting(
                  bad_leading_path1, follow_links, &sb));
    EXPECT_EQ(-kFakeErrnoSentinel,
              open_broker.GetBrokerClientSignalBased()->DefaultStatForTesting(
                  bad_leading_path2, follow_links, &sb));
    EXPECT_EQ(-kFakeErrnoSentinel,
              open_broker.GetBrokerClientSignalBased()->DefaultStatForTesting(
                  bad_leading_path3, follow_links, &sb));
    EXPECT_EQ(-kFakeErrnoSentinel,
              open_broker.GetBrokerClientSignalBased()->DefaultStatForTesting(
                  bad_leading_path4, follow_links, &sb));
    EXPECT_EQ(-kFakeErrnoSentinel,
              open_broker.GetBrokerClientSignalBased()->DefaultStatForTesting(
                  bad_leading_path5, follow_links, &sb));
    EXPECT_EQ(-kFakeErrnoSentinel,
              open_broker.GetBrokerClientSignalBased()->DefaultStatForTesting(
                  bad_leading_path6, follow_links, &sb));
  }
  {
    // Nonexistent file with permissions to create file.
    std::vector<BrokerFilePermission> permissions = {
        BrokerFilePermission::ReadWriteCreate(nonesuch_name)};
    auto policy = std::make_optional<BrokerSandboxConfig>(
        command_set, permissions, kFakeErrnoSentinel);
    BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                              fast_check_in_client);

    ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));

    memset(&sb, 0, sizeof(sb));
    EXPECT_EQ(-ENOENT,
              open_broker.GetBrokerClientSignalBased()->DefaultStatForTesting(
                  nonesuch_name, follow_links, &sb));

    // Gets ENOENT all the way back to root since it has create permission.
    EXPECT_EQ(-ENOENT,
              open_broker.GetBrokerClientSignalBased()->DefaultStatForTesting(
                  leading_path1, follow_links, &sb));
    EXPECT_EQ(-ENOENT,
              open_broker.GetBrokerClientSignalBased()->DefaultStatForTesting(
                  leading_path2, follow_links, &sb));

    // But can always get the root.
    EXPECT_EQ(0,
              open_broker.GetBrokerClientSignalBased()->DefaultStatForTesting(
                  leading_path3, follow_links, &sb));

    // Not fooled by substrings.
    EXPECT_EQ(-kFakeErrnoSentinel,
              open_broker.GetBrokerClientSignalBased()->DefaultStatForTesting(
                  bad_leading_path1, follow_links, &sb));
    EXPECT_EQ(-kFakeErrnoSentinel,
              open_broker.GetBrokerClientSignalBased()->DefaultStatForTesting(
                  bad_leading_path2, follow_links, &sb));
    EXPECT_EQ(-kFakeErrnoSentinel,
              open_broker.GetBrokerClientSignalBased()->DefaultStatForTesting(
                  bad_leading_path3, follow_links, &sb));
    EXPECT_EQ(-kFakeErrnoSentinel,
              open_broker.GetBrokerClientSignalBased()->DefaultStatForTesting(
                  bad_leading_path4, follow_links, &sb));
    EXPECT_EQ(-kFakeErrnoSentinel,
              open_broker.GetBrokerClientSignalBased()->DefaultStatForTesting(
                  bad_leading_path5, follow_links, &sb));
    EXPECT_EQ(-kFakeErrnoSentinel,
              open_broker.GetBrokerClientSignalBased()->DefaultStatForTesting(
                  bad_leading_path6, follow_links, &sb));
  }
  {
    // Actual file with permissions to see file.
    std::vector<BrokerFilePermission> permissions = {
        BrokerFilePermission::ReadOnly(tempfile_name)};
    auto policy = std::make_optional<BrokerSandboxConfig>(
        command_set, permissions, kFakeErrnoSentinel);
    BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                              fast_check_in_client);

    ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));

    memset(&sb, 0, sizeof(sb));
    EXPECT_EQ(0,
              open_broker.GetBrokerClientSignalBased()->DefaultStatForTesting(
                  tempfile_name, follow_links, &sb));

    // Following fields may never be consistent but should be non-zero.
    // Don't trust the platform to define fields with any particular sign.
    EXPECT_NE(0u, static_cast<unsigned int>(sb.st_dev));
    EXPECT_NE(0u, static_cast<unsigned int>(sb.st_ino));
    EXPECT_NE(0u, static_cast<unsigned int>(sb.st_mode));
    EXPECT_NE(0u, static_cast<unsigned int>(sb.st_blksize));
    EXPECT_NE(0u, static_cast<unsigned int>(sb.st_blocks));

    // We are the ones that made the file.
    EXPECT_EQ(geteuid(), sb.st_uid);
    EXPECT_EQ(getegid(), sb.st_gid);

    // Wrote 12 bytes above which should fit in one block.
    EXPECT_EQ(12, sb.st_size);

    // Can't go backwards in time, 1500000000 was some time ago.
    EXPECT_LT(1500000000u, static_cast<unsigned int>(sb.st_atime_));
    EXPECT_LT(1500000000u, static_cast<unsigned int>(sb.st_mtime_));
    EXPECT_LT(1500000000u, static_cast<unsigned int>(sb.st_ctime_));
  }
}

TEST(BrokerProcess, StatFileClient) {
  TestStatHelper(true, true);
  TestStatHelper(true, false);
}

TEST(BrokerProcess, StatFileHost) {
  TestStatHelper(false, true);
  TestStatHelper(false, false);
}

void TestRenameHelper(bool fast_check_in_client) {
  std::string oldpath;
  std::string newpath;
  {
    // Just to generate names and ensure they do not exist upon scope exit.
    ScopedTemporaryFile oldfile;
    ScopedTemporaryFile newfile;
    oldpath = oldfile.full_file_name();
    newpath = newfile.full_file_name();
  }

  // Now make a file using old path name.
  int fd = open(oldpath.c_str(), O_RDWR | O_CREAT, 0600);
  EXPECT_TRUE(fd > 0);
  close(fd);

  EXPECT_TRUE(access(oldpath.c_str(), F_OK) == 0);
  EXPECT_TRUE(access(newpath.c_str(), F_OK) < 0);

  std::vector<BrokerFilePermission> rwc_permissions = {
      BrokerFilePermission::ReadWriteCreate(oldpath),
      BrokerFilePermission::ReadWriteCreate(newpath)};

  {
    // Check rename fails with write permissions to both files but command
    // itself is not allowed.
    auto policy = std::make_optional<BrokerSandboxConfig>(
        BrokerCommandSet(), rwc_permissions, kFakeErrnoSentinel);
    BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                              fast_check_in_client);

    ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));
    EXPECT_EQ(-kFakeErrnoSentinel,
              open_broker.GetBrokerClientSignalBased()->Rename(
                  oldpath.c_str(), newpath.c_str()));

    // ... and no files moved around.
    EXPECT_TRUE(access(oldpath.c_str(), F_OK) == 0);
    EXPECT_TRUE(access(newpath.c_str(), F_OK) < 0);
  }

  BrokerCommandSet command_set = MakeBrokerCommandSet({COMMAND_RENAME});

  {
    // Check rename fails when no permission to new file.
    std::vector<BrokerFilePermission> permissions = {
        BrokerFilePermission::ReadWriteCreate(oldpath)};
    auto policy = std::make_optional<BrokerSandboxConfig>(
        command_set, permissions, kFakeErrnoSentinel);
    BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                              fast_check_in_client);

    ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));
    EXPECT_EQ(-kFakeErrnoSentinel,
              open_broker.GetBrokerClientSignalBased()->Rename(
                  oldpath.c_str(), newpath.c_str()));

    // ... and no files moved around.
    EXPECT_TRUE(access(oldpath.c_str(), F_OK) == 0);
    EXPECT_TRUE(access(newpath.c_str(), F_OK) < 0);
  }
  {
    // Check rename fails when no permission to old file.
    std::vector<BrokerFilePermission> permissions = {
        BrokerFilePermission::ReadWriteCreate(newpath)};
    auto policy = std::make_optional<BrokerSandboxConfig>(
        command_set, permissions, kFakeErrnoSentinel);
    BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                              fast_check_in_client);

    ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));
    EXPECT_EQ(-kFakeErrnoSentinel,
              open_broker.GetBrokerClientSignalBased()->Rename(
                  oldpath.c_str(), newpath.c_str()));

    // ... and no files moved around.
    EXPECT_TRUE(access(oldpath.c_str(), F_OK) == 0);
    EXPECT_TRUE(access(newpath.c_str(), F_OK) < 0);
  }
  {
    // Check rename fails when only read permission to first file.
    std::vector<BrokerFilePermission> permissions = {
        BrokerFilePermission::ReadOnly(oldpath),
        BrokerFilePermission::ReadWriteCreate(newpath)};
    auto policy = std::make_optional<BrokerSandboxConfig>(
        command_set, permissions, kFakeErrnoSentinel);
    BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                              fast_check_in_client);

    ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));
    EXPECT_EQ(-kFakeErrnoSentinel,
              open_broker.GetBrokerClientSignalBased()->Rename(
                  oldpath.c_str(), newpath.c_str()));

    // ... and no files moved around.
    EXPECT_TRUE(access(oldpath.c_str(), F_OK) == 0);
    EXPECT_TRUE(access(newpath.c_str(), F_OK) < 0);
  }
  {
    // Check rename fails when only read permission to second file.
    std::vector<BrokerFilePermission> permissions = {
        BrokerFilePermission::ReadWriteCreate(oldpath),
        BrokerFilePermission::ReadOnly(newpath)};
    auto policy = std::make_optional<BrokerSandboxConfig>(
        command_set, permissions, kFakeErrnoSentinel);
    BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                              fast_check_in_client);

    ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));
    EXPECT_EQ(-kFakeErrnoSentinel,
              open_broker.GetBrokerClientSignalBased()->Rename(
                  oldpath.c_str(), newpath.c_str()));

    // ... and no files moved around.
    EXPECT_TRUE(access(oldpath.c_str(), F_OK) == 0);
    EXPECT_TRUE(access(newpath.c_str(), F_OK) < 0);
  }
  {
    // Check rename passes with write permissions to both files.
    auto policy = std::make_optional<BrokerSandboxConfig>(
        command_set, rwc_permissions, kFakeErrnoSentinel);
    BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                              fast_check_in_client);

    ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));
    EXPECT_EQ(0, open_broker.GetBrokerClientSignalBased()->Rename(
                     oldpath.c_str(), newpath.c_str()));

    // ... and files were moved around.
    EXPECT_TRUE(access(oldpath.c_str(), F_OK) < 0);
    EXPECT_TRUE(access(newpath.c_str(), F_OK) == 0);
  }

  // Cleanup using new path name.
  unlink(newpath.c_str());
}

TEST(BrokerProcess, RenameFileClient) {
  TestRenameHelper(true);
}

TEST(BrokerProcess, RenameFileHost) {
  TestRenameHelper(false);
}

void TestReadlinkHelper(bool fast_check_in_client) {
  std::string oldpath;
  std::string newpath;
  {
    // Just to generate names and ensure they do not exist upon scope exit.
    ScopedTemporaryFile oldfile;
    ScopedTemporaryFile newfile;
    oldpath = oldfile.full_file_name();
    newpath = newfile.full_file_name();
  }

  // Now make a link from old to new path name.
  EXPECT_TRUE(symlink(oldpath.c_str(), newpath.c_str()) == 0);

  const char* nonesuch_name = "/mbogo/nonesuch";
  const char* oldpath_name = oldpath.c_str();
  const char* newpath_name = newpath.c_str();
  char buf[1024];
  {
    // Actual file with permissions to see file but command itself not allowed.
    std::vector<BrokerFilePermission> permissions = {
        BrokerFilePermission::ReadOnly(newpath_name)};
    auto policy = std::make_optional<BrokerSandboxConfig>(
        BrokerCommandSet(), permissions, kFakeErrnoSentinel);
    BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                              fast_check_in_client);

    ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));
    EXPECT_EQ(-kFakeErrnoSentinel,
              open_broker.GetBrokerClientSignalBased()->Readlink(
                  newpath_name, buf, sizeof(buf)));
  }

  BrokerCommandSet command_set;
  command_set.set(COMMAND_READLINK);

  {
    // Nonexistent file with no permissions to see file.
    std::vector<BrokerFilePermission> permissions;
    auto policy = std::make_optional<BrokerSandboxConfig>(
        command_set, permissions, kFakeErrnoSentinel);
    BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                              fast_check_in_client);

    ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));
    EXPECT_EQ(-kFakeErrnoSentinel,
              open_broker.GetBrokerClientSignalBased()->Readlink(
                  nonesuch_name, buf, sizeof(buf)));
  }
  {
    // Actual file with no permissions to see file.
    std::vector<BrokerFilePermission> permissions;
    auto policy = std::make_optional<BrokerSandboxConfig>(
        command_set, permissions, kFakeErrnoSentinel);
    BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                              fast_check_in_client);

    ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));
    EXPECT_EQ(-kFakeErrnoSentinel,
              open_broker.GetBrokerClientSignalBased()->Readlink(
                  newpath_name, buf, sizeof(buf)));
  }
  {
    // Nonexistent file with permissions to see file.
    std::vector<BrokerFilePermission> permissions = {
        BrokerFilePermission::ReadOnly(nonesuch_name)};
    auto policy = std::make_optional<BrokerSandboxConfig>(
        command_set, permissions, kFakeErrnoSentinel);
    BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                              fast_check_in_client);

    ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));
    EXPECT_EQ(-ENOENT, open_broker.GetBrokerClientSignalBased()->Readlink(
                           nonesuch_name, buf, sizeof(buf)));
  }
  {
    // Actual file with permissions to see file.
    std::vector<BrokerFilePermission> permissions = {
        BrokerFilePermission::ReadOnly(newpath_name)};
    auto policy = std::make_optional<BrokerSandboxConfig>(
        command_set, permissions, kFakeErrnoSentinel);
    BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                              fast_check_in_client);

    ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));
    ssize_t retlen = open_broker.GetBrokerClientSignalBased()->Readlink(
        newpath_name, buf, sizeof(buf));
    EXPECT_TRUE(retlen == static_cast<ssize_t>(strlen(oldpath_name)));
    EXPECT_EQ(0, memcmp(oldpath_name, buf, retlen));
  }
  {
    // Actual file with permissions to see file, but too small a buffer.
    std::vector<BrokerFilePermission> permissions = {
        BrokerFilePermission::ReadOnly(newpath_name)};
    auto policy = std::make_optional<BrokerSandboxConfig>(
        command_set, permissions, kFakeErrnoSentinel);
    BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                              fast_check_in_client);

    ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));
    EXPECT_EQ(4, open_broker.GetBrokerClientSignalBased()->Readlink(
                     newpath_name, buf, 4));
  }

  // Cleanup both paths.
  unlink(oldpath.c_str());
  unlink(newpath.c_str());
}

TEST(BrokerProcess, ReadlinkFileClient) {
  TestReadlinkHelper(true);
}

TEST(BrokerProcess, ReadlinkFileHost) {
  TestReadlinkHelper(false);
}

void TestMkdirHelper(bool fast_check_in_client) {
  std::string path;
  {
    // Generate name and ensure it does not exist upon scope exit.
    ScopedTemporaryFile file;
    path = file.full_file_name();
  }
  const char* path_name = path.c_str();
  const char* nonesuch_name = "/mbogo/nonesuch";

  std::vector<BrokerFilePermission> no_permissions;
  std::vector<BrokerFilePermission> ro_permissions = {
      BrokerFilePermission::ReadOnly(path_name),
      BrokerFilePermission::ReadOnly(nonesuch_name)};

  std::vector<BrokerFilePermission> rw_permissions = {
      BrokerFilePermission::ReadWrite(path_name),
      BrokerFilePermission::ReadWrite(nonesuch_name)};

  std::vector<BrokerFilePermission> rwc_permissions = {
      BrokerFilePermission::ReadWriteCreate(path_name),
      BrokerFilePermission::ReadWriteCreate(nonesuch_name)};

  {
    // Actual file with permissions to use but command itself not allowed.
    auto policy = std::make_optional<BrokerSandboxConfig>(
        BrokerCommandSet(), rw_permissions, kFakeErrnoSentinel);
    BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                              fast_check_in_client);

    ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));
    EXPECT_EQ(-kFakeErrnoSentinel,
              open_broker.GetBrokerClientSignalBased()->Mkdir(path_name, 0600));
  }

  BrokerCommandSet command_set = MakeBrokerCommandSet({COMMAND_MKDIR});

  {
    // Nonexistent file with no permissions to see file.
    auto policy = std::make_optional<BrokerSandboxConfig>(
        command_set, no_permissions, kFakeErrnoSentinel);
    BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                              fast_check_in_client);

    ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));
    EXPECT_EQ(
        -kFakeErrnoSentinel,
        open_broker.GetBrokerClientSignalBased()->Mkdir(nonesuch_name, 0600));
  }
  {
    // Actual file with no permissions to see file.
    auto policy = std::make_optional<BrokerSandboxConfig>(
        command_set, no_permissions, kFakeErrnoSentinel);
    BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                              fast_check_in_client);

    ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));
    EXPECT_EQ(-kFakeErrnoSentinel,
              open_broker.GetBrokerClientSignalBased()->Mkdir(path_name, 0600));
  }
  {
    // Nonexistent file with insufficient permissions to see file.
    auto policy = std::make_optional<BrokerSandboxConfig>(
        command_set, ro_permissions, kFakeErrnoSentinel);
    BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                              fast_check_in_client);

    ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));
    EXPECT_EQ(
        -kFakeErrnoSentinel,
        open_broker.GetBrokerClientSignalBased()->Mkdir(nonesuch_name, 0600));
  }
  {
    // Actual file with insufficient permissions to see file.
    auto policy = std::make_optional<BrokerSandboxConfig>(
        command_set, ro_permissions, kFakeErrnoSentinel);
    BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                              fast_check_in_client);

    ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));
    EXPECT_EQ(-kFakeErrnoSentinel,
              open_broker.GetBrokerClientSignalBased()->Mkdir(path_name, 0600));
  }
  {
    // Nonexistent file with insufficient permissions to see file, case 2.
    auto policy = std::make_optional<BrokerSandboxConfig>(
        command_set, rw_permissions, kFakeErrnoSentinel);
    BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                              fast_check_in_client);

    ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));
    EXPECT_EQ(
        -kFakeErrnoSentinel,
        open_broker.GetBrokerClientSignalBased()->Mkdir(nonesuch_name, 0600));
  }
  {
    // Actual file with insufficient permissions to see file, case 2.
    auto policy = std::make_optional<BrokerSandboxConfig>(
        command_set, rw_permissions, kFakeErrnoSentinel);
    BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                              fast_check_in_client);

    ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));
    EXPECT_EQ(-kFakeErrnoSentinel,
              open_broker.GetBrokerClientSignalBased()->Mkdir(path_name, 0600));
  }
  {
    // Nonexistent file with permissions to see file.
    auto policy = std::make_optional<BrokerSandboxConfig>(
        command_set, rwc_permissions, kFakeErrnoSentinel);
    BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                              fast_check_in_client);

    ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));
    EXPECT_EQ(-ENOENT, open_broker.GetBrokerClientSignalBased()->Mkdir(
                           nonesuch_name, 0600));
  }
  {
    // Actual file with permissions to see file.
    auto policy = std::make_optional<BrokerSandboxConfig>(
        command_set, rwc_permissions, kFakeErrnoSentinel);
    BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                              fast_check_in_client);

    ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));
    EXPECT_EQ(0,
              open_broker.GetBrokerClientSignalBased()->Mkdir(path_name, 0600));
  }

  // Cleanup.
  rmdir(path_name);
}

TEST(BrokerProcess, MkdirClient) {
  TestMkdirHelper(true);
}

TEST(BrokerProcess, MkdirHost) {
  TestMkdirHelper(false);
}

void TestRmdirHelper(bool fast_check_in_client) {
  std::string path;
  {
    // Generate name and ensure it does not exist upon scope exit.
    ScopedTemporaryFile file;
    path = file.full_file_name();
  }
  const char* path_name = path.c_str();
  const char* nonesuch_name = "/mbogo/nonesuch";

  EXPECT_EQ(0, mkdir(path_name, 0600));
  EXPECT_EQ(0, access(path_name, F_OK));

  std::vector<BrokerFilePermission> no_permissions;
  std::vector<BrokerFilePermission> ro_permissions = {
      BrokerFilePermission::ReadOnly(path_name),
      BrokerFilePermission::ReadOnly(nonesuch_name)};

  std::vector<BrokerFilePermission> rw_permissions = {
      BrokerFilePermission::ReadWrite(path_name),
      BrokerFilePermission::ReadWrite(nonesuch_name)};

  std::vector<BrokerFilePermission> rwc_permissions = {
      BrokerFilePermission::ReadWriteCreate(path_name),
      BrokerFilePermission::ReadWriteCreate(nonesuch_name)};

  {
    // Actual dir with permissions to use but command itself not allowed.
    auto policy = std::make_optional<BrokerSandboxConfig>(
        BrokerCommandSet(), rw_permissions, kFakeErrnoSentinel);
    BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                              fast_check_in_client);

    ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));
    EXPECT_EQ(-kFakeErrnoSentinel,
              open_broker.GetBrokerClientSignalBased()->Rmdir(path_name));
  }
  EXPECT_EQ(0, access(path_name, F_OK));

  BrokerCommandSet command_set = MakeBrokerCommandSet({COMMAND_RMDIR});

  {
    // Nonexistent dir with no permissions to see dir.
    auto policy = std::make_optional<BrokerSandboxConfig>(
        command_set, no_permissions, kFakeErrnoSentinel);
    BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                              fast_check_in_client);

    ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));
    EXPECT_EQ(-kFakeErrnoSentinel,
              open_broker.GetBrokerClientSignalBased()->Rmdir(nonesuch_name));
  }
  EXPECT_EQ(0, access(path_name, F_OK));

  {
    // Actual dir with no permissions to see dir.
    auto policy = std::make_optional<BrokerSandboxConfig>(
        command_set, no_permissions, kFakeErrnoSentinel);
    BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                              fast_check_in_client);

    ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));
    EXPECT_EQ(-kFakeErrnoSentinel,
              open_broker.GetBrokerClientSignalBased()->Rmdir(path_name));
  }
  EXPECT_EQ(0, access(path_name, F_OK));

  {
    // Nonexistent dir with insufficient permissions to see dir.
    auto policy = std::make_optional<BrokerSandboxConfig>(
        command_set, ro_permissions, kFakeErrnoSentinel);
    BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                              fast_check_in_client);

    ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));
    EXPECT_EQ(-kFakeErrnoSentinel,
              open_broker.GetBrokerClientSignalBased()->Rmdir(nonesuch_name));
  }
  EXPECT_EQ(0, access(path_name, F_OK));

  {
    // Actual dir with insufficient permissions to see dir.
    auto policy = std::make_optional<BrokerSandboxConfig>(
        command_set, ro_permissions, kFakeErrnoSentinel);
    BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                              fast_check_in_client);

    ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));
    EXPECT_EQ(-kFakeErrnoSentinel,
              open_broker.GetBrokerClientSignalBased()->Rmdir(path_name));
  }
  EXPECT_EQ(0, access(path_name, F_OK));

  {
    // Nonexistent dir with insufficient permissions to see dir, case 2.
    auto policy = std::make_optional<BrokerSandboxConfig>(
        command_set, rw_permissions, kFakeErrnoSentinel);
    BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                              fast_check_in_client);

    ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));
    EXPECT_EQ(-kFakeErrnoSentinel,
              open_broker.GetBrokerClientSignalBased()->Rmdir(nonesuch_name));
  }
  EXPECT_EQ(0, access(path_name, F_OK));

  {
    // Actual dir with insufficient permissions to see dir, case 2.
    auto policy = std::make_optional<BrokerSandboxConfig>(
        command_set, rw_permissions, kFakeErrnoSentinel);
    BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                              fast_check_in_client);

    ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));
    EXPECT_EQ(-kFakeErrnoSentinel,
              open_broker.GetBrokerClientSignalBased()->Rmdir(path_name));
  }
  EXPECT_EQ(0, access(path_name, F_OK));

  {
    // Nonexistent dir with permissions to see dir.
    auto policy = std::make_optional<BrokerSandboxConfig>(
        command_set, rwc_permissions, kFakeErrnoSentinel);
    BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                              fast_check_in_client);

    ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));
    EXPECT_TRUE(open_broker.GetBrokerClientSignalBased()->Rmdir(nonesuch_name) <
                0);
  }
  EXPECT_EQ(0, access(path_name, F_OK));

  {
    // Actual dir with permissions to see dir.
    auto policy = std::make_optional<BrokerSandboxConfig>(
        command_set, rwc_permissions, kFakeErrnoSentinel);
    BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                              fast_check_in_client);

    ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));
    EXPECT_EQ(0, open_broker.GetBrokerClientSignalBased()->Rmdir(path_name));
  }
  // Confirm it was erased.
  EXPECT_EQ(-1, access(path_name, F_OK));
}

TEST(BrokerProcess, RmdirClient) {
  TestRmdirHelper(true);
}

TEST(BrokerProcess, RmdirHost) {
  TestRmdirHelper(false);
}

// Will have to split this into many tests, and the "cleanup" will have ASSERTs.
void TestUnlinkHelper(bool fast_check_in_client) {
  std::string path;
  {
    // Generate name and ensure it does not exist upon scope exit.
    ScopedTemporaryFile file;
    path = file.full_file_name();
  }
  const char* path_name = path.c_str();
  const char* nonesuch_name = "/mbogo/nonesuch";

  int fd = open(path_name, O_RDWR | O_CREAT, 0600);
  EXPECT_TRUE(fd >= 0);
  close(fd);
  EXPECT_EQ(0, access(path_name, F_OK));

  std::vector<BrokerFilePermission> no_permissions;
  std::vector<BrokerFilePermission> ro_permissions = {
      BrokerFilePermission::ReadOnly(path_name),
      BrokerFilePermission::ReadOnly(nonesuch_name)};

  std::vector<BrokerFilePermission> rw_permissions = {
      BrokerFilePermission::ReadWrite(path_name),
      BrokerFilePermission::ReadWrite(nonesuch_name)};

  std::vector<BrokerFilePermission> rwc_permissions = {
      BrokerFilePermission::ReadWriteCreate(path_name),
      BrokerFilePermission::ReadWriteCreate(nonesuch_name)};

  {
    // Actual file with permissions to use but command itself not allowed.
    auto policy = std::make_optional<BrokerSandboxConfig>(
        BrokerCommandSet(), rwc_permissions, kFakeErrnoSentinel);
    BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                              fast_check_in_client);

    ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));
    EXPECT_EQ(-kFakeErrnoSentinel,
              open_broker.GetBrokerClientSignalBased()->Unlink(path_name));
  }
  EXPECT_EQ(0, access(path_name, F_OK));

  BrokerCommandSet command_set = MakeBrokerCommandSet({COMMAND_UNLINK});

  {
    // Nonexistent file with no permissions to see file.
    auto policy = std::make_optional<BrokerSandboxConfig>(
        command_set, no_permissions, kFakeErrnoSentinel);
    BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                              fast_check_in_client);

    ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));
    EXPECT_EQ(-kFakeErrnoSentinel,
              open_broker.GetBrokerClientSignalBased()->Unlink(nonesuch_name));
  }
  EXPECT_EQ(0, access(path_name, F_OK));

  {
    // Actual file with no permissions to see file.
    auto policy = std::make_optional<BrokerSandboxConfig>(
        command_set, no_permissions, kFakeErrnoSentinel);
    BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                              fast_check_in_client);

    ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));
    EXPECT_EQ(-kFakeErrnoSentinel,
              open_broker.GetBrokerClientSignalBased()->Unlink(path_name));
  }
  EXPECT_EQ(0, access(path_name, F_OK));

  {
    // Nonexistent file with insufficient permissions to see file.
    auto policy = std::make_optional<BrokerSandboxConfig>(
        command_set, ro_permissions, kFakeErrnoSentinel);
    BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                              fast_check_in_client);

    ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));
    EXPECT_EQ(-kFakeErrnoSentinel,
              open_broker.GetBrokerClientSignalBased()->Unlink(nonesuch_name));
  }
  EXPECT_EQ(0, access(path_name, F_OK));

  {
    // Actual file with insufficient permissions to see file.
    auto policy = std::make_optional<BrokerSandboxConfig>(
        command_set, ro_permissions, kFakeErrnoSentinel);
    BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                              fast_check_in_client);

    ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));
    EXPECT_EQ(-kFakeErrnoSentinel,
              open_broker.GetBrokerClientSignalBased()->Unlink(path_name));
  }
  EXPECT_EQ(0, access(path_name, F_OK));

  {
    // Nonexistent file with insufficient permissions to see file, case 2.
    auto policy = std::make_optional<BrokerSandboxConfig>(
        command_set, rw_permissions, kFakeErrnoSentinel);
    BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                              fast_check_in_client);

    ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));
    EXPECT_EQ(-kFakeErrnoSentinel,
              open_broker.GetBrokerClientSignalBased()->Unlink(nonesuch_name));
  }
  EXPECT_EQ(0, access(path_name, F_OK));

  {
    // Actual file with insufficient permissions to see file, case 2.
    auto policy = std::make_optional<BrokerSandboxConfig>(
        command_set, rw_permissions, kFakeErrnoSentinel);
    BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                              fast_check_in_client);

    ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));
    EXPECT_EQ(-kFakeErrnoSentinel,
              open_broker.GetBrokerClientSignalBased()->Unlink(path_name));
  }
  EXPECT_EQ(0, access(path_name, F_OK));

  {
    // Nonexistent file with permissions to see file.
    auto policy = std::make_optional<BrokerSandboxConfig>(
        command_set, rwc_permissions, kFakeErrnoSentinel);
    BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                              fast_check_in_client);

    ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));
    EXPECT_TRUE(
        open_broker.GetBrokerClientSignalBased()->Unlink(nonesuch_name) < 0);
  }
  EXPECT_EQ(0, access(path_name, F_OK));

  {
    // Actual file with permissions to see file.
    auto policy = std::make_optional<BrokerSandboxConfig>(
        command_set, rwc_permissions, kFakeErrnoSentinel);
    BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                              fast_check_in_client);

    ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));
    EXPECT_EQ(0, open_broker.GetBrokerClientSignalBased()->Unlink(path_name));
  }
  // Confirm it was erased.
  EXPECT_EQ(-1, access(path_name, F_OK));
}

TEST(BrokerProcess, UnlinkClient) {
  TestUnlinkHelper(true);
}

TEST(BrokerProcess, UnlinkHost) {
  TestUnlinkHelper(false);
}

void TestInotifyAddWatchHelper(bool fast_check_in_client) {
  const uint32_t kBadMask =
      IN_CREATE | IN_DELETE | IN_CLOSE_WRITE | IN_MOVE | IN_ONLYDIR;
  const uint32_t kGoodMask = kBadMask | IN_ATTRIB;

  // Create two nested temp dirs.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(
      temp_dir.CreateUniqueTempDirUnderPath(base::FilePath(kTempDirForTests)));
  std::string temp_dir_str = temp_dir.GetPath().MaybeAsASCII();
  ASSERT_FALSE(temp_dir_str.empty());

  base::ScopedTempDir nested_temp_dir;
  ASSERT_TRUE(
      nested_temp_dir.Set(temp_dir.GetPath().AppendASCII("nested_temp_dir")));
  std::string nested_temp_dir_str = nested_temp_dir.GetPath().MaybeAsASCII();
  ASSERT_FALSE(nested_temp_dir_str.empty());

  base::FilePath bad_prefix = temp_dir.GetPath().AppendASCII("nested_t");
  std::string bad_prefix_str = bad_prefix.MaybeAsASCII();
  ASSERT_FALSE(bad_prefix_str.empty());

  {
    // Try to watch a directory without COMMAND_INOTIFY_ADD_WATCH
    std::vector<BrokerFilePermission> permissions = {
        BrokerFilePermission::InotifyAddWatchWithIntermediateDirs(
            nested_temp_dir_str)};
    auto policy = std::make_optional<BrokerSandboxConfig>(
        BrokerCommandSet(), permissions, kFakeErrnoSentinel);
    BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                              fast_check_in_client);

    ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));

    base::ScopedFD inotify_instance(inotify_init());
    ASSERT_TRUE(inotify_instance.is_valid());
    EXPECT_EQ(
        -kFakeErrnoSentinel,
        open_broker.GetBrokerClientSignalBased()->InotifyAddWatch(
            inotify_instance.get(), nested_temp_dir_str.c_str(), kGoodMask));
  }

  BrokerCommandSet command_set;
  command_set.set(COMMAND_INOTIFY_ADD_WATCH);

  {
    // Try to watch a directory with no permission.
    std::vector<BrokerFilePermission> permissions;
    auto policy = std::make_optional<BrokerSandboxConfig>(
        command_set, permissions, kFakeErrnoSentinel);
    BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                              fast_check_in_client);

    ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));

    base::ScopedFD inotify_instance(inotify_init());
    ASSERT_TRUE(inotify_instance.is_valid());
    EXPECT_EQ(
        -kFakeErrnoSentinel,
        open_broker.GetBrokerClientSignalBased()->InotifyAddWatch(
            inotify_instance.get(), nested_temp_dir_str.c_str(), kGoodMask));
  }
  {
    // Try to watch a directory with permission, but bad flags.
    std::vector<BrokerFilePermission> permissions = {
        BrokerFilePermission::InotifyAddWatchWithIntermediateDirs(
            nested_temp_dir_str)};
    auto policy = std::make_optional<BrokerSandboxConfig>(
        command_set, permissions, kFakeErrnoSentinel);
    BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                              fast_check_in_client);

    ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));

    base::ScopedFD inotify_instance(inotify_init());
    ASSERT_TRUE(inotify_instance.is_valid());
    EXPECT_EQ(
        -kFakeErrnoSentinel,
        open_broker.GetBrokerClientSignalBased()->InotifyAddWatch(
            inotify_instance.get(), nested_temp_dir_str.c_str(), kBadMask));
  }
  {
    // Add a directory with permissions and make sure it does not give watch
    // permission to uintended directories or files.
    std::vector<BrokerFilePermission> permissions = {
        BrokerFilePermission::InotifyAddWatchWithIntermediateDirs(
            nested_temp_dir_str)};
    auto policy = std::make_optional<BrokerSandboxConfig>(
        command_set, permissions, kFakeErrnoSentinel);
    BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                              fast_check_in_client);

    ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));

    base::ScopedTempDir other_directory;
    ASSERT_TRUE(
        other_directory.CreateUniqueTempDirUnderPath(temp_dir.GetPath()));
    std::string other_directory_str = other_directory.GetPath().MaybeAsASCII();
    ASSERT_FALSE(other_directory_str.empty());

    base::ScopedFD inotify_instance(inotify_init());
    ASSERT_TRUE(inotify_instance.is_valid());

    // Try to watch an unintended directory, should fail.
    ASSERT_EQ(
        -kFakeErrnoSentinel,
        open_broker.GetBrokerClientSignalBased()->InotifyAddWatch(
            inotify_instance.get(), other_directory_str.c_str(), kGoodMask));

    // Try to access a prefix that isn't a full directory.
    ASSERT_EQ(-kFakeErrnoSentinel,
              open_broker.GetBrokerClientSignalBased()->InotifyAddWatch(
                  inotify_instance.get(), bad_prefix_str.c_str(), kGoodMask));
  }
  {
    // Try to watch a directory with permission, and good flags.
    std::vector<BrokerFilePermission> permissions = {
        BrokerFilePermission::InotifyAddWatchWithIntermediateDirs(
            nested_temp_dir_str)};
    auto policy = std::make_optional<BrokerSandboxConfig>(
        command_set, permissions, kFakeErrnoSentinel);
    BrokerProcess open_broker(std::move(policy), BrokerType::SIGNAL_BASED,
                              fast_check_in_client);

    ASSERT_TRUE(open_broker.Fork(base::BindOnce(&NoOpCallback)));

    base::ScopedFD inotify_instance(inotify_init());
    ASSERT_TRUE(inotify_instance.is_valid());

    // Try to watch the directory, which should succeed.
    int wd = open_broker.GetBrokerClientSignalBased()->InotifyAddWatch(
        inotify_instance.get(), nested_temp_dir_str.c_str(), kGoodMask);
    // The returned watch descriptor should be valid.
    ASSERT_LE(0, wd);
    // Removing the watch should succeed.
    ASSERT_EQ(0, inotify_rm_watch(inotify_instance.get(), wd));

    // Now try watching a leading directory, which should succeed.
    wd = open_broker.GetBrokerClientSignalBased()->InotifyAddWatch(
        inotify_instance.get(), temp_dir_str.c_str(), kGoodMask);
    // The returned watch descriptor should be valid.
    ASSERT_LE(0, wd);
    // Removing the watch should succeed.
    ASSERT_EQ(0, inotify_rm_watch(inotify_instance.get(), wd));

    // Watching root should succeed with any valid permission.
    wd = open_broker.GetBrokerClientSignalBased()->InotifyAddWatch(
        inotify_instance.get(), "/", kGoodMask);
    // The returned watch descriptor should be valid.
    ASSERT_LE(0, wd);
    // Removing the watch should succeed.
    ASSERT_EQ(0, inotify_rm_watch(inotify_instance.get(), wd));
  }
}

TEST(BrokerProcess, InotifyAddWatchClient) {
  TestInotifyAddWatchHelper(true);
}

TEST(BrokerProcess, InotifyAddWatchHost) {
  TestInotifyAddWatchHelper(false);
}

TEST(BrokerProcess, IsSyscallAllowed) {
  const base::flat_map<BrokerCommand, base::flat_set<int>> kSysnosForCommand = {
      {COMMAND_ACCESS,
       {__NR_faccessat, __NR_faccessat2,
#if defined(__NR_access) && !BUILDFLAG(IS_ANDROID)
        __NR_access
#endif
       }},
      {COMMAND_MKDIR,
       {__NR_mkdirat,
#if defined(__NR_mkdir) && !BUILDFLAG(IS_ANDROID)
        __NR_mkdir
#endif
       }},
      {COMMAND_OPEN,
       {__NR_openat,
#if defined(__NR_open) && !BUILDFLAG(IS_ANDROID)
        __NR_open
#endif
       }},
      {COMMAND_READLINK,
       {__NR_readlinkat,
#if defined(__NR_readlink) && !BUILDFLAG(IS_ANDROID)
        __NR_readlink
#endif
       }},
      {COMMAND_RENAME,
       {__NR_renameat,
#if defined(__NR_rename) && !BUILDFLAG(IS_ANDROID)
        __NR_rename
#endif
       }},
      {COMMAND_UNLINK,
       {__NR_unlinkat,
#if defined(__NR_unlink) && !BUILDFLAG(IS_ANDROID)
        __NR_unlink
#endif
       }},
      {COMMAND_RMDIR,
       {__NR_unlinkat,
#if defined(__NR_rmdir) && !BUILDFLAG(IS_ANDROID)
        __NR_rmdir
#endif
       }},
      {COMMAND_STAT,
       {
#if defined(__NR_stat) && !BUILDFLAG(IS_ANDROID)
           __NR_stat,
#endif
#if defined(__NR_lstat) && !BUILDFLAG(IS_ANDROID)
           __NR_lstat,
#endif
#if defined(__NR_fstatat)
           __NR_fstatat,
#endif
#if defined(__NR_fstatat64)
           __NR_fstatat64,
#endif
#if defined(__NR_newfstatat)
           __NR_newfstatat,
#endif
#if defined(__NR_stat64)
           __NR_stat64,
#endif
#if defined(__NR_lstat64)
           __NR_lstat64,
#endif
       }},
      {COMMAND_INOTIFY_ADD_WATCH,
       {
#if defined(__NR_inotify_add_watch)
           __NR_inotify_add_watch
#endif
       }}};

  // First gather up all the syscalls numbers we want to test.
  base::flat_set<int> all_sysnos;
  for (const auto& command_sysno_set_pair : kSysnosForCommand) {
    all_sysnos.insert(command_sysno_set_pair.second.begin(),
                      command_sysno_set_pair.second.end());
  }

  for (const auto& test : kSysnosForCommand) {
    // Test with fast_check_in_client.
    {
      BrokerCommand command = test.first;
      const base::flat_set<int>& sysnos = test.second;
      SCOPED_TRACE(base::StringPrintf("fast check, command=%d", command));
      auto policy = std::make_optional<BrokerSandboxConfig>(
          MakeBrokerCommandSet({command}), std::vector<BrokerFilePermission>(),
          ENOSYS);
      BrokerProcess process(std::move(policy), BrokerType::SIGNAL_BASED,
                            /*fast_check_in_client=*/true,
                            /*quiet_failures_for_tests=*/true);
      // Check that only the correct system calls are allowed.
      for (int sysno : all_sysnos) {
        SCOPED_TRACE(base::StringPrintf("test syscalls, sysno=%d", sysno));
        EXPECT_EQ(sysnos.count(sysno) > 0, process.IsSyscallAllowed(sysno));
      }
    }

    // Test without fast_check_in_client.
    {
      BrokerCommand command = test.first;
      SCOPED_TRACE(base::StringPrintf("no fast check, command=%d", command));
      auto policy = std::make_optional<BrokerSandboxConfig>(
          MakeBrokerCommandSet({command}), std::vector<BrokerFilePermission>(),
          ENOSYS);
      BrokerProcess process(std::move(policy), BrokerType::SIGNAL_BASED,
                            /*fast_check_in_client=*/false,
                            /*quiet_failures_for_tests=*/true);
      // Check that all system calls are allowed.
      for (int sysno : all_sysnos) {
        SCOPED_TRACE(base::StringPrintf("test syscalls, sysno=%d", sysno));
        EXPECT_TRUE(process.IsSyscallAllowed(sysno));
      }
    }
  }
}

}  // namespace syscall_broker
}  // namespace sandbox
