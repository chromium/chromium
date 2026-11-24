// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/mac/sandbox_mac.h"

#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <libproc.h>
#include <pthread.h>
#include <servers/bootstrap.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <unistd.h>

#include <string>
#include <string_view>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/process/process.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "remoting/host/base/switches.h"
#include "sandbox/mac/seatbelt.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace remoting::mac {
namespace {

constexpr char kSandboxTestReadFileSwitch[] = "sandbox-test-read-file";
constexpr char kSandboxTestWriteFileSwitch[] = "sandbox-test-write-file";

void VerifyCommonSandboxBehavior(std::string_view process_type) {
  CHECK(!sandbox::Seatbelt::IsSandboxed());

  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  base::FilePath read_file_path =
      cmd_line->GetSwitchValuePath(kSandboxTestReadFileSwitch);
  base::FilePath write_file_path =
      cmd_line->GetSwitchValuePath(kSandboxTestWriteFileSwitch);
  CHECK(!read_file_path.empty());
  CHECK(!write_file_path.empty());

  DropRootPrivilegesAndAcquireBootstrapPort();
  EnterSeatbeltSandbox(process_type);
  CHECK(sandbox::Seatbelt::IsSandboxed());

  // Self-operations must remain permitted.
  PCHECK(kill(getpid(), 0) == 0);
  CHECK_EQ(pthread_setname_np("sandbox_test"), 0);

  struct proc_bsdinfo bsd_info = {};
  CHECK_EQ(
      proc_pidinfo(getpid(), PROC_PIDTBSDINFO, 0, &bsd_info, sizeof(bsd_info)),
      static_cast<int>(sizeof(bsd_info)));

  // Cross-process signals and process inspection must be denied.
  pid_t parent_pid = getppid();
  if (parent_pid > 1) {
    errno = 0;
    CHECK_EQ(kill(parent_pid, 0), -1);
    CHECK_EQ(errno, EPERM);

    errno = 0;
    struct proc_bsdinfo parent_info = {};
    CHECK_LE(proc_pidinfo(parent_pid, PROC_PIDTBSDINFO, 0, &parent_info,
                          sizeof(parent_info)),
             0);
  }

  // Reading or writing files outside the bundle/system allowlists must fail
  // with EPERM.
  errno = 0;
  int read_fd = open(read_file_path.value().c_str(), O_RDONLY);
  CHECK_EQ(read_fd, -1);
  CHECK_EQ(errno, EPERM);

  errno = 0;
  int write_fd =
      open(write_file_path.value().c_str(), O_CREAT | O_WRONLY, 0600);
  CHECK_EQ(write_fd, -1);
  CHECK_EQ(errno, EPERM);

  // Reading the root host config file must be denied (EPERM or ENOENT if absent
  // on the test machine, never readable).
  int config_fd = open(
      "/Library/PrivilegedHelperTools/org.chromium.chromoting.json", O_RDONLY);
  if (config_fd >= 0) {
    close(config_fd);
    CHECK(false) << "Root config file must not be readable inside sandbox";
  }

  // Subprocess creation must be denied.
  errno = 0;
  pid_t fork_pid = fork();
  if (fork_pid == 0) {
    _exit(1);
  }
  CHECK_EQ(fork_pid, -1);
  CHECK_EQ(errno, EPERM);

  // Unrelated sysctls must be denied.
  struct timeval boottime = {};
  size_t boottime_size = sizeof(boottime);
  errno = 0;
  CHECK_EQ(sysctlbyname("kern.boottime", &boottime, &boottime_size, nullptr, 0),
           -1);
  CHECK_EQ(errno, EPERM);

  // UDP sockets and interface enumeration (WebRTC getifaddrs) must be allowed
  // for both network and peer_connection.
  int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
  PCHECK(sock_fd >= 0);
  close(sock_fd);

  struct ifaddrs* ifaddrs_list = nullptr;
  PCHECK(getifaddrs(&ifaddrs_list) == 0);
  freeifaddrs(ifaddrs_list);

  // DNS / hosts files must be readable.
  int hosts_fd = open("/etc/hosts", O_RDONLY);
  PCHECK(hosts_fd >= 0);
  close(hosts_fd);

  // Arbitrary security daemon Mach lookup must be denied in both profiles.
  mach_port_t security_port = MACH_PORT_NULL;
  kern_return_t security_kr = bootstrap_look_up(
      bootstrap_port, "com.apple.SecurityServer", &security_port);
  CHECK_NE(security_kr, KERN_SUCCESS);
}

MULTIPROCESS_TEST_MAIN(SandboxMacNetworkChild) {
  VerifyCommonSandboxBehavior(kProcessTypeNetwork);
  return 0;
}

MULTIPROCESS_TEST_MAIN(SandboxMacPeerConnectionChild) {
  VerifyCommonSandboxBehavior(kProcessTypePeerConnection);

  // peer_connection must not be able to look up trustd.
  mach_port_t trustd_port = MACH_PORT_NULL;
  kern_return_t kr =
      bootstrap_look_up(bootstrap_port, "com.apple.trustd.agent", &trustd_port);
  CHECK_NE(kr, KERN_SUCCESS);
  return 0;
}

class SandboxMacTest : public base::MultiProcessTest {
 protected:
  void RunSandboxChild(const std::string& child_main) {
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

    base::FilePath read_file = temp_dir.GetPath().Append("secret.txt");
    ASSERT_TRUE(base::WriteFile(read_file, "secret"));
    base::FilePath write_file = temp_dir.GetPath().Append("output.txt");

    base::CommandLine child_cmd =
        base::GetMultiProcessTestChildBaseCommandLine();
    child_cmd.AppendSwitchPath(kSandboxTestReadFileSwitch, read_file);
    child_cmd.AppendSwitchPath(kSandboxTestWriteFileSwitch, write_file);

    base::Process child = base::SpawnMultiProcessTestChild(
        child_main, child_cmd, base::LaunchOptions());
    ASSERT_TRUE(child.IsValid());

    int exit_code = -1;
    EXPECT_TRUE(child.WaitForExitWithTimeout(TestTimeouts::action_max_timeout(),
                                             &exit_code));
    EXPECT_EQ(exit_code, 0);
    EXPECT_FALSE(base::PathExists(write_file));
  }
};

TEST_F(SandboxMacTest, NetworkSandboxProfile) {
  RunSandboxChild("SandboxMacNetworkChild");
}

TEST_F(SandboxMacTest, PeerConnectionSandboxProfile) {
  RunSandboxChild("SandboxMacPeerConnectionChild");
}

}  // namespace
}  // namespace remoting::mac
