// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/services/namespace_sandbox.h"

#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/test/multiprocess_test.h"
#include "sandbox/linux/services/credentials.h"
#include "sandbox/linux/services/namespace_utils.h"
#include "sandbox/linux/services/proc_util.h"
#include "sandbox/linux/tests/unit_tests.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace sandbox {

namespace {

bool RootDirectoryIsEmpty() {
  base::FilePath root("/");
  int file_type =
      base::FileEnumerator::DIRECTORIES | base::FileEnumerator::FILES;
  base::FileEnumerator enumerator_before(root, false, file_type);
  return enumerator_before.Next().empty();
}

class NamespaceSandboxTest : public base::MultiProcessTest {
 public:
  void TestProc(const std::string& procname) {
    TestProcWithOptions(procname, NamespaceSandbox::Options());
  }

  void TestProcWithOptions(
      const std::string& procname,
      const NamespaceSandbox::Options& ns_sandbox_options) {
    if (!Credentials::CanCreateProcessInNewUserNS()) {
      return;
    }

    base::LaunchOptions launch_options;
    launch_options.fds_to_remap.push_back(
        std::make_pair(STDOUT_FILENO, STDOUT_FILENO));
    launch_options.fds_to_remap.push_back(
        std::make_pair(STDERR_FILENO, STDERR_FILENO));

    base::Process process = NamespaceSandbox::LaunchProcessWithOptions(
        MakeCmdLine(procname), launch_options, ns_sandbox_options);
    ASSERT_TRUE(process.IsValid());

    const int kDummyExitCode = 42;
    int exit_code = kDummyExitCode;
    EXPECT_TRUE(process.WaitForExit(&exit_code));
    EXPECT_EQ(0, exit_code);
  }
};

MULTIPROCESS_TEST_MAIN(SimpleChildProcess) {
  const bool in_user_ns = NamespaceSandbox::InNewUserNamespace();
  const bool in_pid_ns = NamespaceSandbox::InNewPidNamespace();
  const bool in_net_ns = NamespaceSandbox::InNewNetNamespace();
  CHECK(in_user_ns);
  CHECK_EQ(in_pid_ns,
           NamespaceUtils::KernelSupportsUnprivilegedNamespace(CLONE_NEWPID));
  CHECK_EQ(in_net_ns,
           NamespaceUtils::KernelSupportsUnprivilegedNamespace(CLONE_NEWNET));
  if (in_pid_ns) {
    CHECK_EQ(1, getpid());
  }
  return 0;
}

TEST_F(NamespaceSandboxTest, BasicUsage) {
  TestProc("SimpleChildProcess");
}

MULTIPROCESS_TEST_MAIN(PidNsOnlyChildProcess) {
  const bool in_user_ns = NamespaceSandbox::InNewUserNamespace();
  const bool in_pid_ns = NamespaceSandbox::InNewPidNamespace();
  const bool in_net_ns = NamespaceSandbox::InNewNetNamespace();
  CHECK(in_user_ns);
  CHECK_EQ(in_pid_ns,
           NamespaceUtils::KernelSupportsUnprivilegedNamespace(CLONE_NEWPID));
  CHECK(!in_net_ns);
  if (in_pid_ns) {
    CHECK_EQ(1, getpid());
  }
  return 0;
}


TEST_F(NamespaceSandboxTest, BasicUsageWithOptions) {
  NamespaceSandbox::Options options;
  options.ns_types = CLONE_NEWUSER | CLONE_NEWPID;
  TestProcWithOptions("PidNsOnlyChildProcess", options);
}

MULTIPROCESS_TEST_MAIN(ChrootMe) {
  CHECK(!RootDirectoryIsEmpty());
  CHECK(sandbox::Credentials::MoveToNewUserNS());
  CHECK(sandbox::Credentials::DropFileSystemAccess(ProcUtil::OpenProc().get()));
  CHECK(RootDirectoryIsEmpty());
  return 0;
}

// Temporarily disabled on ASAN due to crbug.com/451603.
// Disabled on MSAN due to crbug.com/1180105
TEST_F(NamespaceSandboxTest, DISABLE_ON_SANITIZERS(ChrootAndDropCapabilities)) {
  TestProc("ChrootMe");
}

MULTIPROCESS_TEST_MAIN(NestedNamespaceSandbox) {
  base::LaunchOptions launch_options;
  launch_options.fds_to_remap.push_back(
      std::make_pair(STDOUT_FILENO, STDOUT_FILENO));
  launch_options.fds_to_remap.push_back(
      std::make_pair(STDERR_FILENO, STDERR_FILENO));

  base::Process process = NamespaceSandbox::LaunchProcess(
      base::CommandLine(base::FilePath("/bin/true")), launch_options);
  CHECK(process.IsValid());

  const int kDummyExitCode = 42;
  int exit_code = kDummyExitCode;
  CHECK(process.WaitForExit(&exit_code));
  CHECK_EQ(0, exit_code);
  return 0;
}

TEST_F(NamespaceSandboxTest, NestedNamespaceSandbox) {
  TestProc("NestedNamespaceSandbox");
}

const int kNormalExitCode = 0;

// Ensure that CHECK(false) is distinguishable from _exit(kNormalExitCode).
// Allowing noise since CHECK(false) will write a stack trace to stderr.
SANDBOX_TEST_ALLOW_NOISE(ForkInNewPidNamespace, CheckDoesNotReturnZero) {
  if (!Credentials::CanCreateProcessInNewUserNS()) {
    return;
  }

  CHECK(sandbox::Credentials::MoveToNewUserNS());
  const pid_t pid = NamespaceSandbox::ForkInNewPidNamespace(
      /*drop_capabilities_in_child=*/true);
  CHECK_GE(pid, 0);

  if (pid == 0) {
    CHECK(false);
    _exit(kNormalExitCode);
  }

  int status;
  PCHECK(waitpid(pid, &status, 0) == pid);
  if (WIFEXITED(status)) {
    CHECK_NE(kNormalExitCode, WEXITSTATUS(status));
  }
}

SANDBOX_TEST(ForkInNewPidNamespace, BasicUsage) {
  if (!Credentials::CanCreateProcessInNewUserNS()) {
    return;
  }

  CHECK(sandbox::Credentials::MoveToNewUserNS());
  const pid_t pid = NamespaceSandbox::ForkInNewPidNamespace(
      /*drop_capabilities_in_child=*/true);
  CHECK_GE(pid, 0);

  if (pid == 0) {
    CHECK_EQ(1, getpid());
    CHECK(!Credentials::HasAnyCapability());
    _exit(kNormalExitCode);
  }

  int status;
  PCHECK(waitpid(pid, &status, 0) == pid);
  CHECK(WIFEXITED(status));
  CHECK_EQ(kNormalExitCode, WEXITSTATUS(status));
}

SANDBOX_TEST(ForkInNewPidNamespace, ExitWithSignal) {
  if (!Credentials::CanCreateProcessInNewUserNS()) {
    return;
  }

  CHECK(sandbox::Credentials::MoveToNewUserNS());
  const pid_t pid = NamespaceSandbox::ForkInNewPidNamespace(
      /*drop_capabilities_in_child=*/true);
  CHECK_GE(pid, 0);

  if (pid == 0) {
    CHECK_EQ(1, getpid());
    CHECK(!Credentials::HasAnyCapability());
    CHECK(NamespaceSandbox::InstallTerminationSignalHandler(
        SIGTERM, NamespaceSandbox::SignalExitCode(SIGTERM)));
    while (true) {
      raise(SIGTERM);
    }
  }

  int status;
  PCHECK(waitpid(pid, &status, 0) == pid);
  CHECK(WIFEXITED(status));
  CHECK_EQ(NamespaceSandbox::SignalExitCode(SIGTERM), WEXITSTATUS(status));
}

volatile sig_atomic_t signal_handler_called;
void ExitSuccessfully(int sig) {
  signal_handler_called = 1;
}

SANDBOX_TEST(InstallTerminationSignalHandler, DoesNotOverrideExistingHandlers) {
  struct sigaction action = {};
  action.sa_handler = &ExitSuccessfully;
  PCHECK(sigaction(SIGUSR1, &action, nullptr) == 0);

  NamespaceSandbox::InstallDefaultTerminationSignalHandlers();
  CHECK(!NamespaceSandbox::InstallTerminationSignalHandler(
            SIGUSR1, NamespaceSandbox::SignalExitCode(SIGUSR1)));

  raise(SIGUSR1);
  CHECK_EQ(1, signal_handler_called);
}

}  // namespace

}  // namespace sandbox
