// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "base/debug/leak_annotations.h"
#include "base/files/file_util.h"
#include "base/posix/eintr_wrapper.h"
#include "build/build_config.h"
#include "sandbox/linux/tests/unit_tests.h"

// Specifically, PNaCl toolchain does not have this flag.
#if !defined(POLLRDHUP)
#define POLLRDHUP 0x2000
#endif

namespace {
std::string TestFailedMessage(const std::string& msg) {
  return msg.empty() ? std::string() : "Actual test failure: " + msg;
}

int GetSubProcessTimeoutTimeInSeconds() {
#ifdef NDEBUG
  // Chromecast build lab devices need this much time to complete.
  // They only run in release.
  return 30;
#else
  // Want a shorter timeout than test runner to get a useful callstack
  // in debug.
  return 10;
#endif
}

// Returns the number of threads of the current process or -1.
int CountThreads() {
  struct stat task_stat;
  int task_d = stat("/proc/self/task", &task_stat);
  // task_stat.st_nlink should be the number of tasks + 2 (accounting for
  // "." and "..".
  if (task_d != 0 || task_stat.st_nlink < 3)
    return -1;
  const int num_threads = task_stat.st_nlink - 2;
  return num_threads;
}

}  // namespace

namespace sandbox {

bool IsAndroid() {
#if defined(OS_ANDROID)
  return true;
#else
  return false;
#endif
}

bool IsArchitectureArm() {
#if defined(ARCH_CPU_ARM_FAMILY)
  return true;
#else
  return false;
#endif
}

static const int kExpectedValue = 42;
static const int kIgnoreThisTest = 43;
static const int kExitWithAssertionFailure = 1;
#if !defined(OS_NACL_NONSFI)
static const int kExitForTimeout = 2;
#endif

#if defined(SANDBOX_USES_BASE_TEST_SUITE)
// This is due to StackDumpSignalHandler() performing _exit(1).
// TODO(jln): get rid of the collision with kExitWithAssertionFailure.
const int kExitAfterSIGSEGV = 1;
#endif

// PNaCl toolchain's signal ABIs are incompatible with Linux's.
// So, for simplicity, just drop the "timeout" feature from unittest framework
// with relying on the buildbot's timeout feature.
#if !defined(OS_NACL_NONSFI)
static void SigAlrmHandler(int) {
  const char failure_message[] = "Timeout reached!\n";
  // Make sure that we never block here.
  if (!fcntl(2, F_SETFL, O_NONBLOCK)) {
    ignore_result(write(2, failure_message, sizeof(failure_message) - 1));
  }
  _exit(kExitForTimeout);
}

// Set a timeout with a handler that will automatically fail the
// test.
static void SetProcessTimeout(int time_in_seconds) {
  struct sigaction act = {};
  act.sa_handler = SigAlrmHandler;
  SANDBOX_ASSERT(sigemptyset(&act.sa_mask) == 0);
  act.sa_flags = 0;

  struct sigaction old_act;
  SANDBOX_ASSERT(sigaction(SIGALRM, &act, &old_act) == 0);

  // We don't implemenet signal chaining, so make sure that nothing else
  // is expecting to handle SIGALRM.
  SANDBOX_ASSERT((old_act.sa_flags & SA_SIGINFO) == 0);
  SANDBOX_ASSERT(old_act.sa_handler == SIG_DFL);
  sigset_t sigalrm_set;
  SANDBOX_ASSERT(sigemptyset(&sigalrm_set) == 0);
  SANDBOX_ASSERT(sigaddset(&sigalrm_set, SIGALRM) == 0);
  SANDBOX_ASSERT(sigprocmask(SIG_UNBLOCK, &sigalrm_set, NULL) == 0);
  SANDBOX_ASSERT(alarm(time_in_seconds) == 0);  // There should be no previous
                                                // alarm.
}
#endif  // !defined(OS_NACL_NONSFI)

// Runs a test in a sub-process. This is necessary for most of the code
// in the BPF sandbox, as it potentially makes global state changes and as
// it also tends to raise fatal errors, if the code has been used in an
// insecure manner.
void UnitTests::RunTestInProcess(SandboxTestRunner* test_runner,
                                 DeathCheck death,
                                 const void* death_aux) {
  CHECK(test_runner);
  // We need to fork(), so we can't be multi-threaded, as threads could hold
  // locks.
  int num_threads = CountThreads();
  const int kNumExpectedThreads = 1;

  // The kernel is at liberty to wake a thread id futex before updating /proc.
  // If another test running in the same process has stopped a thread, it may
  // appear as still running in /proc.
  // We poll /proc, with an exponential back-off. At most, we'll sleep around
  // 2^iterations nanoseconds in nanosleep().
  for (unsigned int iteration = 0; iteration < 30; iteration++) {
    struct timespec ts = {0, 1L << iteration /* nanoseconds */};
    PCHECK(0 == HANDLE_EINTR(nanosleep(&ts, &ts)));
    num_threads = CountThreads();
    if (kNumExpectedThreads == num_threads)
      break;
  }

  ASSERT_EQ(kNumExpectedThreads, num_threads)
      << "Running sandbox tests with multiple threads "
      << "is not supported and will make the tests flaky.";
  int fds[2];
  ASSERT_EQ(0, pipe(fds));
  // Check that our pipe is not on one of the standard file descriptor.
  SANDBOX_ASSERT(fds[0] > 2 && fds[1] > 2);

  pid_t pid;
  ASSERT_LE(0, (pid = fork()));
  if (!pid) {
    // In child process
    // Redirect stderr to our pipe. This way, we can capture all error
    // messages, if we decide we want to do so in our tests.
    SANDBOX_ASSERT(dup2(fds[1], 2) == 2);
    SANDBOX_ASSERT(!close(fds[0]));
    SANDBOX_ASSERT(!close(fds[1]));

#if !defined(OS_NACL_NONSFI)
    SetProcessTimeout(GetSubProcessTimeoutTimeInSeconds());
#endif

    // Disable core files. They are not very useful for our individual test
    // cases.
    struct rlimit no_core = {0};
    setrlimit(RLIMIT_CORE, &no_core);

#if defined(OS_ANDROID)
    // On Android Oreo and higher, the system applies a seccomp filter to all
    // processes. It has its own SIGSYS handler that is un-hooked here in the
    // test child process, so that the Chromium handler can be used. This
    // is performed by SeccompStarterAndroid in normal builds.
    signal(SIGSYS, SIG_DFL);
    // In addition, libsigchain will install a SEGV handler that is normally
    // used for JVM fault handling. Reset it so that the test SEGV failures
    // are interpreted correctly.
    signal(SIGSEGV, SIG_DFL);
#endif

    test_runner->Run();
    if (test_runner->ShouldCheckForLeaks()) {
#if defined(LEAK_SANITIZER)
      __lsan_do_leak_check();
#endif
    }
    _exit(kExpectedValue);
  }

  close(fds[1]);
  std::vector<char> msg_buf;
  ssize_t rc;

  // Make sure read() will never block as we'll use poll() to
  // block with a timeout instead.
  const int fcntl_ret = fcntl(fds[0], F_SETFL, O_NONBLOCK);
  ASSERT_EQ(0, fcntl_ret);
  struct pollfd poll_fd = {fds[0], POLLIN | POLLRDHUP, 0};

  int poll_ret;
  // We prefer the SIGALRM timeout to trigger in the child than this timeout
  // so we double the common value here.
  int poll_timeout = GetSubProcessTimeoutTimeInSeconds() * 2 * 1000;
  while ((poll_ret = poll(&poll_fd, 1, poll_timeout) > 0)) {
    const size_t kCapacity = 256;
    const size_t len = msg_buf.size();
    msg_buf.resize(len + kCapacity);
    rc = HANDLE_EINTR(read(fds[0], &msg_buf[len], kCapacity));
    msg_buf.resize(len + std::max(rc, static_cast<ssize_t>(0)));
    if (rc <= 0)
      break;
  }
  ASSERT_NE(poll_ret, -1) << "poll() failed";
  ASSERT_NE(poll_ret, 0) << "Timeout while reading child state";
  close(fds[0]);
  std::string msg(msg_buf.begin(), msg_buf.end());

  int status = 0;
  int waitpid_returned = HANDLE_EINTR(waitpid(pid, &status, 0));
  ASSERT_EQ(pid, waitpid_returned) << TestFailedMessage(msg);

  // At run-time, we sometimes decide that a test shouldn't actually
  // run (e.g. when testing sandbox features on a kernel that doesn't
  // have sandboxing support). When that happens, don't attempt to
  // call the "death" function, as it might be looking for a
  // death-test condition that would never have triggered.
  if (!WIFEXITED(status) || WEXITSTATUS(status) != kIgnoreThisTest ||
      !msg.empty()) {
    // We use gtest's ASSERT_XXX() macros instead of the DeathCheck
    // functions.  This means, on failure, "return" is called. This
    // only works correctly, if the call of the "death" callback is
    // the very last thing in our function.
    death(status, msg, death_aux);
  }
}

void UnitTests::DeathSuccess(int status, const std::string& msg, const void*) {
  std::string details(TestFailedMessage(msg));

  bool subprocess_terminated_normally = WIFEXITED(status);
  ASSERT_TRUE(subprocess_terminated_normally) << details;
  int subprocess_exit_status = WEXITSTATUS(status);
  ASSERT_EQ(kExpectedValue, subprocess_exit_status) << details;
#if !defined(LEAK_SANITIZER)
  // LSan may print warnings to stdout, breaking this expectation.
  bool subprocess_exited_but_printed_messages = !msg.empty();
  EXPECT_FALSE(subprocess_exited_but_printed_messages) << details;
#endif
}

void UnitTests::DeathSuccessAllowNoise(int status,
                                       const std::string& msg,
                                       const void*) {
  std::string details(TestFailedMessage(msg));

  bool subprocess_terminated_normally = WIFEXITED(status);
  ASSERT_TRUE(subprocess_terminated_normally) << details;
  int subprocess_exit_status = WEXITSTATUS(status);
  ASSERT_EQ(kExpectedValue, subprocess_exit_status) << details;
}

void UnitTests::DeathMessage(int status,
                             const std::string& msg,
                             const void* aux) {
  std::string details(TestFailedMessage(msg));
  const char* expected_msg = static_cast<const char*>(aux);

  bool subprocess_terminated_normally = WIFEXITED(status);
  ASSERT_TRUE(subprocess_terminated_normally) << "Exit status: " << status
                                              << " " << details;
  int subprocess_exit_status = WEXITSTATUS(status);
  ASSERT_EQ(1, subprocess_exit_status) << details;

  bool subprocess_exited_without_matching_message =
      msg.find(expected_msg) == std::string::npos;

// In official builds CHECK messages are dropped, look for SIGABRT or SIGTRAP.
// See https://crbug.com/437312 and https://crbug.com/612507.
#if defined(OFFICIAL_BUILD) && defined(NDEBUG) && !defined(OS_ANDROID)
  if (subprocess_exited_without_matching_message) {
    static const char kSigTrapMessage[] = "Received signal 5";
    static const char kSigAbortMessage[] = "Received signal 6";
    subprocess_exited_without_matching_message =
        msg.find(kSigTrapMessage) == std::string::npos &&
        msg.find(kSigAbortMessage) == std::string::npos;
  }
#endif
  EXPECT_FALSE(subprocess_exited_without_matching_message) << details;
}

void UnitTests::DeathSEGVMessage(int status,
                                 const std::string& msg,
                                 const void* aux) {
  std::string details(TestFailedMessage(msg));
  const char* expected_msg = static_cast<const char*>(aux);

#if !defined(SANDBOX_USES_BASE_TEST_SUITE)
  const bool subprocess_got_sigsegv =
      WIFSIGNALED(status) && (SIGSEGV == WTERMSIG(status));
#else
  // This hack is required when a signal handler is installed
  // for SEGV that will _exit(1).
  const bool subprocess_got_sigsegv =
      WIFEXITED(status) && (kExitAfterSIGSEGV == WEXITSTATUS(status));
#endif

  ASSERT_TRUE(subprocess_got_sigsegv) << "Exit status: " << status
                                      << " " << details;

  bool subprocess_exited_without_matching_message =
      msg.find(expected_msg) == std::string::npos;
  EXPECT_FALSE(subprocess_exited_without_matching_message) << details;
}

void UnitTests::DeathExitCode(int status,
                              const std::string& msg,
                              const void* aux) {
  int expected_exit_code = static_cast<int>(reinterpret_cast<intptr_t>(aux));
  std::string details(TestFailedMessage(msg));

  bool subprocess_terminated_normally = WIFEXITED(status);
  ASSERT_TRUE(subprocess_terminated_normally) << details;
  int subprocess_exit_status = WEXITSTATUS(status);
  ASSERT_EQ(expected_exit_code, subprocess_exit_status) << details;
}

void UnitTests::DeathBySignal(int status,
                              const std::string& msg,
                              const void* aux) {
  int expected_signo = static_cast<int>(reinterpret_cast<intptr_t>(aux));
  std::string details(TestFailedMessage(msg));

  bool subprocess_terminated_by_signal = WIFSIGNALED(status);
  ASSERT_TRUE(subprocess_terminated_by_signal) << details;
  int subprocess_signal_number = WTERMSIG(status);
  ASSERT_EQ(expected_signo, subprocess_signal_number) << details;
}

void UnitTests::AssertionFailure(const char* expr, const char* file, int line) {
  fprintf(stderr, "%s:%d:%s", file, line, expr);
  fflush(stderr);
  _exit(kExitWithAssertionFailure);
}

void UnitTests::IgnoreThisTest() {
  fflush(stderr);
  _exit(kIgnoreThisTest);
}

}  // namespace sandbox
