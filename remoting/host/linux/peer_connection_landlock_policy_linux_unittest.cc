// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/peer_connection_landlock_policy_linux.h"

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <optional>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/writable_shared_memory_region.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "sandbox/linux/system_headers/linux_landlock.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace remoting {
namespace {

// Sanitizers may need filesystem access (e.g. to write reports) that the
// policy denies, so these tests are disabled under sanitizers.
#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER) || \
    defined(THREAD_SANITIZER) || defined(LEAK_SANITIZER) ||    \
    defined(UNDEFINED_SANITIZER)
#define MAYBE_LANDLOCK_TEST(test_name) DISABLED_##test_name
#else
#define MAYBE_LANDLOCK_TEST(test_name) test_name
#endif

constexpr char kAllowedDirSwitch[] = "allowed-dir";
constexpr char kDeniedDirSwitch[] = "denied-dir";
constexpr char kTestFileName[] = "test_file";

// Exit codes of the child process. The codes are chosen to not collide with
// exit codes commonly used by crash handlers (e.g. 1).
constexpr int kChildSuccess = 0;
constexpr int kChildPolicyNotApplied = 101;
constexpr int kChildCheckFailedBase = 110;

// Exits the child process immediately, without running at-exit handlers, which
// may access files that are denied by the policy.
[[noreturn]] void ExitChild(int exit_code) {
  _exit(exit_code);
}

bool CanOpenForReading(const base::FilePath& path) {
  base::ScopedFD fd(HANDLE_EINTR(open(path.value().c_str(), O_RDONLY)));
  return fd.is_valid();
}

bool OpenFailsWithEacces(const base::FilePath& path, int flags) {
  base::ScopedFD fd(
      HANDLE_EINTR(open(path.value().c_str(), flags | O_CLOEXEC, 0600)));
  return !fd.is_valid() && errno == EACCES;
}

// Note that Landlock only restricts the calling thread (and threads created by
// it afterwards), so all checks below run on the thread that applies the
// policy.
MULTIPROCESS_TEST_MAIN(LandlockPolicyChild) {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  const base::FilePath allowed_dir =
      command_line->GetSwitchValuePath(kAllowedDirSwitch);
  const base::FilePath denied_dir =
      command_line->GetSwitchValuePath(kDeniedDirSwitch);
  // Open the parent's /proc entry and /proc before applying the policy, to
  // verify that it is only the policy that denies opening them afterwards.
  const std::string parent_pid = base::NumberToString(getppid());
  const base::FilePath parent_proc_status("/proc/" + parent_pid + "/status");
  if (!CanOpenForReading(parent_proc_status)) {
    ExitChild(kChildCheckFailedBase);
  }
  base::ScopedFD proc_fd(
      HANDLE_EINTR(open("/proc", O_RDONLY | O_DIRECTORY | O_CLOEXEC)));
  if (!proc_fd.is_valid()) {
    ExitChild(kChildCheckFailedBase);
  }

  if (ApplyPeerConnectionLandlockPolicy({allowed_dir}) !=
      PeerConnectionLandlockResult::kApplied) {
    ExitChild(kChildPolicyNotApplied);
  }

  int check = kChildCheckFailedBase + 1;
  // Allowed reads.
  if (!CanOpenForReading(base::FilePath("/proc/self/status")) ||
      !CanOpenForReading(base::FilePath("/etc/passwd")) ||
      !CanOpenForReading(allowed_dir.Append(kTestFileName))) {
    ExitChild(check);
  }
  ++check;
  // The /proc entries of other processes, including via other paths that
  // resolve to them, and via a file descriptor opened before the policy was
  // applied. Opening /proc itself is denied as well.
  base::ScopedFD status_fd(HANDLE_EINTR(openat(
      proc_fd.get(), (parent_pid + "/status").c_str(), O_RDONLY | O_CLOEXEC)));
  const bool openat_denied = !status_fd.is_valid() && errno == EACCES;
  if (!openat_denied || !OpenFailsWithEacces(parent_proc_status, O_RDONLY) ||
      !OpenFailsWithEacces(
          base::FilePath("/proc/self/../" + parent_pid + "/status"),
          O_RDONLY) ||
      !OpenFailsWithEacces(base::FilePath("/proc"), O_RDONLY | O_DIRECTORY)) {
    ExitChild(check);
  }
  ++check;
  // Writing or creating files, including in the allowed directory.
  if (!OpenFailsWithEacces(allowed_dir.Append(kTestFileName), O_WRONLY) ||
      !OpenFailsWithEacces(allowed_dir.Append("new_file"),
                           O_WRONLY | O_CREAT) ||
      !OpenFailsWithEacces(denied_dir.Append("new_file"), O_WRONLY | O_CREAT)) {
    ExitChild(check);
  }
  ++check;
  // Reading files outside of the allowed paths.
  if (!OpenFailsWithEacces(denied_dir.Append(kTestFileName), O_RDONLY)) {
    ExitChild(check);
  }
  ++check;
  // Shared memory regions, which use memfd and reopen /proc/self/fd/N.
  {
    base::WritableSharedMemoryRegion writable_region =
        base::WritableSharedMemoryRegion::Create(4096);
    base::MappedReadOnlyRegion read_only_region =
        base::ReadOnlySharedMemoryRegion::Create(4096);
    if (!writable_region.IsValid() || !read_only_region.IsValid()) {
      ExitChild(check);
    }
  }
  ++check;
  // Name resolution, which reads files in /etc and may load NSS modules.
  struct addrinfo* result = nullptr;
  if (getaddrinfo("localhost", nullptr, nullptr, &result) != 0) {
    ExitChild(check);
  }
  freeaddrinfo(result);
  ++check;
  // Signals to processes outside of the Landlock domain (ABI 6+).
  if (GetLandlockAbiVersion() >= 6 &&
      !(kill(getppid(), 0) == -1 && errno == EPERM)) {
    ExitChild(check);
  }
  ++check;
  // The process' own /proc entries are still readable after doing other work.
  if (!CanOpenForReading(base::FilePath("/proc/self/status"))) {
    ExitChild(check);
  }
  ExitChild(kChildSuccess);
}

TEST(PeerConnectionLandlockPolicyLinuxTest,
     MAYBE_LANDLOCK_TEST(RestrictsFilesystemAccess)) {
  if (GetLandlockAbiVersion() <= 0) {
    GTEST_SKIP() << "Landlock is not supported.";
  }

  base::ScopedTempDir allowed_dir;
  base::ScopedTempDir denied_dir;
  ASSERT_TRUE(allowed_dir.CreateUniqueTempDir());
  ASSERT_TRUE(denied_dir.CreateUniqueTempDir());
  ASSERT_TRUE(
      base::WriteFile(allowed_dir.GetPath().Append(kTestFileName), "test"));
  ASSERT_TRUE(
      base::WriteFile(denied_dir.GetPath().Append(kTestFileName), "test"));

  base::CommandLine command_line =
      base::GetMultiProcessTestChildBaseCommandLine();
  command_line.AppendSwitchPath(kAllowedDirSwitch, allowed_dir.GetPath());
  command_line.AppendSwitchPath(kDeniedDirSwitch, denied_dir.GetPath());
  base::Process child = base::SpawnMultiProcessTestChild(
      "LandlockPolicyChild", command_line, base::LaunchOptions());
  ASSERT_TRUE(child.IsValid());
  int exit_code = -1;
  if (!base::WaitForMultiprocessTestChildExit(
          child, TestTimeouts::action_timeout(), &exit_code)) {
    child.Terminate(/*exit_code=*/1, /*wait=*/true);
    FAIL() << "Timed out waiting for the child process.";
  }
  EXPECT_EQ(exit_code, kChildSuccess);
}

TEST(PeerConnectionLandlockPolicyLinuxTest, ResolvConfPathToAllow) {
  struct TestCase {
    const char* resolved_path;
    const char* expected_path;
  };
  const TestCase kTestCases[] = {
      // Allow the directory.
      {"/run/systemd/resolve/stub-resolv.conf", "/run/systemd/resolve"},
      {"/run/NetworkManager/resolv.conf", "/run/NetworkManager"},
      {"/etc/resolv.conf", "/etc"},
      // Too broad: only allow the file.
      {"/resolv.conf", "/resolv.conf"},
      {"/run/resolv.conf", "/run/resolv.conf"},
      {"/var/run/resolv.conf", "/var/run/resolv.conf"},
      {"/run/user/resolv.conf", "/run/user/resolv.conf"},
      {"/run/user/1000/resolv.conf", "/run/user/1000/resolv.conf"},
      {"/tmp/resolv.conf", "/tmp/resolv.conf"},
      {"/tmp/dir/resolv.conf", "/tmp/dir/resolv.conf"},
      {"/proc/1/root/resolv.conf", "/proc/1/root/resolv.conf"},
  };
  for (const TestCase& test_case : kTestCases) {
    EXPECT_EQ(GetResolvConfPathToAllowForTesting(
                  base::FilePath(test_case.resolved_path)),
              base::FilePath(test_case.expected_path))
        << test_case.resolved_path;
  }
}

}  // namespace
}  // namespace remoting
