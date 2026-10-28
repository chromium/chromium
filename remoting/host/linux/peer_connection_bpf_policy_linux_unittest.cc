// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/peer_connection_bpf_policy_linux.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <memory>
#include <optional>
#include <string>

#include "base/command_line.h"
#include "base/process/process.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl_impl.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace remoting {
namespace {

using sandbox::bpf_dsl::ResultExpr;

struct SyscallTestParam {
  const char* name;
  int sysno;
};

// -----------------------------------------------------------------------------
// Denied Syscalls Test Suite
// -----------------------------------------------------------------------------

class PeerConnectionBpfDeniedSyscallsTest
    : public testing::TestWithParam<SyscallTestParam> {};

TEST_P(PeerConnectionBpfDeniedSyscallsTest, IsDenied) {
  PeerConnectionBpfPolicyLinux policy(getpid());
  ResultExpr expr = policy.EvaluateSyscall(GetParam().sysno);
  ASSERT_TRUE(expr);
  EXPECT_TRUE(expr->IsDeny())
      << "Syscall " << GetParam().name << " was expected to be denied.";
  EXPECT_FALSE(expr->IsAllow());
}

INSTANTIATE_TEST_SUITE_P(
    PeerConnectionBpfPolicyLinuxTest,
    PeerConnectionBpfDeniedSyscallsTest,
    testing::Values(
#if defined(__NR_execveat)
        SyscallTestParam{"execveat", __NR_execveat},
#endif
#if defined(__NR_fork)
        SyscallTestParam{"fork", __NR_fork},
#endif
#if defined(__NR_vfork)
        SyscallTestParam{"vfork", __NR_vfork},
#endif
#if defined(__NR_clone3)
        SyscallTestParam{"clone3", __NR_clone3},
#endif
#if defined(__NR_unshare)
        SyscallTestParam{"unshare", __NR_unshare},
#endif
#if defined(__NR_setns)
        SyscallTestParam{"setns", __NR_setns},
#endif
#if defined(__NR_mkdir)
        SyscallTestParam{"mkdir", __NR_mkdir},
#endif
#if defined(__NR_chmod)
        SyscallTestParam{"chmod", __NR_chmod},
#endif
#if defined(__NR_chown)
        SyscallTestParam{"chown", __NR_chown},
#endif
#if defined(__NR_shmget)
        SyscallTestParam{"shmget", __NR_shmget},
#endif
#if defined(__NR_msgget)
        SyscallTestParam{"msgget", __NR_msgget},
#endif
#if defined(__NR_semget)
        SyscallTestParam{"semget", __NR_semget},
#endif
        SyscallTestParam{"execve", __NR_execve},
        SyscallTestParam{"ptrace", __NR_ptrace},
        SyscallTestParam{"process_vm_writev", __NR_process_vm_writev},
        SyscallTestParam{"mount", __NR_mount},
        SyscallTestParam{"chroot", __NR_chroot},
        SyscallTestParam{"pidfd_open", __NR_pidfd_open},
        SyscallTestParam{"pidfd_getfd", __NR_pidfd_getfd},
        SyscallTestParam{"pidfd_send_signal", __NR_pidfd_send_signal},
        SyscallTestParam{"process_madvise", __NR_process_madvise},
        SyscallTestParam{"process_mrelease", __NR_process_mrelease},
        SyscallTestParam{"get_robust_list", __NR_get_robust_list},
        SyscallTestParam{"move_pages", __NR_move_pages},
        SyscallTestParam{"migrate_pages", __NR_migrate_pages},
        SyscallTestParam{"add_key", __NR_add_key},
        SyscallTestParam{"keyctl", __NR_keyctl},
        SyscallTestParam{"request_key", __NR_request_key},
        SyscallTestParam{"mq_open", __NR_mq_open},
        SyscallTestParam{"io_uring_setup", __NR_io_uring_setup},
        SyscallTestParam{"io_uring_enter", __NR_io_uring_enter},
        SyscallTestParam{"io_uring_register", __NR_io_uring_register},
        SyscallTestParam{"bpf", __NR_bpf},
        SyscallTestParam{"userfaultfd", __NR_userfaultfd},
        SyscallTestParam{"perf_event_open", __NR_perf_event_open},
        SyscallTestParam{"fanotify_init", __NR_fanotify_init},
        SyscallTestParam{"fchmodat2", __NR_fchmodat2},
#if defined(ARCH_CPU_ARM64)
        // TODO(crbug.com/574906195): Fails on arm64 because __NR_mseal + 1
        // exceeds MAX_PUBLIC_SYSCALL (__NR_syscalls in the sysroot).
        SyscallTestParam{"DISABLED_newer_than_mseal", __NR_mseal + 1}),
#else
        SyscallTestParam{"newer_than_mseal", __NR_mseal + 1}),
#endif
    [](const testing::TestParamInfo<SyscallTestParam>& info) {
      return info.param.name;
    });

// -----------------------------------------------------------------------------
// Allowed Syscalls Test Suite
// -----------------------------------------------------------------------------

class PeerConnectionBpfAllowedSyscallsTest
    : public testing::TestWithParam<SyscallTestParam> {};

TEST_P(PeerConnectionBpfAllowedSyscallsTest, IsAllowed) {
  PeerConnectionBpfPolicyLinux policy(getpid());
  ResultExpr expr = policy.EvaluateSyscall(GetParam().sysno);
  ASSERT_TRUE(expr);
  EXPECT_TRUE(expr->IsAllow())
      << "Syscall " << GetParam().name << " was expected to be allowed.";
  EXPECT_FALSE(expr->IsDeny());
}

INSTANTIATE_TEST_SUITE_P(
    PeerConnectionBpfPolicyLinuxTest,
    PeerConnectionBpfAllowedSyscallsTest,
    testing::Values(
#if defined(__NR_epoll_wait)
        SyscallTestParam{"epoll_wait", __NR_epoll_wait},
#endif
#if defined(__NR_open)
        SyscallTestParam{"open", __NR_open},
#endif
#if defined(__NR_openat)
        SyscallTestParam{"openat", __NR_openat},
#endif
#if defined(__NR_unlink)
        SyscallTestParam{"unlink", __NR_unlink},
#endif
#if defined(__NR_unlinkat)
        SyscallTestParam{"unlinkat", __NR_unlinkat},
#endif
        SyscallTestParam{"read", __NR_read},
        SyscallTestParam{"write", __NR_write},
        SyscallTestParam{"close", __NR_close},
        SyscallTestParam{"futex", __NR_futex},
        SyscallTestParam{"memfd_create", __NR_memfd_create},
        SyscallTestParam{"mseal", __NR_mseal}),
    [](const testing::TestParamInfo<SyscallTestParam>& info) {
      return info.param.name;
    });

// -----------------------------------------------------------------------------
// Conditional Syscalls Test Suite
// -----------------------------------------------------------------------------

class PeerConnectionBpfConditionalSyscallsTest
    : public testing::TestWithParam<SyscallTestParam> {};

TEST_P(PeerConnectionBpfConditionalSyscallsTest, IsConditional) {
  PeerConnectionBpfPolicyLinux policy(getpid());
  ResultExpr expr = policy.EvaluateSyscall(GetParam().sysno);
  ASSERT_TRUE(expr);
  EXPECT_FALSE(expr->IsAllow())
      << "Syscall " << GetParam().name << " is unexpectedly always allowed.";
  EXPECT_FALSE(expr->IsDeny())
      << "Syscall " << GetParam().name << " is unexpectedly always denied.";
}

INSTANTIATE_TEST_SUITE_P(
    PeerConnectionBpfPolicyLinuxTest,
    PeerConnectionBpfConditionalSyscallsTest,
    testing::Values(
#if defined(__NR_fcntl64)
        SyscallTestParam{"fcntl64", __NR_fcntl64},
#endif
        SyscallTestParam{"clone", __NR_clone},
        SyscallTestParam{"socket", __NR_socket},
        SyscallTestParam{"kill", __NR_kill},
        SyscallTestParam{"tgkill", __NR_tgkill},
        SyscallTestParam{"rt_sigqueueinfo", __NR_rt_sigqueueinfo},
        SyscallTestParam{"rt_tgsigqueueinfo", __NR_rt_tgsigqueueinfo},
        SyscallTestParam{"prlimit64", __NR_prlimit64},
        SyscallTestParam{"getpriority", __NR_getpriority},
        SyscallTestParam{"setpriority", __NR_setpriority},
        SyscallTestParam{"ioprio_get", __NR_ioprio_get},
        SyscallTestParam{"ioprio_set", __NR_ioprio_set},
        SyscallTestParam{"sched_getaffinity", __NR_sched_getaffinity},
        SyscallTestParam{"sched_setaffinity", __NR_sched_setaffinity},
        SyscallTestParam{"sched_setscheduler", __NR_sched_setscheduler},
        SyscallTestParam{"fcntl", __NR_fcntl},
        SyscallTestParam{"ioctl", __NR_ioctl},
        SyscallTestParam{"prctl", __NR_prctl}),
    [](const testing::TestParamInfo<SyscallTestParam>& info) {
      return info.param.name;
    });

// -----------------------------------------------------------------------------
// Tests that run the policy in a child process
// -----------------------------------------------------------------------------

// Sanitizers make syscalls (e.g. LSan's leak check at exit) that the policy
// denies or crashes on, so these tests are disabled under sanitizers, similar
// to the tests in //sandbox/linux.
#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER) || \
    defined(THREAD_SANITIZER) || defined(LEAK_SANITIZER) ||    \
    defined(UNDEFINED_SANITIZER)
#define MAYBE_SANDBOX_TEST(test_name) DISABLED_##test_name
#else
#define MAYBE_SANDBOX_TEST(test_name) test_name
#endif

// Exit codes of the sandboxed child processes. Each failed check exits with a
// distinct code to make failures easy to diagnose. The codes are chosen to not
// collide with exit codes commonly used by crash handlers (e.g. 1).
constexpr int kChildSuccess = 0;
constexpr int kChildSandboxFailed = 101;
constexpr int kChildStillAliveAfterKill = 102;
constexpr int kChildCheckFailedBase = 110;

// From <linux/keyctl.h> (KEYCTL_GET_KEYRING_ID and KEY_SPEC_USER_KEYRING).
constexpr int kKeyctlGetKeyringId = 0;
constexpr int kKeySpecUserKeyring = -4;
// From <linux/ioprio.h> (IOPRIO_WHO_PROCESS).
constexpr int kIoprioWhoProcess = 1;

bool IsSandboxSupported() {
  return sandbox::SandboxBPF::SupportsSeccompSandbox(
      sandbox::SandboxBPF::SeccompLevel::MULTI_THREADED);
}

// Exits the child process immediately, without running at-exit handlers, which
// may make syscalls that are denied by the policy.
[[noreturn]] void ExitChild(int exit_code) {
  _exit(exit_code);
}

// Starts the sandbox in the current (multi-threaded) child process, or exits
// the child process if that fails.
void StartSandboxOrExit() {
  sandbox::SandboxBPF sandbox(
      std::make_unique<PeerConnectionBpfPolicyLinux>(getpid()));
  if (!sandbox.StartSandbox(
          sandbox::SandboxBPF::SeccompLevel::MULTI_THREADED)) {
    ExitChild(kChildSandboxFailed);
  }
}

bool FailsWithErrno(long result, int expected_errno) {
  return result == -1 && errno == expected_errno;
}

// Thread function that targets the calling thread by its thread ID. `arg`
// points to a bool that receives the result. A raw pthread is used since the
// check needs a thread other than the main thread in the sandboxed child, which
// has no task environment.
void* RunOwnThreadIdChecks(void* arg) {
  const pid_t tid = static_cast<pid_t>(syscall(__NR_gettid));
  errno = 0;
  const int nice_value = getpriority(PRIO_PROCESS, tid);
  cpu_set_t cpu_set;
  *static_cast<bool*>(arg) =
      errno == 0 && setpriority(PRIO_PROCESS, tid, nice_value) == 0 &&
      sched_getaffinity(tid, sizeof(cpu_set), &cpu_set) == 0 &&
      syscall(__NR_ioprio_get, kIoprioWhoProcess, tid) >= 0;
  return nullptr;
}

MULTIPROCESS_TEST_MAIN(OperationsOnSelfAreAllowed) {
  StartSandboxOrExit();

  const pid_t pid = getpid();
  int check = kChildCheckFailedBase;
  if (kill(pid, 0) != 0) {
    ExitChild(check);
  }
  ++check;
  if (syscall(__NR_tgkill, pid, syscall(__NR_gettid), 0) != 0) {
    ExitChild(check);
  }
  ++check;
  struct rlimit limit;
  if (prlimit(0, RLIMIT_NOFILE, nullptr, &limit) != 0 ||
      prlimit(pid, RLIMIT_NOFILE, nullptr, &limit) != 0) {
    ExitChild(check);
  }
  ++check;
  errno = 0;
  const int nice_value = getpriority(PRIO_PROCESS, 0);
  if (errno != 0 || setpriority(PRIO_PROCESS, 0, nice_value) != 0) {
    ExitChild(check);
  }
  ++check;
  if (prctl(PR_SET_DUMPABLE, 0) != 0) {
    ExitChild(check);
  }
  ++check;
  // A thread may target itself using its own thread ID. The policy rewrites
  // such calls to use 0 instead.
  bool thread_checks_passed = false;
  pthread_t thread;
  if (pthread_create(&thread, nullptr, &RunOwnThreadIdChecks,
                     &thread_checks_passed) != 0 ||
      pthread_join(thread, nullptr) != 0 || !thread_checks_passed) {
    ExitChild(check);
  }
  ExitChild(kChildSuccess);
}

// Each check below would succeed (or fail with a different error) without the
// policy, since the parent process runs as the same user. Read-only operations
// are used where possible, so that a regression has no side effects.
MULTIPROCESS_TEST_MAIN(OperationsOnOtherProcessesAreDenied) {
  StartSandboxOrExit();

  const pid_t other_pid = getppid();
  int check = kChildCheckFailedBase;
  struct rlimit limit;
  if (!FailsWithErrno(prlimit(other_pid, RLIMIT_NOFILE, nullptr, &limit),
                      EPERM)) {
    ExitChild(check);
  }
  ++check;
  if (!FailsWithErrno(getpriority(PRIO_PROCESS, other_pid), EPERM)) {
    ExitChild(check);
  }
  ++check;
  if (!FailsWithErrno(getpriority(PRIO_USER, getuid()), EPERM)) {
    ExitChild(check);
  }
  ++check;
  cpu_set_t cpu_set;
  if (!FailsWithErrno(sched_getaffinity(other_pid, sizeof(cpu_set), &cpu_set),
                      EPERM)) {
    ExitChild(check);
  }
  ++check;
  if (!FailsWithErrno(syscall(__NR_ioprio_get, kIoprioWhoProcess, other_pid),
                      EPERM)) {
    ExitChild(check);
  }
  ++check;
  // SI_QUEUE is allowed by the kernel for a target running as the same user.
  // Signal 0 only checks for permission.
  siginfo_t info = {};
  info.si_code = SI_QUEUE;
  if (!FailsWithErrno(syscall(__NR_rt_sigqueueinfo, other_pid, 0, &info),
                      EPERM)) {
    ExitChild(check);
  }
  ++check;
  int sockets[2];
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0) {
    ExitChild(check);
  }
  ++check;
  if (!FailsWithErrno(fcntl(sockets[0], F_SETOWN, other_pid), EPERM)) {
    ExitChild(check);
  }
  ++check;
  int owner = other_pid;
  if (!FailsWithErrno(ioctl(sockets[0], FIOSETOWN, &owner), EPERM)) {
    ExitChild(check);
  }
  ++check;
  if (!FailsWithErrno(prctl(PR_SET_DUMPABLE, 1), EPERM)) {
    ExitChild(check);
  }
  ++check;
  if (!FailsWithErrno(prctl(PR_SET_PDEATHSIG, 0), EPERM)) {
    ExitChild(check);
  }
  ++check;
  if (!FailsWithErrno(syscall(__NR_pidfd_open, other_pid, 0), ENOSYS)) {
    ExitChild(check);
  }
  ++check;
  if (!FailsWithErrno(syscall(__NR_pidfd_getfd, -1, 0, 0), EPERM)) {
    ExitChild(check);
  }
  ++check;
  if (!FailsWithErrno(
          syscall(__NR_keyctl, kKeyctlGetKeyringId, kKeySpecUserKeyring, 0),
          EPERM)) {
    ExitChild(check);
  }
  ++check;
  // Lookup only. This would fail with ENOENT if it were allowed.
  if (!FailsWithErrno(shmget(0x43524400, 0, 0), EPERM)) {
    ExitChild(check);
  }
  ++check;
  if (!FailsWithErrno(syscall(__NR_io_uring_setup, 1, nullptr), ENOSYS)) {
    ExitChild(check);
  }
  ++check;
  // Note that on kernels without this syscall (< 6.13), the kernel itself
  // returns ENOSYS.
  if (!FailsWithErrno(syscall(__NR_mseal + 1), ENOSYS)) {
    ExitChild(check);
  }
  ExitChild(kChildSuccess);
}

MULTIPROCESS_TEST_MAIN(SignalingOtherProcessCrashes) {
  StartSandboxOrExit();
  // Signal 0 only checks for permission; it is harmless if it is delivered.
  kill(getppid(), 0);
  ExitChild(kChildStillAliveAfterKill);
}

// Runs `child_main` in a child process and returns its exit code, which is -1
// if the child was killed by a signal.
std::optional<int> RunChild(const std::string& child_main) {
  base::Process child = base::SpawnMultiProcessTestChild(
      child_main, base::GetMultiProcessTestChildBaseCommandLine(),
      base::LaunchOptions());
  if (!child.IsValid()) {
    ADD_FAILURE() << "Failed to launch the child process.";
    return std::nullopt;
  }
  int exit_code = 0;
  if (!base::WaitForMultiprocessTestChildExit(
          child, TestTimeouts::action_timeout(), &exit_code)) {
    ADD_FAILURE() << "Timed out waiting for the child process.";
    child.Terminate(/*exit_code=*/1, /*wait=*/true);
    return std::nullopt;
  }
  return exit_code;
}

TEST(PeerConnectionBpfPolicyLinuxTest,
     MAYBE_SANDBOX_TEST(OperationsOnSelfAreAllowed)) {
  if (!IsSandboxSupported()) {
    GTEST_SKIP() << "Seccomp-BPF is not supported.";
  }
  EXPECT_EQ(RunChild("OperationsOnSelfAreAllowed"), kChildSuccess);
}

TEST(PeerConnectionBpfPolicyLinuxTest,
     MAYBE_SANDBOX_TEST(OperationsOnOtherProcessesAreDenied)) {
  if (!IsSandboxSupported()) {
    GTEST_SKIP() << "Seccomp-BPF is not supported.";
  }
  EXPECT_EQ(RunChild("OperationsOnOtherProcessesAreDenied"), kChildSuccess);
}

TEST(PeerConnectionBpfPolicyLinuxTest,
     MAYBE_SANDBOX_TEST(SignalingOtherProcessCrashes)) {
  if (!IsSandboxSupported()) {
    GTEST_SKIP() << "Seccomp-BPF is not supported.";
  }
  // The child is expected to be killed by a signal.
  EXPECT_EQ(RunChild("SignalingOtherProcessCrashes"), -1);
}

}  // namespace
}  // namespace remoting
