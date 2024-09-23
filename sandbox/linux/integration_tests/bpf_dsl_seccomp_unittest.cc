// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/prctl.h>
#include <sys/ptrace.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <memory>
#include <vector>

#include "base/containers/adapters.h"
#include "base/containers/contains.h"

#if defined(ANDROID)
// Work-around for buggy headers in Android's NDK
#define __user
#endif
#include <linux/futex.h>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/posix/eintr_wrapper.h"
#include "base/synchronization/waitable_event.h"
#include "base/system/sys_info.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/bpf_dsl/errorcode.h"
#include "sandbox/linux/bpf_dsl/linux_syscall_ranges.h"
#include "sandbox/linux/bpf_dsl/policy.h"
#include "sandbox/linux/bpf_dsl/seccomp_macros.h"
#include "sandbox/linux/seccomp-bpf/bpf_tests.h"
#include "sandbox/linux/seccomp-bpf/die.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/seccomp-bpf/syscall.h"
#include "sandbox/linux/seccomp-bpf/trap.h"
#include "sandbox/linux/services/syscall_wrappers.h"
#include "sandbox/linux/services/thread_helpers.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"
#include "sandbox/linux/tests/scoped_temporary_file.h"
#include "sandbox/linux/tests/unit_tests.h"
#include "testing/gtest/include/gtest/gtest.h"

// Workaround for Android's prctl.h file.
#ifndef PR_GET_ENDIAN
#define PR_GET_ENDIAN 19
#endif
#ifndef PR_CAPBSET_READ
#define PR_CAPBSET_READ 23
#define PR_CAPBSET_DROP 24
#endif

namespace sandbox {
namespace bpf_dsl {

namespace {

const int kExpectedReturnValue = 42;
const char kSandboxDebuggingEnv[] = "CHROME_SANDBOX_DEBUGGING";

// Set the global environment to allow the use of UnsafeTrap() policies.
void EnableUnsafeTraps() {
  // The use of UnsafeTrap() causes us to print a warning message. This is
  // generally desirable, but it results in the unittest failing, as it doesn't
  // expect any messages on "stderr". So, temporarily disable messages. The
  // BPF_TEST() is guaranteed to turn messages back on, after the policy
  // function has completed.
  setenv(kSandboxDebuggingEnv, "t", 0);
  Die::SuppressInfoMessages(true);
}

// BPF_TEST does a lot of the boiler-plate code around setting up a
// policy and optional passing data between the caller, the policy and
// any Trap() handlers. This is great for writing short and concise tests,
// and it helps us accidentally forgetting any of the crucial steps in
// setting up the sandbox. But it wouldn't hurt to have at least one test
// that explicitly walks through all these steps.

intptr_t IncreaseCounter(const struct arch_seccomp_data& args, void* aux) {
  BPF_ASSERT(aux);
  int* counter = static_cast<int*>(aux);
  return (*counter)++;
}

class VerboseAPITestingPolicy : public Policy {
 public:
  explicit VerboseAPITestingPolicy(int* counter_ptr)
      : counter_ptr_(counter_ptr) {}

  VerboseAPITestingPolicy(const VerboseAPITestingPolicy&) = delete;
  VerboseAPITestingPolicy& operator=(const VerboseAPITestingPolicy&) = delete;

  ~VerboseAPITestingPolicy() override = default;

  ResultExpr EvaluateSyscall(int sysno) const override {
    DCHECK(SandboxBPF::IsValidSyscallNumber(sysno));
    if (sysno == __NR_uname) {
      return Trap(IncreaseCounter, counter_ptr_);
    }
    return Allow();
  }

 private:
  const raw_ptr<int> counter_ptr_;
};

SANDBOX_TEST(SandboxBPF, DISABLE_ON_TSAN(VerboseAPITesting)) {
  if (SandboxBPF::SupportsSeccompSandbox(
          SandboxBPF::SeccompLevel::SINGLE_THREADED)) {
    static int counter = 0;
    SandboxBPF sandbox(std::make_unique<VerboseAPITestingPolicy>(&counter));
    BPF_ASSERT(sandbox.StartSandbox(SandboxBPF::SeccompLevel::SINGLE_THREADED));
    BPF_ASSERT_EQ(0, counter);
    BPF_ASSERT_EQ(0, syscall(__NR_uname, 0));
    BPF_ASSERT_EQ(1, counter);
    BPF_ASSERT_EQ(1, syscall(__NR_uname, 0));
    BPF_ASSERT_EQ(2, counter);
  }
}

// A simple denylist test

class DenylistNanosleepPolicy : public Policy {
 public:
  DenylistNanosleepPolicy() = default;

  DenylistNanosleepPolicy(const DenylistNanosleepPolicy&) = delete;
  DenylistNanosleepPolicy& operator=(const DenylistNanosleepPolicy&) = delete;

  ~DenylistNanosleepPolicy() override = default;

  ResultExpr EvaluateSyscall(int sysno) const override {
    DCHECK(SandboxBPF::IsValidSyscallNumber(sysno));
    switch (sysno) {
      case __NR_nanosleep:
        return Error(EACCES);
      default:
        return Allow();
    }
  }

  static void AssertNanosleepFails() {
    const struct timespec ts = {0, 0};
    errno = 0;
    BPF_ASSERT_EQ(-1, HANDLE_EINTR(syscall(__NR_nanosleep, &ts, NULL)));
    BPF_ASSERT_EQ(EACCES, errno);
  }
};

BPF_TEST_C(SandboxBPF, ApplyBasicDenylistPolicy, DenylistNanosleepPolicy) {
  DenylistNanosleepPolicy::AssertNanosleepFails();
}

BPF_TEST_C(SandboxBPF, UseVsyscall, DenylistNanosleepPolicy) {
  time_t current_time;
  // time() is implemented as a vsyscall. With an older glibc, with
  // vsyscall=emulate and some versions of the seccomp BPF patch
  // we may get SIGKILL-ed. Detect this!
  BPF_ASSERT_NE(static_cast<time_t>(-1), time(&current_time));
}

bool IsSyscallForTestHarness(int sysno) {
  if (sysno == __NR_exit_group || sysno == __NR_write) {
    // exit_group is special and we really need it to work.
    // write() is needed for BPF_ASSERT() to report a useful error message.
    return true;
  }
#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER) || \
    defined(UNDEFINED_SANITIZER)
  // UBSan_vptr checker needs mmap, munmap, pipe, write.
  // ASan and MSan don't need any of these for normal operation, but they
  // require at least mmap & munmap to print a report if an error is detected.
  // ASan requires sigaltstack.
  if (sysno == kMMapNr || sysno == __NR_munmap ||
#if !defined(__aarch64__)
      sysno == __NR_pipe ||
#endif
      sysno == __NR_pipe2 || sysno == __NR_sigaltstack) {
    return true;
  }
#endif
  return false;
}

// Now do a simple allowlist test

class AllowlistGetpidPolicy : public Policy {
 public:
  AllowlistGetpidPolicy() = default;

  AllowlistGetpidPolicy(const AllowlistGetpidPolicy&) = delete;
  AllowlistGetpidPolicy& operator=(const AllowlistGetpidPolicy&) = delete;

  ~AllowlistGetpidPolicy() override = default;

  ResultExpr EvaluateSyscall(int sysno) const override {
    DCHECK(SandboxBPF::IsValidSyscallNumber(sysno));
    if (IsSyscallForTestHarness(sysno) || sysno == __NR_getpid) {
      return Allow();
    }
    return Error(ENOMEM);
  }
};

BPF_TEST_C(SandboxBPF, ApplyBasicAllowlistPolicy, AllowlistGetpidPolicy) {
  // getpid() should be allowed
  errno = 0;
  BPF_ASSERT(sys_getpid() > 0);
  BPF_ASSERT(errno == 0);

  // getpgid() should be denied
  BPF_ASSERT(getpgid(0) == -1);
  BPF_ASSERT(errno == ENOMEM);
}

// A simple denylist policy, with a SIGSYS handler
intptr_t EnomemHandler(const struct arch_seccomp_data& args, void* aux) {
  // We also check that the auxiliary data is correct
  SANDBOX_ASSERT(aux);
  *(static_cast<int*>(aux)) = kExpectedReturnValue;
  return -ENOMEM;
}

class DenylistNanosleepTrapPolicy : public Policy {
 public:
  explicit DenylistNanosleepTrapPolicy(int* aux) : aux_(aux) {}

  DenylistNanosleepTrapPolicy(const DenylistNanosleepTrapPolicy&) = delete;
  DenylistNanosleepTrapPolicy& operator=(const DenylistNanosleepTrapPolicy&) =
      delete;

  ~DenylistNanosleepTrapPolicy() override = default;

  ResultExpr EvaluateSyscall(int sysno) const override {
    DCHECK(SandboxBPF::IsValidSyscallNumber(sysno));
    switch (sysno) {
      case __NR_nanosleep:
        return Trap(EnomemHandler, aux_);
      default:
        return Allow();
    }
  }

 private:
  const raw_ptr<int> aux_;
};

BPF_TEST(SandboxBPF,
         BasicDenylistWithSigsys,
         DenylistNanosleepTrapPolicy,
         int /* (*BPF_AUX) */) {
  // getpid() should work properly
  errno = 0;
  BPF_ASSERT(sys_getpid() > 0);
  BPF_ASSERT(errno == 0);

  // Our Auxiliary Data, should be reset by the signal handler
  *BPF_AUX = -1;
  const struct timespec ts = {0, 0};
  BPF_ASSERT(syscall(__NR_nanosleep, &ts, NULL) == -1);
  BPF_ASSERT(errno == ENOMEM);

  // We expect the signal handler to modify AuxData
  BPF_ASSERT(*BPF_AUX == kExpectedReturnValue);
}

// A simple test that verifies we can return arbitrary errno values.

class ErrnoTestPolicy : public Policy {
 public:
  ErrnoTestPolicy() = default;

  ErrnoTestPolicy(const ErrnoTestPolicy&) = delete;
  ErrnoTestPolicy& operator=(const ErrnoTestPolicy&) = delete;

  ~ErrnoTestPolicy() override = default;

  ResultExpr EvaluateSyscall(int sysno) const override;
};

ResultExpr ErrnoTestPolicy::EvaluateSyscall(int sysno) const {
  DCHECK(SandboxBPF::IsValidSyscallNumber(sysno));
  switch (sysno) {
    case __NR_dup3:  // dup2 is a wrapper of dup3 in android
#if defined(__NR_dup2)
    case __NR_dup2:
#endif
      // Pretend that dup2() worked, but don't actually do anything.
      return Error(0);
    case __NR_setuid:
#if defined(__NR_setuid32)
    case __NR_setuid32:
#endif
      // Return errno = 1.
      return Error(1);
    case __NR_setgid:
#if defined(__NR_setgid32)
    case __NR_setgid32:
#endif
      // Return maximum errno value (typically 4095).
      return Error(ErrorCode::ERR_MAX_ERRNO);
    case __NR_uname:
      // Return errno = 42;
      return Error(42);
    default:
      return Allow();
  }
}

BPF_TEST_C(SandboxBPF, ErrnoTest, ErrnoTestPolicy) {
  // Verify that dup2() returns success, but doesn't actually run.
  int fds[4];
  BPF_ASSERT(pipe(fds) == 0);
  BPF_ASSERT(pipe(fds + 2) == 0);
  BPF_ASSERT(dup2(fds[2], fds[0]) == 0);
  char buf[1] = {};
  BPF_ASSERT(write(fds[1], "\x55", 1) == 1);
  BPF_ASSERT(write(fds[3], "\xAA", 1) == 1);
  BPF_ASSERT(read(fds[0], buf, 1) == 1);

  // If dup2() executed, we will read \xAA, but it dup2() has been turned
  // into a no-op by our policy, then we will read \x55.
  BPF_ASSERT(buf[0] == '\x55');

  // Verify that we can return the minimum and maximum errno values.
  errno = 0;
  BPF_ASSERT(setuid(0) == -1);
  BPF_ASSERT(errno == 1);

  // On Android, errno is only supported up to 255, otherwise errno
  // processing is skipped.
  // We work around this (crbug.com/181647).
  if (sandbox::IsAndroid() && setgid(0) != -1) {
    errno = 0;
    BPF_ASSERT(setgid(0) == -ErrorCode::ERR_MAX_ERRNO);
    BPF_ASSERT(errno == 0);
  } else {
    errno = 0;
    BPF_ASSERT(setgid(0) == -1);
    BPF_ASSERT(errno == ErrorCode::ERR_MAX_ERRNO);
  }

  // Finally, test an errno in between the minimum and maximum.
  errno = 0;
  struct utsname uts_buf;
  BPF_ASSERT(uname(&uts_buf) == -1);
  BPF_ASSERT(errno == 42);
}

// Testing the stacking of two sandboxes

class StackingPolicyPartOne : public Policy {
 public:
  StackingPolicyPartOne() = default;

  StackingPolicyPartOne(const StackingPolicyPartOne&) = delete;
  StackingPolicyPartOne& operator=(const StackingPolicyPartOne&) = delete;

  ~StackingPolicyPartOne() override = default;

  ResultExpr EvaluateSyscall(int sysno) const override {
    DCHECK(SandboxBPF::IsValidSyscallNumber(sysno));
    switch (sysno) {
      case __NR_getppid: {
        const Arg<int> arg(0);
        return If(arg == 0, Allow()).Else(Error(EPERM));
      }
      default:
        return Allow();
    }
  }
};

class StackingPolicyPartTwo : public Policy {
 public:
  StackingPolicyPartTwo() = default;

  StackingPolicyPartTwo(const StackingPolicyPartTwo&) = delete;
  StackingPolicyPartTwo& operator=(const StackingPolicyPartTwo&) = delete;

  ~StackingPolicyPartTwo() override = default;

  ResultExpr EvaluateSyscall(int sysno) const override {
    DCHECK(SandboxBPF::IsValidSyscallNumber(sysno));
    switch (sysno) {
      case __NR_getppid: {
        const Arg<int> arg(0);
        return If(arg == 0, Error(EINVAL)).Else(Allow());
      }
      default:
        return Allow();
    }
  }
};

// Depending on DCHECK being enabled or not the test may create some output.
// Therefore explicitly specify the death test to allow some noise.
BPF_DEATH_TEST_C(SandboxBPF,
                 StackingPolicy,
                 DEATH_SUCCESS_ALLOW_NOISE(),
                 StackingPolicyPartOne) {
  errno = 0;
  BPF_ASSERT(syscall(__NR_getppid, 0) > 0);
  BPF_ASSERT(errno == 0);

  BPF_ASSERT(syscall(__NR_getppid, 1) == -1);
  BPF_ASSERT(errno == EPERM);

  // Stack a second sandbox with its own policy. Verify that we can further
  // restrict filters, but we cannot relax existing filters.
  SandboxBPF sandbox(std::make_unique<StackingPolicyPartTwo>());
  BPF_ASSERT(sandbox.StartSandbox(SandboxBPF::SeccompLevel::SINGLE_THREADED));

  errno = 0;
  BPF_ASSERT(syscall(__NR_getppid, 0) == -1);
  BPF_ASSERT(errno == EINVAL);

  BPF_ASSERT(syscall(__NR_getppid, 1) == -1);
  BPF_ASSERT(errno == EPERM);
}

// A more complex, but synthetic policy. This tests the correctness of the BPF
// program by iterating through all syscalls and checking for an errno that
// depends on the syscall number. Unlike the Verifier, this exercises the BPF
// interpreter in the kernel.

// We try to make sure we exercise optimizations in the BPF compiler. We make
// sure that the compiler can have an opportunity to coalesce syscalls with
// contiguous numbers and we also make sure that disjoint sets can return the
// same errno.
int SysnoToRandomErrno(int sysno) {
  // Small contiguous sets of 3 system calls return an errno equal to the
  // index of that set + 1 (so that we never return a NUL errno).
  return ((sysno & ~3) >> 2) % 29 + 1;
}

class SyntheticPolicy : public Policy {
 public:
  SyntheticPolicy() = default;

  SyntheticPolicy(const SyntheticPolicy&) = delete;
  SyntheticPolicy& operator=(const SyntheticPolicy&) = delete;

  ~SyntheticPolicy() override = default;

  ResultExpr EvaluateSyscall(int sysno) const override {
    DCHECK(SandboxBPF::IsValidSyscallNumber(sysno));
    if (IsSyscallForTestHarness(sysno)) {
      return Allow();
    }
    return Error(SysnoToRandomErrno(sysno));
  }
};

BPF_TEST_C(SandboxBPF, SyntheticPolicy, SyntheticPolicy) {
  // Ensure that that kExpectedReturnValue + syscallnumber + 1 does not int
  // overflow.
  BPF_ASSERT(std::numeric_limits<int>::max() - kExpectedReturnValue - 1 >=
             static_cast<int>(MAX_PUBLIC_SYSCALL));

  for (int syscall_number = static_cast<int>(MIN_SYSCALL);
       syscall_number <= static_cast<int>(MAX_PUBLIC_SYSCALL);
       ++syscall_number) {
    if (IsSyscallForTestHarness(syscall_number)) {
      continue;
    }
    errno = 0;
    BPF_ASSERT(syscall(syscall_number) == -1);
    BPF_ASSERT(errno == SysnoToRandomErrno(syscall_number));
  }
}

#if defined(__arm__)
// A simple policy that tests whether ARM private system calls are supported
// by our BPF compiler and by the BPF interpreter in the kernel.

// For ARM private system calls, return an errno equal to their offset from
// MIN_PRIVATE_SYSCALL plus 1 (to avoid NUL errno).
int ArmPrivateSysnoToErrno(int sysno) {
  if (sysno >= static_cast<int>(MIN_PRIVATE_SYSCALL) &&
      sysno <= static_cast<int>(MAX_PRIVATE_SYSCALL)) {
    return (sysno - MIN_PRIVATE_SYSCALL) + 1;
  }
  return ENOSYS;
}

class ArmPrivatePolicy : public Policy {
 public:
  ArmPrivatePolicy() = default;

  ArmPrivatePolicy(const ArmPrivatePolicy&) = delete;
  ArmPrivatePolicy& operator=(const ArmPrivatePolicy&) = delete;

  ~ArmPrivatePolicy() override = default;

  ResultExpr EvaluateSyscall(int sysno) const override {
    DCHECK(SandboxBPF::IsValidSyscallNumber(sysno));
    // Start from |__ARM_NR_set_tls + 1| so as not to mess with actual
    // ARM private system calls.
    if (sysno >= static_cast<int>(__ARM_NR_set_tls + 1) &&
        sysno <= static_cast<int>(MAX_PRIVATE_SYSCALL)) {
      return Error(ArmPrivateSysnoToErrno(sysno));
    }
    return Allow();
  }
};

BPF_TEST_C(SandboxBPF, ArmPrivatePolicy, ArmPrivatePolicy) {
  for (int syscall_number = static_cast<int>(__ARM_NR_set_tls + 1);
       syscall_number <= static_cast<int>(MAX_PRIVATE_SYSCALL);
       ++syscall_number) {
    errno = 0;
    BPF_ASSERT(syscall(syscall_number) == -1);
    BPF_ASSERT(errno == ArmPrivateSysnoToErrno(syscall_number));
  }
}
#endif  // defined(__arm__)

intptr_t CountSyscalls(const struct arch_seccomp_data& args, void* aux) {
  // Count all invocations of our callback function.
  ++*reinterpret_cast<int*>(aux);

  // Verify that within the callback function all filtering is temporarily
  // disabled.
  BPF_ASSERT(sys_getpid() > 1);

  // Verify that we can now call the underlying system call without causing
  // infinite recursion.
  return SandboxBPF::ForwardSyscall(args);
}

class GreyListedPolicy : public Policy {
 public:
  explicit GreyListedPolicy(int* aux) : aux_(aux) {
    // Set the global environment for unsafe traps once.
    EnableUnsafeTraps();
  }

  GreyListedPolicy(const GreyListedPolicy&) = delete;
  GreyListedPolicy& operator=(const GreyListedPolicy&) = delete;

  ~GreyListedPolicy() override = default;

  ResultExpr EvaluateSyscall(int sysno) const override {
    DCHECK(SandboxBPF::IsValidSyscallNumber(sysno));
    // Some system calls must always be allowed, if our policy wants to make
    // use of UnsafeTrap()
    if (SandboxBPF::IsRequiredForUnsafeTrap(sysno)) {
      return Allow();
    }
    if (sysno == __NR_getpid) {
      // Disallow getpid()
      return Error(EPERM);
    }
    // Allow (and count) all other system calls.
    return UnsafeTrap(CountSyscalls, aux_);
  }

 private:
  const raw_ptr<int> aux_;
};

BPF_TEST(SandboxBPF, GreyListedPolicy, GreyListedPolicy, int /* (*BPF_AUX) */) {
  BPF_ASSERT(sys_getpid() == -1);
  BPF_ASSERT(errno == EPERM);
  BPF_ASSERT(*BPF_AUX == 0);
  BPF_ASSERT(syscall(__NR_geteuid) == syscall(__NR_getuid));
  BPF_ASSERT(*BPF_AUX == 2);
  char name[17] = {};
  BPF_ASSERT(!syscall(__NR_prctl,
                      PR_GET_NAME,
                      name,
                      (void*)NULL,
                      (void*)NULL,
                      (void*)NULL));
  BPF_ASSERT(*BPF_AUX == 3);
  BPF_ASSERT(*name);
}

SANDBOX_TEST(SandboxBPF, EnableUnsafeTrapsInSigSysHandler) {
  // Disabling warning messages that could confuse our test framework.
  setenv(kSandboxDebuggingEnv, "t", 0);
  Die::SuppressInfoMessages(true);

  unsetenv(kSandboxDebuggingEnv);
  SANDBOX_ASSERT(Trap::Registry()->EnableUnsafeTraps() == false);
  setenv(kSandboxDebuggingEnv, "", 1);
  SANDBOX_ASSERT(Trap::Registry()->EnableUnsafeTraps() == false);
  setenv(kSandboxDebuggingEnv, "t", 1);
  SANDBOX_ASSERT(Trap::Registry()->EnableUnsafeTraps() == true);
}

intptr_t PrctlHandler(const struct arch_seccomp_data& args, void*) {
  if (args.args[0] == PR_CAPBSET_DROP && static_cast<int>(args.args[1]) == -1) {
    // prctl(PR_CAPBSET_DROP, -1) is never valid. The kernel will always
    // return an error. But our handler allows this call.
    return 0;
  }
  return SandboxBPF::ForwardSyscall(args);
}

class PrctlPolicy : public Policy {
 public:
  PrctlPolicy() = default;

  PrctlPolicy(const PrctlPolicy&) = delete;
  PrctlPolicy& operator=(const PrctlPolicy&) = delete;

  ~PrctlPolicy() override = default;

  ResultExpr EvaluateSyscall(int sysno) const override {
    DCHECK(SandboxBPF::IsValidSyscallNumber(sysno));
    setenv(kSandboxDebuggingEnv, "t", 0);
    Die::SuppressInfoMessages(true);

    if (sysno == __NR_prctl) {
      // Handle prctl() inside an UnsafeTrap()
      return UnsafeTrap(PrctlHandler, nullptr);
    }

    // Allow all other system calls.
    return Allow();
  }
};

BPF_TEST_C(SandboxBPF, ForwardSyscall, PrctlPolicy) {
  // This call should never be allowed. But our policy will intercept it and
  // let it pass successfully.
  BPF_ASSERT(
      !prctl(PR_CAPBSET_DROP, -1, (void*)NULL, (void*)NULL, (void*)NULL));

  // Verify that the call will fail, if it makes it all the way to the kernel.
  BPF_ASSERT(
      prctl(PR_CAPBSET_DROP, -2, (void*)NULL, (void*)NULL, (void*)NULL) == -1);

  // And verify that other uses of prctl() work just fine.
  char name[17] = {};
  BPF_ASSERT(!syscall(__NR_prctl,
                      PR_GET_NAME,
                      name,
                      (void*)NULL,
                      (void*)NULL,
                      (void*)NULL));
  BPF_ASSERT(*name);

  // Finally, verify that system calls other than prctl() are completely
  // unaffected by our policy.
  struct utsname uts = {};
  BPF_ASSERT(!uname(&uts));
  BPF_ASSERT(!strcmp(uts.sysname, "Linux"));
}

intptr_t AllowRedirectedSyscall(const struct arch_seccomp_data& args, void*) {
  return SandboxBPF::ForwardSyscall(args);
}

class RedirectAllSyscallsPolicy : public Policy {
 public:
  RedirectAllSyscallsPolicy() = default;

  RedirectAllSyscallsPolicy(const RedirectAllSyscallsPolicy&) = delete;
  RedirectAllSyscallsPolicy& operator=(const RedirectAllSyscallsPolicy&) =
      delete;

  ~RedirectAllSyscallsPolicy() override = default;

  ResultExpr EvaluateSyscall(int sysno) const override;
};

ResultExpr RedirectAllSyscallsPolicy::EvaluateSyscall(int sysno) const {
  DCHECK(SandboxBPF::IsValidSyscallNumber(sysno));
  setenv(kSandboxDebuggingEnv, "t", 0);
  Die::SuppressInfoMessages(true);

  // Some system calls must always be allowed, if our policy wants to make
  // use of UnsafeTrap()
  if (SandboxBPF::IsRequiredForUnsafeTrap(sysno))
    return Allow();
  return UnsafeTrap(AllowRedirectedSyscall, nullptr);
}

#if !defined(ADDRESS_SANITIZER)
// ASan does not allow changing the signal handler for SIGBUS, and treats it as
// a fatal signal.

int bus_handler_fd_ = -1;

void SigBusHandler(int, siginfo_t* info, void* void_context) {
  BPF_ASSERT(write(bus_handler_fd_, "\x55", 1) == 1);
}

BPF_TEST_C(SandboxBPF, SigBus, RedirectAllSyscallsPolicy) {
  // We use the SIGBUS bit in the signal mask as a thread-local boolean
  // value in the implementation of UnsafeTrap(). This is obviously a bit
  // of a hack that could conceivably interfere with code that uses SIGBUS
  // in more traditional ways. This test verifies that basic functionality
  // of SIGBUS is not impacted, but it is certainly possibly to construe
  // more complex uses of signals where our use of the SIGBUS mask is not
  // 100% transparent. This is expected behavior.
  int fds[2];
  BPF_ASSERT(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
  bus_handler_fd_ = fds[1];
  struct sigaction sa = {};
  sa.sa_sigaction = SigBusHandler;
  sa.sa_flags = SA_SIGINFO;
  BPF_ASSERT(sigaction(SIGBUS, &sa, nullptr) == 0);
  kill(getpid(), SIGBUS);
  char c = '\000';
  BPF_ASSERT(read(fds[0], &c, 1) == 1);
  BPF_ASSERT(close(fds[0]) == 0);
  BPF_ASSERT(close(fds[1]) == 0);
  BPF_ASSERT(c == 0x55);
}
#endif  // !defined(ADDRESS_SANITIZER)

BPF_TEST_C(SandboxBPF, SigMask, RedirectAllSyscallsPolicy) {
  // Signal masks are potentially tricky to handle. For instance, if we
  // ever tried to update them from inside a Trap() or UnsafeTrap() handler,
  // the call to sigreturn() at the end of the signal handler would undo
  // all of our efforts. So, it makes sense to test that sigprocmask()
  // works, even if we have a policy in place that makes use of UnsafeTrap().
  // In practice, this works because we force sigprocmask() to be handled
  // entirely in the kernel.
  sigset_t mask0, mask1, mask2;

  // Call sigprocmask() to verify that SIGUSR2 wasn't blocked, if we didn't
  // change the mask (it shouldn't have been, as it isn't blocked by default
  // in POSIX).
  //
  // Use SIGUSR2 because Android seems to use SIGUSR1 for some purpose.
  sigemptyset(&mask0);
  BPF_ASSERT(!sigprocmask(SIG_BLOCK, &mask0, &mask1));
  BPF_ASSERT(!sigismember(&mask1, SIGUSR2));

  // Try again, and this time we verify that we can block it. This
  // requires a second call to sigprocmask().
  sigaddset(&mask0, SIGUSR2);
  BPF_ASSERT(!sigprocmask(SIG_BLOCK, &mask0, nullptr));
  BPF_ASSERT(!sigprocmask(SIG_BLOCK, nullptr, &mask2));
  BPF_ASSERT(sigismember(&mask2, SIGUSR2));
}

BPF_TEST_C(SandboxBPF, UnsafeTrapWithErrno, RedirectAllSyscallsPolicy) {
  // An UnsafeTrap() (or for that matter, a Trap()) has to report error
  // conditions by returning an exit code in the range -1..-4096. This
  // should happen automatically if using ForwardSyscall(). If the TrapFnc()
  // uses some other method to make system calls, then it is responsible
  // for computing the correct return code.
  // This test verifies that ForwardSyscall() does the correct thing.

  // The glibc system wrapper will ultimately set errno for us. So, from normal
  // userspace, all of this should be completely transparent.
  errno = 0;
  BPF_ASSERT(close(-1) == -1);
  BPF_ASSERT(errno == EBADF);

  // Explicitly avoid the glibc wrapper. This is not normally the way anybody
  // would make system calls, but it allows us to verify that we don't
  // accidentally mess with errno, when we shouldn't.
  errno = 0;
  struct arch_seccomp_data args = {};
  args.nr = __NR_close;
  args.args[0] = -1;
  BPF_ASSERT(SandboxBPF::ForwardSyscall(args) == -EBADF);
  BPF_ASSERT(errno == 0);
}

// Simple test demonstrating how to use SandboxBPF::Cond()

class SimpleCondTestPolicy : public Policy {
 public:
  SimpleCondTestPolicy() = default;

  SimpleCondTestPolicy(const SimpleCondTestPolicy&) = delete;
  SimpleCondTestPolicy& operator=(const SimpleCondTestPolicy&) = delete;

  ~SimpleCondTestPolicy() override = default;

  ResultExpr EvaluateSyscall(int sysno) const override;
};

ResultExpr SimpleCondTestPolicy::EvaluateSyscall(int sysno) const {
  DCHECK(SandboxBPF::IsValidSyscallNumber(sysno));

  // We deliberately return unusual errno values upon failure, so that we
  // can uniquely test for these values. In a "real" policy, you would want
  // to return more traditional values.
  int flags_argument_position = -1;
  switch (sysno) {
#if defined(__NR_open)
    case __NR_open:
      flags_argument_position = 1;
      [[fallthrough]];
#endif
    case __NR_openat: {  // open can be a wrapper for openat(2).
      if (sysno == __NR_openat)
        flags_argument_position = 2;

      // Allow opening files for reading, but don't allow writing.
      static_assert(O_RDONLY == 0, "O_RDONLY must be all zero bits");
      const Arg<int> flags(flags_argument_position);
      return If((flags & O_ACCMODE) != 0, Error(EROFS)).Else(Allow());
    }
    case __NR_prctl: {
      // Allow prctl(PR_SET_DUMPABLE) and prctl(PR_GET_DUMPABLE), but
      // disallow everything else.
      const Arg<int> option(0);
      return Switch(option)
          .Cases({PR_SET_DUMPABLE, PR_GET_DUMPABLE}, Allow())
          .Default(Error(ENOMEM));
    }
    default:
      return Allow();
  }
}

BPF_TEST_C(SandboxBPF, SimpleCondTest, SimpleCondTestPolicy) {
  int fd;
  BPF_ASSERT((fd = open("/proc/self/comm", O_RDWR)) == -1);
  BPF_ASSERT(errno == EROFS);
  BPF_ASSERT((fd = open("/proc/self/comm", O_RDONLY)) >= 0);
  close(fd);

  int ret;
  BPF_ASSERT((ret = prctl(PR_GET_DUMPABLE)) >= 0);
  BPF_ASSERT(prctl(PR_SET_DUMPABLE, 1 - ret) == 0);
  BPF_ASSERT(prctl(PR_GET_ENDIAN, &ret) == -1);
  BPF_ASSERT(errno == ENOMEM);
}

// This test exercises the SandboxBPF::Cond() method by building a complex
// tree of conditional equality operations. It then makes system calls and
// verifies that they return the values that we expected from our BPF
// program.
class EqualityStressTest {
 public:
  EqualityStressTest() {
    // We want a deterministic test
    srand(0);

    // Iterates over system call numbers and builds a random tree of
    // equality tests.
    // We are actually constructing a graph of ArgValue objects. This
    // graph will later be used to a) compute our sandbox policy, and
    // b) drive the code that verifies the output from the BPF program.
    static_assert(
        kNumTestCases < (int)(MAX_PUBLIC_SYSCALL - MIN_SYSCALL - 10),
        "kNumTestCases must be significantly smaller than the number "
        "of system calls");
    for (int sysno = MIN_SYSCALL, end = kNumTestCases; sysno < end; ++sysno) {
      if (IsReservedSyscall(sysno)) {
        // Skip reserved system calls. This ensures that our test frame
        // work isn't impacted by the fact that we are overriding
        // a lot of different system calls.
        ++end;
        arg_values_.push_back(nullptr);
      } else {
        arg_values_.push_back(
            RandomArgValue(rand() % kMaxArgs, 0, rand() % kMaxArgs));
      }
    }
  }

  ~EqualityStressTest() = default;

  ResultExpr Policy(int sysno) {
    DCHECK(SandboxBPF::IsValidSyscallNumber(sysno));
    if (sysno < 0 || sysno >= (int)arg_values_.size() ||
        IsReservedSyscall(sysno)) {
      // We only return ErrorCode values for the system calls that
      // are part of our test data. Every other system call remains
      // allowed.
      return Allow();
    }
    // ToErrorCode() turns an ArgValue object into an ErrorCode that is
    // suitable for use by a sandbox policy.
    return ToErrorCode(arg_values_[sysno].get());
  }

  void VerifyFilter() {
    // Iterate over all system calls. Skip the system calls that have
    // previously been determined as being reserved.
    for (int sysno = 0; sysno < (int)arg_values_.size(); ++sysno) {
      if (!arg_values_[sysno]) {
        // Skip reserved system calls.
        continue;
      }
      // Verify that system calls return the values that we expect them to
      // return. This involves passing different combinations of system call
      // parameters in order to exercise all possible code paths through the
      // BPF filter program.
      // We arbitrarily start by setting all six system call arguments to
      // zero. And we then recursive traverse our tree of ArgValues to
      // determine the necessary combinations of parameters.
      intptr_t args[6] = {};
      Verify(sysno, args, *arg_values_[sysno]);
    }
  }

 private:
  struct Tests;
  struct ArgValue {
    int argno;  // Argument number to inspect.
    int err;    // If none of the tests passed, this is what
                // we'll return (this is the "else" branch).
    std::vector<Tests> tests;
    std::unique_ptr<ArgValue> next;
  };

  struct Tests {
    uint32_t k_value;  // Value to compare syscall arg against.
    int err;           // If non-zero, errno value to return.
                       // Otherwise, more args needs inspecting.
    std::unique_ptr<ArgValue> arg_value;
  };

  bool IsReservedSyscall(int sysno) {
    // There are a handful of system calls that we should never use in our
    // test cases. These system calls are needed to allow the test framework
    // to run properly.
    // If we wanted to write fully generic code, there are more system calls
    // that could be listed here, and it is quite difficult to come up with a
    // truly comprehensive list. After all, we are deliberately making system
    // calls unavailable. In practice, we have a pretty good idea of the system
    // calls that will be made by this particular test. So, this small list is
    // sufficient. But if anybody copy'n'pasted this code for other uses, they
    // would have to review that the list.
    return sysno == __NR_read || sysno == __NR_write || sysno == __NR_exit ||
           sysno == __NR_exit_group || sysno == __NR_restart_syscall;
  }

  std::unique_ptr<ArgValue> RandomArgValue(int argno,
                                           int args_mask,
                                           int remaining_args) {
    // Create a new ArgValue and fill it with random data. We use as bit mask
    // to keep track of the system call parameters that have previously been
    // set; this ensures that we won't accidentally define a contradictory
    // set of equality tests.
    auto arg_value = std::make_unique<ArgValue>();
    args_mask |= 1 << argno;
    arg_value->argno = argno;

    // Apply some restrictions on just how complex our tests can be.
    // Otherwise, we end up with a BPF program that is too complicated for
    // the kernel to load.
    int fan_out = kMaxFanOut;
    if (remaining_args > 3) {
      fan_out = 1;
    } else if (remaining_args > 2) {
      fan_out = 2;
    }

    // Create a couple of different test cases with randomized values that
    // we want to use when comparing system call parameter number "argno".
    arg_value->tests.resize(rand() % fan_out + 1);

    uint32_t k_value = rand();
    for (auto& test : arg_value->tests) {
      // Ensure that we have unique values
      k_value += rand() % (RAND_MAX / (kMaxFanOut + 1)) + 1;
      test.k_value = k_value;

      // There are two possible types of nodes. Either this is a leaf node;
      // in that case, we have completed all the equality tests that we
      // wanted to perform, and we can now compute a random "errno" value that
      // we should return. Or this is part of a more complex boolean
      // expression; in that case, we have to recursively add tests for some
      // of system call parameters that we have not yet included in our
      // tests.
      if (!remaining_args || (rand() & 1)) {
        test.err = 1 + (rand() % 1000);
        test.arg_value = nullptr;
      } else {
        test.err = 0;
        test.arg_value =
            RandomArgValue(RandomArg(args_mask), args_mask, remaining_args - 1);
      }
    }
    // Finally, we have to define what we should return if none of the
    // previous equality tests pass. Again, we can either deal with a leaf
    // node, or we can randomly add another couple of tests.
    if (!remaining_args || (rand() & 1)) {
      arg_value->err = (rand() % 1000) + 1;
      arg_value->next = nullptr;
    } else {
      arg_value->err = 0;
      arg_value->next =
          RandomArgValue(RandomArg(args_mask), args_mask, remaining_args - 1);
    }
    // We have now built a new (sub-)tree of ArgValues defining a set of
    // boolean expressions for testing random system call arguments against
    // random values. Return this tree to our caller.
    return arg_value;
  }

  int RandomArg(int args_mask) {
    // Compute a random system call parameter number.
    int argno = rand() % kMaxArgs;

    // Make sure that this same parameter number has not previously been
    // used. Otherwise, we could end up with a test that is impossible to
    // satisfy (e.g. args[0] == 1 && args[0] == 2).
    while (args_mask & (1 << argno)) {
      argno = (argno + 1) % kMaxArgs;
    }
    return argno;
  }

  ResultExpr ToErrorCode(ArgValue* arg_value) {
    // Compute the ResultExpr that should be returned, if none of our
    // tests succeed (i.e. the system call parameter doesn't match any
    // of the values in arg_value->tests[].k_value).
    // If this was a leaf node, return the errno value that we expect to
    // return from the BPF filter program.
    // If this wasn't a leaf node yet, recursively descend into the rest
    // of the tree. This will end up adding a few more SandboxBPF::Cond()
    // tests to our ErrorCode.
    ResultExpr err = arg_value->err ? Error(arg_value->err)
                                    : ToErrorCode(arg_value->next.get());

    // Now, iterate over all the test cases that we want to compare against.
    // This builds a chain of SandboxBPF::Cond() tests
    // (aka "if ... elif ... elif ... elif ... fi")
    for (auto& test : base::Reversed(arg_value->tests)) {
      // Again, we distinguish between leaf nodes and subtrees.
      ResultExpr matched =
          test.err ? Error(test.err) : ToErrorCode(test.arg_value.get());

      // For now, all of our tests are limited to 32bit.
      // We have separate tests that check the behavior of 32bit vs. 64bit
      // conditional expressions.
      const Arg<uint32_t> arg(arg_value->argno);
      err = If(arg == test.k_value, matched).Else(err);
    }
    return err;
  }

  void Verify(int sysno, intptr_t* args, const ArgValue& arg_value) {
    uint32_t mismatched = 0;
    // Iterate over all the k_values in arg_value.tests[] and verify that
    // we see the expected return values from system calls, when we pass
    // the k_value as a parameter in a system call.
    for (auto& test : base::Reversed(arg_value.tests)) {
      mismatched += test.k_value;
      args[arg_value.argno] = test.k_value;
      if (test.err) {
        VerifyErrno(sysno, args, test.err);
      } else {
        Verify(sysno, args, *test.arg_value);
      }
    }
    // Find a k_value that doesn't match any of the k_values in
    // arg_value.tests[]. In most cases, the current value of "mismatched"
    // would fit this requirement. But on the off-chance that it happens
    // to collide, we double-check.
    while (base::Contains(arg_value.tests, mismatched, &Tests::k_value)) {
      ++mismatched;
    }
    // Now verify that we see the expected return value from system calls,
    // if we pass a value that doesn't match any of the conditions (i.e. this
    // is testing the "else" clause of the conditions).
    args[arg_value.argno] = mismatched;
    if (arg_value.err) {
      VerifyErrno(sysno, args, arg_value.err);
    } else {
      Verify(sysno, args, *arg_value.next);
    }
    // Reset args[arg_value.argno]. This is not technically needed, but it
    // makes it easier to reason about the correctness of our tests.
    args[arg_value.argno] = 0;
  }

  void VerifyErrno(int sysno, intptr_t* args, int err) {
    // We installed BPF filters that return different errno values
    // based on the system call number and the parameters that we decided
    // to pass in. Verify that this condition holds true.
    BPF_ASSERT(
        Syscall::Call(
            sysno, args[0], args[1], args[2], args[3], args[4], args[5]) ==
        -err);
  }

  // Vector of ArgValue trees. These trees define all the possible boolean
  // expressions that we want to turn into a BPF filter program.
  std::vector<std::unique_ptr<ArgValue>> arg_values_;

  // Don't increase these values. We are pushing the limits of the maximum
  // BPF program that the kernel will allow us to load. If the values are
  // increased too much, the test will start failing.
#if defined(__aarch64__)
  static const int kNumTestCases = 30;
#else
  static const int kNumTestCases = 40;
#endif
  static const int kMaxFanOut = 3;
  static const int kMaxArgs = 6;
};

class EqualityStressTestPolicy : public Policy {
 public:
  explicit EqualityStressTestPolicy(EqualityStressTest* aux) : aux_(aux) {}

  EqualityStressTestPolicy(const EqualityStressTestPolicy&) = delete;
  EqualityStressTestPolicy& operator=(const EqualityStressTestPolicy&) = delete;

  ~EqualityStressTestPolicy() override = default;

  ResultExpr EvaluateSyscall(int sysno) const override {
    return aux_->Policy(sysno);
  }

 private:
  const raw_ptr<EqualityStressTest> aux_;
};

BPF_TEST(SandboxBPF,
         EqualityTests,
         EqualityStressTestPolicy,
         EqualityStressTest /* (*BPF_AUX) */) {
  BPF_AUX->VerifyFilter();
}

class EqualityArgumentWidthPolicy : public Policy {
 public:
  EqualityArgumentWidthPolicy() = default;

  EqualityArgumentWidthPolicy(const EqualityArgumentWidthPolicy&) = delete;
  EqualityArgumentWidthPolicy& operator=(const EqualityArgumentWidthPolicy&) =
      delete;

  ~EqualityArgumentWidthPolicy() override = default;

  ResultExpr EvaluateSyscall(int sysno) const override;
};

ResultExpr EqualityArgumentWidthPolicy::EvaluateSyscall(int sysno) const {
  DCHECK(SandboxBPF::IsValidSyscallNumber(sysno));
  if (sysno == __NR_uname) {
    const Arg<int> option(0);
    const Arg<uint32_t> arg32(1);
    const Arg<uint64_t> arg64(1);
    return Switch(option)
        .Case(0, If(arg32 == 0x55555555, Error(1)).Else(Error(2)))
#if __SIZEOF_POINTER__ > 4
        .Case(1, If(arg64 == 0x55555555AAAAAAAAULL, Error(1)).Else(Error(2)))
#endif
        .Default(Error(3));
  }
  return Allow();
}

BPF_TEST_C(SandboxBPF, EqualityArgumentWidth, EqualityArgumentWidthPolicy) {
  BPF_ASSERT(Syscall::Call(__NR_uname, 0, 0x55555555) == -1);
  BPF_ASSERT(Syscall::Call(__NR_uname, 0, 0xAAAAAAAA) == -2);
#if __SIZEOF_POINTER__ > 4
  // On 32bit machines, there is no way to pass a 64bit argument through the
  // syscall interface. So, we have to skip the part of the test that requires
  // 64bit arguments.
  BPF_ASSERT(Syscall::Call(__NR_uname, 1, 0x55555555AAAAAAAAULL) == -1);
  BPF_ASSERT(Syscall::Call(__NR_uname, 1, 0x5555555500000000ULL) == -2);
  BPF_ASSERT(Syscall::Call(__NR_uname, 1, 0x5555555511111111ULL) == -2);
  BPF_ASSERT(Syscall::Call(__NR_uname, 1, 0x11111111AAAAAAAAULL) == -2);
#endif
}

#if __SIZEOF_POINTER__ > 4
// On 32bit machines, there is no way to pass a 64bit argument through the
// syscall interface. So, we have to skip the part of the test that requires
// 64bit arguments.
BPF_DEATH_TEST_C(SandboxBPF,
                 EqualityArgumentUnallowed64bit,
                 DEATH_MESSAGE("Unexpected 64bit argument detected"),
                 EqualityArgumentWidthPolicy) {
  Syscall::Call(__NR_uname, 0, 0x5555555555555555ULL);
}
#endif

class EqualityWithNegativeArgumentsPolicy : public Policy {
 public:
  EqualityWithNegativeArgumentsPolicy() = default;

  EqualityWithNegativeArgumentsPolicy(
      const EqualityWithNegativeArgumentsPolicy&) = delete;
  EqualityWithNegativeArgumentsPolicy& operator=(
      const EqualityWithNegativeArgumentsPolicy&) = delete;

  ~EqualityWithNegativeArgumentsPolicy() override = default;

  ResultExpr EvaluateSyscall(int sysno) const override {
    DCHECK(SandboxBPF::IsValidSyscallNumber(sysno));
    if (sysno == __NR_uname) {
      // TODO(mdempsky): This currently can't be Arg<int> because then
      // 0xFFFFFFFF will be treated as a (signed) int, and then when
      // Arg::EqualTo casts it to uint64_t, it will be sign extended.
      const Arg<unsigned> arg(0);
      return If(arg == 0xFFFFFFFF, Error(1)).Else(Error(2));
    }
    return Allow();
  }
};

BPF_TEST_C(SandboxBPF,
           EqualityWithNegativeArguments,
           EqualityWithNegativeArgumentsPolicy) {
  BPF_ASSERT(Syscall::Call(__NR_uname, 0xFFFFFFFF) == -1);
  BPF_ASSERT(Syscall::Call(__NR_uname, -1) == -1);
  BPF_ASSERT(Syscall::Call(__NR_uname, -1LL) == -1);
}

#if __SIZEOF_POINTER__ > 4
BPF_DEATH_TEST_C(SandboxBPF,
                 EqualityWithNegative64bitArguments,
                 DEATH_MESSAGE("Unexpected 64bit argument detected"),
                 EqualityWithNegativeArgumentsPolicy) {
  // When expecting a 32bit system call argument, we look at the MSB of the
  // 64bit value and allow both "0" and "-1". But the latter is allowed only
  // iff the LSB was negative. So, this death test should error out.
  BPF_ASSERT(Syscall::Call(__NR_uname, 0xFFFFFFFF00000000LL) == -1);
}
#endif

class AllBitTestPolicy : public Policy {
 public:
  AllBitTestPolicy() = default;

  AllBitTestPolicy(const AllBitTestPolicy&) = delete;
  AllBitTestPolicy& operator=(const AllBitTestPolicy&) = delete;

  ~AllBitTestPolicy() override = default;

  ResultExpr EvaluateSyscall(int sysno) const override;

 private:
  static ResultExpr HasAllBits32(uint32_t bits);
  static ResultExpr HasAllBits64(uint64_t bits);
};

ResultExpr AllBitTestPolicy::HasAllBits32(uint32_t bits) {
  if (bits == 0) {
    return Error(1);
  }
  const Arg<uint32_t> arg(1);
  return If((arg & bits) == bits, Error(1)).Else(Error(0));
}

ResultExpr AllBitTestPolicy::HasAllBits64(uint64_t bits) {
  if (bits == 0) {
    return Error(1);
  }
  const Arg<uint64_t> arg(1);
  return If((arg & bits) == bits, Error(1)).Else(Error(0));
}

ResultExpr AllBitTestPolicy::EvaluateSyscall(int sysno) const {
  DCHECK(SandboxBPF::IsValidSyscallNumber(sysno));
  // Test masked-equality cases that should trigger the "has all bits"
  // peephole optimizations. We try to find bitmasks that could conceivably
  // touch corner cases.
  // For all of these tests, we override the uname(). We can make use with
  // a single system call number, as we use the first system call argument to
  // select the different bit masks that we want to test against.
  if (sysno == __NR_uname) {
    const Arg<int> option(0);
    return Switch(option)
        .Case(0, HasAllBits32(0x0))
        .Case(1, HasAllBits32(0x1))
        .Case(2, HasAllBits32(0x3))
        .Case(3, HasAllBits32(0x80000000))
#if __SIZEOF_POINTER__ > 4
        .Case(4, HasAllBits64(0x0))
        .Case(5, HasAllBits64(0x1))
        .Case(6, HasAllBits64(0x3))
        .Case(7, HasAllBits64(0x80000000))
        .Case(8, HasAllBits64(0x100000000ULL))
        .Case(9, HasAllBits64(0x300000000ULL))
        .Case(10, HasAllBits64(0x100000001ULL))
#endif
        .Default(Kill());
  }
  return Allow();
}

// Define a macro that performs tests using our test policy.
// NOTE: Not all of the arguments in this macro are actually used!
//       They are here just to serve as documentation of the conditions
//       implemented in the test policy.
//       Most notably, "op" and "mask" are unused by the macro. If you want
//       to make changes to these values, you will have to edit the
//       test policy instead.
#define BITMASK_TEST(testcase, arg, op, mask, expected_value) \
  BPF_ASSERT(Syscall::Call(__NR_uname, (testcase), (arg)) == (expected_value))

// Our uname() system call returns ErrorCode(1) for success and
// ErrorCode(0) for failure. Syscall::Call() turns this into an
// exit code of -1 or 0.
#define EXPECT_FAILURE 0
#define EXPECT_SUCCESS -1

// A couple of our tests behave differently on 32bit and 64bit systems, as
// there is no way for a 32bit system call to pass in a 64bit system call
// argument "arg".
// We expect these tests to succeed on 64bit systems, but to tail on 32bit
// systems.
#define EXPT64_SUCCESS (sizeof(void*) > 4 ? EXPECT_SUCCESS : EXPECT_FAILURE)
BPF_TEST_C(SandboxBPF, AllBitTests, AllBitTestPolicy) {
  // 32bit test: all of 0x0 (should always be true)
  BITMASK_TEST( 0,                   0, ALLBITS32,          0, EXPECT_SUCCESS);
  BITMASK_TEST( 0,                   1, ALLBITS32,          0, EXPECT_SUCCESS);
  BITMASK_TEST( 0,                   3, ALLBITS32,          0, EXPECT_SUCCESS);
  BITMASK_TEST( 0,         0xFFFFFFFFU, ALLBITS32,          0, EXPECT_SUCCESS);
  BITMASK_TEST( 0,                -1LL, ALLBITS32,          0, EXPECT_SUCCESS);

  // 32bit test: all of 0x1
  BITMASK_TEST( 1,                   0, ALLBITS32,        0x1, EXPECT_FAILURE);
  BITMASK_TEST( 1,                   1, ALLBITS32,        0x1, EXPECT_SUCCESS);
  BITMASK_TEST( 1,                   2, ALLBITS32,        0x1, EXPECT_FAILURE);
  BITMASK_TEST( 1,                   3, ALLBITS32,        0x1, EXPECT_SUCCESS);

  // 32bit test: all of 0x3
  BITMASK_TEST( 2,                   0, ALLBITS32,        0x3, EXPECT_FAILURE);
  BITMASK_TEST( 2,                   1, ALLBITS32,        0x3, EXPECT_FAILURE);
  BITMASK_TEST( 2,                   2, ALLBITS32,        0x3, EXPECT_FAILURE);
  BITMASK_TEST( 2,                   3, ALLBITS32,        0x3, EXPECT_SUCCESS);
  BITMASK_TEST( 2,                   7, ALLBITS32,        0x3, EXPECT_SUCCESS);

  // 32bit test: all of 0x80000000
  BITMASK_TEST( 3,                   0, ALLBITS32, 0x80000000, EXPECT_FAILURE);
  BITMASK_TEST( 3,         0x40000000U, ALLBITS32, 0x80000000, EXPECT_FAILURE);
  BITMASK_TEST( 3,         0x80000000U, ALLBITS32, 0x80000000, EXPECT_SUCCESS);
  BITMASK_TEST( 3,         0xC0000000U, ALLBITS32, 0x80000000, EXPECT_SUCCESS);
  BITMASK_TEST( 3,       -0x80000000LL, ALLBITS32, 0x80000000, EXPECT_SUCCESS);

#if __SIZEOF_POINTER__ > 4
  // 64bit test: all of 0x0 (should always be true)
  BITMASK_TEST( 4,                   0, ALLBITS64,          0, EXPECT_SUCCESS);
  BITMASK_TEST( 4,                   1, ALLBITS64,          0, EXPECT_SUCCESS);
  BITMASK_TEST( 4,                   3, ALLBITS64,          0, EXPECT_SUCCESS);
  BITMASK_TEST( 4,         0xFFFFFFFFU, ALLBITS64,          0, EXPECT_SUCCESS);
  BITMASK_TEST( 4,       0x100000000LL, ALLBITS64,          0, EXPECT_SUCCESS);
  BITMASK_TEST( 4,       0x300000000LL, ALLBITS64,          0, EXPECT_SUCCESS);
  BITMASK_TEST( 4,0x8000000000000000LL, ALLBITS64,          0, EXPECT_SUCCESS);
  BITMASK_TEST( 4,                -1LL, ALLBITS64,          0, EXPECT_SUCCESS);

  // 64bit test: all of 0x1
  BITMASK_TEST( 5,                   0, ALLBITS64,          1, EXPECT_FAILURE);
  BITMASK_TEST( 5,                   1, ALLBITS64,          1, EXPECT_SUCCESS);
  BITMASK_TEST( 5,                   2, ALLBITS64,          1, EXPECT_FAILURE);
  BITMASK_TEST( 5,                   3, ALLBITS64,          1, EXPECT_SUCCESS);
  BITMASK_TEST( 5,       0x100000000LL, ALLBITS64,          1, EXPECT_FAILURE);
  BITMASK_TEST( 5,       0x100000001LL, ALLBITS64,          1, EXPECT_SUCCESS);
  BITMASK_TEST( 5,       0x100000002LL, ALLBITS64,          1, EXPECT_FAILURE);
  BITMASK_TEST( 5,       0x100000003LL, ALLBITS64,          1, EXPECT_SUCCESS);

  // 64bit test: all of 0x3
  BITMASK_TEST( 6,                   0, ALLBITS64,          3, EXPECT_FAILURE);
  BITMASK_TEST( 6,                   1, ALLBITS64,          3, EXPECT_FAILURE);
  BITMASK_TEST( 6,                   2, ALLBITS64,          3, EXPECT_FAILURE);
  BITMASK_TEST( 6,                   3, ALLBITS64,          3, EXPECT_SUCCESS);
  BITMASK_TEST( 6,                   7, ALLBITS64,          3, EXPECT_SUCCESS);
  BITMASK_TEST( 6,       0x100000000LL, ALLBITS64,          3, EXPECT_FAILURE);
  BITMASK_TEST( 6,       0x100000001LL, ALLBITS64,          3, EXPECT_FAILURE);
  BITMASK_TEST( 6,       0x100000002LL, ALLBITS64,          3, EXPECT_FAILURE);
  BITMASK_TEST( 6,       0x100000003LL, ALLBITS64,          3, EXPECT_SUCCESS);
  BITMASK_TEST( 6,       0x100000007LL, ALLBITS64,          3, EXPECT_SUCCESS);

  // 64bit test: all of 0x80000000
  BITMASK_TEST( 7,                   0, ALLBITS64, 0x80000000, EXPECT_FAILURE);
  BITMASK_TEST( 7,         0x40000000U, ALLBITS64, 0x80000000, EXPECT_FAILURE);
  BITMASK_TEST( 7,         0x80000000U, ALLBITS64, 0x80000000, EXPECT_SUCCESS);
  BITMASK_TEST( 7,         0xC0000000U, ALLBITS64, 0x80000000, EXPECT_SUCCESS);
  BITMASK_TEST( 7,       -0x80000000LL, ALLBITS64, 0x80000000, EXPECT_SUCCESS);
  BITMASK_TEST( 7,       0x100000000LL, ALLBITS64, 0x80000000, EXPECT_FAILURE);
  BITMASK_TEST( 7,       0x140000000LL, ALLBITS64, 0x80000000, EXPECT_FAILURE);
  BITMASK_TEST( 7,       0x180000000LL, ALLBITS64, 0x80000000, EXPECT_SUCCESS);
  BITMASK_TEST( 7,       0x1C0000000LL, ALLBITS64, 0x80000000, EXPECT_SUCCESS);
  BITMASK_TEST( 7,      -0x180000000LL, ALLBITS64, 0x80000000, EXPECT_SUCCESS);

  // 64bit test: all of 0x100000000
  BITMASK_TEST( 8,       0x000000000LL, ALLBITS64,0x100000000, EXPECT_FAILURE);
  BITMASK_TEST( 8,       0x100000000LL, ALLBITS64,0x100000000, EXPT64_SUCCESS);
  BITMASK_TEST( 8,       0x200000000LL, ALLBITS64,0x100000000, EXPECT_FAILURE);
  BITMASK_TEST( 8,       0x300000000LL, ALLBITS64,0x100000000, EXPT64_SUCCESS);
  BITMASK_TEST( 8,       0x000000001LL, ALLBITS64,0x100000000, EXPECT_FAILURE);
  BITMASK_TEST( 8,       0x100000001LL, ALLBITS64,0x100000000, EXPT64_SUCCESS);
  BITMASK_TEST( 8,       0x200000001LL, ALLBITS64,0x100000000, EXPECT_FAILURE);
  BITMASK_TEST( 8,       0x300000001LL, ALLBITS64,0x100000000, EXPT64_SUCCESS);

  // 64bit test: all of 0x300000000
  BITMASK_TEST( 9,       0x000000000LL, ALLBITS64,0x300000000, EXPECT_FAILURE);
  BITMASK_TEST( 9,       0x100000000LL, ALLBITS64,0x300000000, EXPECT_FAILURE);
  BITMASK_TEST( 9,       0x200000000LL, ALLBITS64,0x300000000, EXPECT_FAILURE);
  BITMASK_TEST( 9,       0x300000000LL, ALLBITS64,0x300000000, EXPT64_SUCCESS);
  BITMASK_TEST( 9,       0x700000000LL, ALLBITS64,0x300000000, EXPT64_SUCCESS);
  BITMASK_TEST( 9,       0x000000001LL, ALLBITS64,0x300000000, EXPECT_FAILURE);
  BITMASK_TEST( 9,       0x100000001LL, ALLBITS64,0x300000000, EXPECT_FAILURE);
  BITMASK_TEST( 9,       0x200000001LL, ALLBITS64,0x300000000, EXPECT_FAILURE);
  BITMASK_TEST( 9,       0x300000001LL, ALLBITS64,0x300000000, EXPT64_SUCCESS);
  BITMASK_TEST( 9,       0x700000001LL, ALLBITS64,0x300000000, EXPT64_SUCCESS);

  // 64bit test: all of 0x100000001
  BITMASK_TEST(10,       0x000000000LL, ALLBITS64,0x100000001, EXPECT_FAILURE);
  BITMASK_TEST(10,       0x000000001LL, ALLBITS64,0x100000001, EXPECT_FAILURE);
  BITMASK_TEST(10,       0x100000000LL, ALLBITS64,0x100000001, EXPECT_FAILURE);
  BITMASK_TEST(10,       0x100000001LL, ALLBITS64,0x100000001, EXPT64_SUCCESS);
  BITMASK_TEST(10,         0xFFFFFFFFU, ALLBITS64,0x100000001, EXPECT_FAILURE);
  BITMASK_TEST(10,                 -1L, ALLBITS64,0x100000001, EXPT64_SUCCESS);
#endif
}

class AnyBitTestPolicy : public Policy {
 public:
  AnyBitTestPolicy() = default;

  AnyBitTestPolicy(const AnyBitTestPolicy&) = delete;
  AnyBitTestPolicy& operator=(const AnyBitTestPolicy&) = delete;

  ~AnyBitTestPolicy() override = default;

  ResultExpr EvaluateSyscall(int sysno) const override;

 private:
  static ResultExpr HasAnyBits32(uint32_t);
  static ResultExpr HasAnyBits64(uint64_t);
};

ResultExpr AnyBitTestPolicy::HasAnyBits32(uint32_t bits) {
  if (bits == 0) {
    return Error(0);
  }
  const Arg<uint32_t> arg(1);
  return If((arg & bits) != 0, Error(1)).Else(Error(0));
}

ResultExpr AnyBitTestPolicy::HasAnyBits64(uint64_t bits) {
  if (bits == 0) {
    return Error(0);
  }
  const Arg<uint64_t> arg(1);
  return If((arg & bits) != 0, Error(1)).Else(Error(0));
}

ResultExpr AnyBitTestPolicy::EvaluateSyscall(int sysno) const {
  DCHECK(SandboxBPF::IsValidSyscallNumber(sysno));
  // Test masked-equality cases that should trigger the "has any bits"
  // peephole optimizations. We try to find bitmasks that could conceivably
  // touch corner cases.
  // For all of these tests, we override the uname(). We can make use with
  // a single system call number, as we use the first system call argument to
  // select the different bit masks that we want to test against.
  if (sysno == __NR_uname) {
    const Arg<int> option(0);
    return Switch(option)
        .Case(0, HasAnyBits32(0x0))
        .Case(1, HasAnyBits32(0x1))
        .Case(2, HasAnyBits32(0x3))
        .Case(3, HasAnyBits32(0x80000000))
#if __SIZEOF_POINTER__ > 4
        .Case(4, HasAnyBits64(0x0))
        .Case(5, HasAnyBits64(0x1))
        .Case(6, HasAnyBits64(0x3))
        .Case(7, HasAnyBits64(0x80000000))
        .Case(8, HasAnyBits64(0x100000000ULL))
        .Case(9, HasAnyBits64(0x300000000ULL))
        .Case(10, HasAnyBits64(0x100000001ULL))
#endif
        .Default(Kill());
  }
  return Allow();
}

BPF_TEST_C(SandboxBPF, AnyBitTests, AnyBitTestPolicy) {
  // 32bit test: any of 0x0 (should always be false)
  BITMASK_TEST( 0,                   0, ANYBITS32,        0x0, EXPECT_FAILURE);
  BITMASK_TEST( 0,                   1, ANYBITS32,        0x0, EXPECT_FAILURE);
  BITMASK_TEST( 0,                   3, ANYBITS32,        0x0, EXPECT_FAILURE);
  BITMASK_TEST( 0,         0xFFFFFFFFU, ANYBITS32,        0x0, EXPECT_FAILURE);
  BITMASK_TEST( 0,                -1LL, ANYBITS32,        0x0, EXPECT_FAILURE);

  // 32bit test: any of 0x1
  BITMASK_TEST( 1,                   0, ANYBITS32,        0x1, EXPECT_FAILURE);
  BITMASK_TEST( 1,                   1, ANYBITS32,        0x1, EXPECT_SUCCESS);
  BITMASK_TEST( 1,                   2, ANYBITS32,        0x1, EXPECT_FAILURE);
  BITMASK_TEST( 1,                   3, ANYBITS32,        0x1, EXPECT_SUCCESS);

  // 32bit test: any of 0x3
  BITMASK_TEST( 2,                   0, ANYBITS32,        0x3, EXPECT_FAILURE);
  BITMASK_TEST( 2,                   1, ANYBITS32,        0x3, EXPECT_SUCCESS);
  BITMASK_TEST( 2,                   2, ANYBITS32,        0x3, EXPECT_SUCCESS);
  BITMASK_TEST( 2,                   3, ANYBITS32,        0x3, EXPECT_SUCCESS);
  BITMASK_TEST( 2,                   7, ANYBITS32,        0x3, EXPECT_SUCCESS);

  // 32bit test: any of 0x80000000
  BITMASK_TEST( 3,                   0, ANYBITS32, 0x80000000, EXPECT_FAILURE);
  BITMASK_TEST( 3,         0x40000000U, ANYBITS32, 0x80000000, EXPECT_FAILURE);
  BITMASK_TEST( 3,         0x80000000U, ANYBITS32, 0x80000000, EXPECT_SUCCESS);
  BITMASK_TEST( 3,         0xC0000000U, ANYBITS32, 0x80000000, EXPECT_SUCCESS);
  BITMASK_TEST( 3,       -0x80000000LL, ANYBITS32, 0x80000000, EXPECT_SUCCESS);

#if __SIZEOF_POINTER__ > 4
  // 64bit test: any of 0x0 (should always be false)
  BITMASK_TEST( 4,                   0, ANYBITS64,        0x0, EXPECT_FAILURE);
  BITMASK_TEST( 4,                   1, ANYBITS64,        0x0, EXPECT_FAILURE);
  BITMASK_TEST( 4,                   3, ANYBITS64,        0x0, EXPECT_FAILURE);
  BITMASK_TEST( 4,         0xFFFFFFFFU, ANYBITS64,        0x0, EXPECT_FAILURE);
  BITMASK_TEST( 4,       0x100000000LL, ANYBITS64,        0x0, EXPECT_FAILURE);
  BITMASK_TEST( 4,       0x300000000LL, ANYBITS64,        0x0, EXPECT_FAILURE);
  BITMASK_TEST( 4,0x8000000000000000LL, ANYBITS64,        0x0, EXPECT_FAILURE);
  BITMASK_TEST( 4,                -1LL, ANYBITS64,        0x0, EXPECT_FAILURE);

  // 64bit test: any of 0x1
  BITMASK_TEST( 5,                   0, ANYBITS64,        0x1, EXPECT_FAILURE);
  BITMASK_TEST( 5,                   1, ANYBITS64,        0x1, EXPECT_SUCCESS);
  BITMASK_TEST( 5,                   2, ANYBITS64,        0x1, EXPECT_FAILURE);
  BITMASK_TEST( 5,                   3, ANYBITS64,        0x1, EXPECT_SUCCESS);
  BITMASK_TEST( 5,       0x100000001LL, ANYBITS64,        0x1, EXPECT_SUCCESS);
  BITMASK_TEST( 5,       0x100000000LL, ANYBITS64,        0x1, EXPECT_FAILURE);
  BITMASK_TEST( 5,       0x100000002LL, ANYBITS64,        0x1, EXPECT_FAILURE);
  BITMASK_TEST( 5,       0x100000003LL, ANYBITS64,        0x1, EXPECT_SUCCESS);

  // 64bit test: any of 0x3
  BITMASK_TEST( 6,                   0, ANYBITS64,        0x3, EXPECT_FAILURE);
  BITMASK_TEST( 6,                   1, ANYBITS64,        0x3, EXPECT_SUCCESS);
  BITMASK_TEST( 6,                   2, ANYBITS64,        0x3, EXPECT_SUCCESS);
  BITMASK_TEST( 6,                   3, ANYBITS64,        0x3, EXPECT_SUCCESS);
  BITMASK_TEST( 6,                   7, ANYBITS64,        0x3, EXPECT_SUCCESS);
  BITMASK_TEST( 6,       0x100000000LL, ANYBITS64,        0x3, EXPECT_FAILURE);
  BITMASK_TEST( 6,       0x100000001LL, ANYBITS64,        0x3, EXPECT_SUCCESS);
  BITMASK_TEST( 6,       0x100000002LL, ANYBITS64,        0x3, EXPECT_SUCCESS);
  BITMASK_TEST( 6,       0x100000003LL, ANYBITS64,        0x3, EXPECT_SUCCESS);
  BITMASK_TEST( 6,       0x100000007LL, ANYBITS64,        0x3, EXPECT_SUCCESS);

  // 64bit test: any of 0x80000000
  BITMASK_TEST( 7,                   0, ANYBITS64, 0x80000000, EXPECT_FAILURE);
  BITMASK_TEST( 7,         0x40000000U, ANYBITS64, 0x80000000, EXPECT_FAILURE);
  BITMASK_TEST( 7,         0x80000000U, ANYBITS64, 0x80000000, EXPECT_SUCCESS);
  BITMASK_TEST( 7,         0xC0000000U, ANYBITS64, 0x80000000, EXPECT_SUCCESS);
  BITMASK_TEST( 7,       -0x80000000LL, ANYBITS64, 0x80000000, EXPECT_SUCCESS);
  BITMASK_TEST( 7,       0x100000000LL, ANYBITS64, 0x80000000, EXPECT_FAILURE);
  BITMASK_TEST( 7,       0x140000000LL, ANYBITS64, 0x80000000, EXPECT_FAILURE);
  BITMASK_TEST( 7,       0x180000000LL, ANYBITS64, 0x80000000, EXPECT_SUCCESS);
  BITMASK_TEST( 7,       0x1C0000000LL, ANYBITS64, 0x80000000, EXPECT_SUCCESS);
  BITMASK_TEST( 7,      -0x180000000LL, ANYBITS64, 0x80000000, EXPECT_SUCCESS);

  // 64bit test: any of 0x100000000
  BITMASK_TEST( 8,       0x000000000LL, ANYBITS64,0x100000000, EXPECT_FAILURE);
  BITMASK_TEST( 8,       0x100000000LL, ANYBITS64,0x100000000, EXPT64_SUCCESS);
  BITMASK_TEST( 8,       0x200000000LL, ANYBITS64,0x100000000, EXPECT_FAILURE);
  BITMASK_TEST( 8,       0x300000000LL, ANYBITS64,0x100000000, EXPT64_SUCCESS);
  BITMASK_TEST( 8,       0x000000001LL, ANYBITS64,0x100000000, EXPECT_FAILURE);
  BITMASK_TEST( 8,       0x100000001LL, ANYBITS64,0x100000000, EXPT64_SUCCESS);
  BITMASK_TEST( 8,       0x200000001LL, ANYBITS64,0x100000000, EXPECT_FAILURE);
  BITMASK_TEST( 8,       0x300000001LL, ANYBITS64,0x100000000, EXPT64_SUCCESS);

  // 64bit test: any of 0x300000000
  BITMASK_TEST( 9,       0x000000000LL, ANYBITS64,0x300000000, EXPECT_FAILURE);
  BITMASK_TEST( 9,       0x100000000LL, ANYBITS64,0x300000000, EXPT64_SUCCESS);
  BITMASK_TEST( 9,       0x200000000LL, ANYBITS64,0x300000000, EXPT64_SUCCESS);
  BITMASK_TEST( 9,       0x300000000LL, ANYBITS64,0x300000000, EXPT64_SUCCESS);
  BITMASK_TEST( 9,       0x700000000LL, ANYBITS64,0x300000000, EXPT64_SUCCESS);
  BITMASK_TEST( 9,       0x000000001LL, ANYBITS64,0x300000000, EXPECT_FAILURE);
  BITMASK_TEST( 9,       0x100000001LL, ANYBITS64,0x300000000, EXPT64_SUCCESS);
  BITMASK_TEST( 9,       0x200000001LL, ANYBITS64,0x300000000, EXPT64_SUCCESS);
  BITMASK_TEST( 9,       0x300000001LL, ANYBITS64,0x300000000, EXPT64_SUCCESS);
  BITMASK_TEST( 9,       0x700000001LL, ANYBITS64,0x300000000, EXPT64_SUCCESS);

  // 64bit test: any of 0x100000001
  BITMASK_TEST( 10,      0x000000000LL, ANYBITS64,0x100000001, EXPECT_FAILURE);
  BITMASK_TEST( 10,      0x000000001LL, ANYBITS64,0x100000001, EXPECT_SUCCESS);
  BITMASK_TEST( 10,      0x100000000LL, ANYBITS64,0x100000001, EXPT64_SUCCESS);
  BITMASK_TEST( 10,      0x100000001LL, ANYBITS64,0x100000001, EXPECT_SUCCESS);
  BITMASK_TEST( 10,        0xFFFFFFFFU, ANYBITS64,0x100000001, EXPECT_SUCCESS);
  BITMASK_TEST( 10,                -1L, ANYBITS64,0x100000001, EXPECT_SUCCESS);
#endif
}

class MaskedEqualTestPolicy : public Policy {
 public:
  MaskedEqualTestPolicy() = default;

  MaskedEqualTestPolicy(const MaskedEqualTestPolicy&) = delete;
  MaskedEqualTestPolicy& operator=(const MaskedEqualTestPolicy&) = delete;

  ~MaskedEqualTestPolicy() override = default;

  ResultExpr EvaluateSyscall(int sysno) const override;

 private:
  static ResultExpr MaskedEqual32(uint32_t mask, uint32_t value);
  static ResultExpr MaskedEqual64(uint64_t mask, uint64_t value);
};

ResultExpr MaskedEqualTestPolicy::MaskedEqual32(uint32_t mask, uint32_t value) {
  const Arg<uint32_t> arg(1);
  return If((arg & mask) == value, Error(1)).Else(Error(0));
}

ResultExpr MaskedEqualTestPolicy::MaskedEqual64(uint64_t mask, uint64_t value) {
  const Arg<uint64_t> arg(1);
  return If((arg & mask) == value, Error(1)).Else(Error(0));
}

ResultExpr MaskedEqualTestPolicy::EvaluateSyscall(int sysno) const {
  DCHECK(SandboxBPF::IsValidSyscallNumber(sysno));

  if (sysno == __NR_uname) {
    const Arg<int> option(0);
    return Switch(option)
        .Case(0, MaskedEqual32(0x00ff00ff, 0x005500aa))
#if __SIZEOF_POINTER__ > 4
        .Case(1, MaskedEqual64(0x00ff00ff00000000, 0x005500aa00000000))
        .Case(2, MaskedEqual64(0x00ff00ff00ff00ff, 0x005500aa005500aa))
#endif
        .Default(Kill());
  }

  return Allow();
}

#define MASKEQ_TEST(rulenum, arg, expected_result) \
  BPF_ASSERT(Syscall::Call(__NR_uname, (rulenum), (arg)) == (expected_result))

BPF_TEST_C(SandboxBPF, MaskedEqualTests, MaskedEqualTestPolicy) {
  // Allowed:    0x__55__aa
  MASKEQ_TEST(0, 0x00000000, EXPECT_FAILURE);
  MASKEQ_TEST(0, 0x00000001, EXPECT_FAILURE);
  MASKEQ_TEST(0, 0x00000003, EXPECT_FAILURE);
  MASKEQ_TEST(0, 0x00000100, EXPECT_FAILURE);
  MASKEQ_TEST(0, 0x00000300, EXPECT_FAILURE);
  MASKEQ_TEST(0, 0x005500aa, EXPECT_SUCCESS);
  MASKEQ_TEST(0, 0x005500ab, EXPECT_FAILURE);
  MASKEQ_TEST(0, 0x005600aa, EXPECT_FAILURE);
  MASKEQ_TEST(0, 0x005501aa, EXPECT_SUCCESS);
  MASKEQ_TEST(0, 0x005503aa, EXPECT_SUCCESS);
  MASKEQ_TEST(0, 0x555500aa, EXPECT_SUCCESS);
  MASKEQ_TEST(0, 0xaa5500aa, EXPECT_SUCCESS);

#if __SIZEOF_POINTER__ > 4
  // Allowed:    0x__55__aa________
  MASKEQ_TEST(1, 0x0000000000000000, EXPECT_FAILURE);
  MASKEQ_TEST(1, 0x0000000000000010, EXPECT_FAILURE);
  MASKEQ_TEST(1, 0x0000000000000050, EXPECT_FAILURE);
  MASKEQ_TEST(1, 0x0000000100000000, EXPECT_FAILURE);
  MASKEQ_TEST(1, 0x0000000300000000, EXPECT_FAILURE);
  MASKEQ_TEST(1, 0x0000010000000000, EXPECT_FAILURE);
  MASKEQ_TEST(1, 0x0000030000000000, EXPECT_FAILURE);
  MASKEQ_TEST(1, 0x005500aa00000000, EXPECT_SUCCESS);
  MASKEQ_TEST(1, 0x005500ab00000000, EXPECT_FAILURE);
  MASKEQ_TEST(1, 0x005600aa00000000, EXPECT_FAILURE);
  MASKEQ_TEST(1, 0x005501aa00000000, EXPECT_SUCCESS);
  MASKEQ_TEST(1, 0x005503aa00000000, EXPECT_SUCCESS);
  MASKEQ_TEST(1, 0x555500aa00000000, EXPECT_SUCCESS);
  MASKEQ_TEST(1, 0xaa5500aa00000000, EXPECT_SUCCESS);
  MASKEQ_TEST(1, 0xaa5500aa00000000, EXPECT_SUCCESS);
  MASKEQ_TEST(1, 0xaa5500aa0000cafe, EXPECT_SUCCESS);

  // Allowed:    0x__55__aa__55__aa
  MASKEQ_TEST(2, 0x0000000000000000, EXPECT_FAILURE);
  MASKEQ_TEST(2, 0x0000000000000010, EXPECT_FAILURE);
  MASKEQ_TEST(2, 0x0000000000000050, EXPECT_FAILURE);
  MASKEQ_TEST(2, 0x0000000100000000, EXPECT_FAILURE);
  MASKEQ_TEST(2, 0x0000000300000000, EXPECT_FAILURE);
  MASKEQ_TEST(2, 0x0000010000000000, EXPECT_FAILURE);
  MASKEQ_TEST(2, 0x0000030000000000, EXPECT_FAILURE);
  MASKEQ_TEST(2, 0x00000000005500aa, EXPECT_FAILURE);
  MASKEQ_TEST(2, 0x005500aa00000000, EXPECT_FAILURE);
  MASKEQ_TEST(2, 0x005500aa005500aa, EXPECT_SUCCESS);
  MASKEQ_TEST(2, 0x005500aa005700aa, EXPECT_FAILURE);
  MASKEQ_TEST(2, 0x005700aa005500aa, EXPECT_FAILURE);
  MASKEQ_TEST(2, 0x005500aa004500aa, EXPECT_FAILURE);
  MASKEQ_TEST(2, 0x004500aa005500aa, EXPECT_FAILURE);
  MASKEQ_TEST(2, 0x005512aa005500aa, EXPECT_SUCCESS);
  MASKEQ_TEST(2, 0x005500aa005534aa, EXPECT_SUCCESS);
  MASKEQ_TEST(2, 0xff5500aa0055ffaa, EXPECT_SUCCESS);
#endif
}

intptr_t PthreadTrapHandler(const struct arch_seccomp_data& args, void* aux) {
  if (args.args[0] != (CLONE_CHILD_CLEARTID | CLONE_CHILD_SETTID | SIGCHLD)) {
    // We expect to get called for an attempt to fork(). No need to log that
    // call. But if we ever get called for anything else, we want to verbosely
    // print as much information as possible.
    const char* msg = (const char*)aux;
    printf(
        "Clone() was called with unexpected arguments\n"
        "  nr: %d\n"
        "  1: 0x%llX\n"
        "  2: 0x%llX\n"
        "  3: 0x%llX\n"
        "  4: 0x%llX\n"
        "  5: 0x%llX\n"
        "  6: 0x%llX\n"
        "%s\n",
        args.nr,
        (long long)args.args[0],
        (long long)args.args[1],
        (long long)args.args[2],
        (long long)args.args[3],
        (long long)args.args[4],
        (long long)args.args[5],
        msg);
  }
  return -EPERM;
}

class PthreadPolicyEquality : public Policy {
 public:
  PthreadPolicyEquality() = default;

  PthreadPolicyEquality(const PthreadPolicyEquality&) = delete;
  PthreadPolicyEquality& operator=(const PthreadPolicyEquality&) = delete;

  ~PthreadPolicyEquality() override = default;

  ResultExpr EvaluateSyscall(int sysno) const override;
};

ResultExpr PthreadPolicyEquality::EvaluateSyscall(int sysno) const {
  DCHECK(SandboxBPF::IsValidSyscallNumber(sysno));
  // This policy allows creating threads with pthread_create(). But it
  // doesn't allow any other uses of clone(). Most notably, it does not
  // allow callers to implement fork() or vfork() by passing suitable flags
  // to the clone() system call.
  if (sysno == __NR_clone) {
    // We have seen two different valid combinations of flags. Glibc
    // uses the more modern flags, sets the TLS from the call to clone(), and
    // uses futexes to monitor threads. Android's C run-time library, doesn't
    // do any of this, but it sets the obsolete (and no-op) CLONE_DETACHED.
    // More recent versions of Android don't set CLONE_DETACHED anymore, so
    // the last case accounts for that.
    // The following policy is very strict. It only allows the exact masks
    // that we have seen in known implementations. It is probably somewhat
    // stricter than what we would want to do.
    const uint64_t kGlibcCloneMask = CLONE_VM | CLONE_FS | CLONE_FILES |
                                     CLONE_SIGHAND | CLONE_THREAD |
                                     CLONE_SYSVSEM | CLONE_SETTLS |
                                     CLONE_PARENT_SETTID | CLONE_CHILD_CLEARTID;
    const uint64_t kBaseAndroidCloneMask = CLONE_VM | CLONE_FS | CLONE_FILES |
                                           CLONE_SIGHAND | CLONE_THREAD |
                                           CLONE_SYSVSEM;
    const Arg<unsigned long> flags(0);
    return Switch(flags)
        .Cases({kGlibcCloneMask, (kBaseAndroidCloneMask | CLONE_DETACHED),
                kBaseAndroidCloneMask},
               Allow())
        .Default(Trap(PthreadTrapHandler, "Unknown mask"));
  }

  return Allow();
}

class PthreadPolicyBitMask : public Policy {
 public:
  PthreadPolicyBitMask() = default;

  PthreadPolicyBitMask(const PthreadPolicyBitMask&) = delete;
  PthreadPolicyBitMask& operator=(const PthreadPolicyBitMask&) = delete;

  ~PthreadPolicyBitMask() override = default;

  ResultExpr EvaluateSyscall(int sysno) const override;

 private:
  static BoolExpr HasAnyBits(const Arg<unsigned long>& arg, unsigned long bits);
  static BoolExpr HasAllBits(const Arg<unsigned long>& arg, unsigned long bits);
};

BoolExpr PthreadPolicyBitMask::HasAnyBits(const Arg<unsigned long>& arg,
                                          unsigned long bits) {
  return (arg & bits) != 0;
}

BoolExpr PthreadPolicyBitMask::HasAllBits(const Arg<unsigned long>& arg,
                                          unsigned long bits) {
  return (arg & bits) == bits;
}

ResultExpr PthreadPolicyBitMask::EvaluateSyscall(int sysno) const {
  DCHECK(SandboxBPF::IsValidSyscallNumber(sysno));
  // This policy allows creating threads with pthread_create(). But it
  // doesn't allow any other uses of clone(). Most notably, it does not
  // allow callers to implement fork() or vfork() by passing suitable flags
  // to the clone() system call.
  if (sysno == __NR_clone) {
    // We have seen two different valid combinations of flags. Glibc
    // uses the more modern flags, sets the TLS from the call to clone(), and
    // uses futexes to monitor threads. Android's C run-time library, doesn't
    // do any of this, but it sets the obsolete (and no-op) CLONE_DETACHED.
    // The following policy allows for either combination of flags, but it
    // is generally a little more conservative than strictly necessary. We
    // err on the side of rather safe than sorry.
    // Very noticeably though, we disallow fork() (which is often just a
    // wrapper around clone()).
    const unsigned long kMandatoryFlags = CLONE_VM | CLONE_FS | CLONE_FILES |
                                          CLONE_SIGHAND | CLONE_THREAD |
                                          CLONE_SYSVSEM;
    const unsigned long kFutexFlags =
        CLONE_SETTLS | CLONE_PARENT_SETTID | CLONE_CHILD_CLEARTID;
    const unsigned long kNoopFlags = CLONE_DETACHED;
    const unsigned long kKnownFlags =
        kMandatoryFlags | kFutexFlags | kNoopFlags;

    const Arg<unsigned long> flags(0);
    return If(HasAnyBits(flags, ~kKnownFlags),
              Trap(PthreadTrapHandler, "Unexpected CLONE_XXX flag found"))
        .ElseIf(Not(HasAllBits(flags, kMandatoryFlags)),
                Trap(PthreadTrapHandler,
                     "Missing mandatory CLONE_XXX flags "
                     "when creating new thread"))
        .ElseIf(AllOf(Not(HasAllBits(flags, kFutexFlags)),
                      HasAnyBits(flags, kFutexFlags)),
                Trap(PthreadTrapHandler,
                     "Must set either all or none of the TLS and futex bits in "
                     "call to clone()"))
        .Else(Allow());
  }

  return Allow();
}

static void* ThreadFnc(void* arg) {
  ++*reinterpret_cast<int*>(arg);
  Syscall::Call(__NR_futex, arg, FUTEX_WAKE, 1, 0, 0, 0);
  return nullptr;
}

static void PthreadTest() {
  // Attempt to start a joinable thread. This should succeed.
  pthread_t thread;
  int thread_ran = 0;
  BPF_ASSERT(!pthread_create(&thread, nullptr, ThreadFnc, &thread_ran));
  BPF_ASSERT(!pthread_join(thread, nullptr));
  BPF_ASSERT(thread_ran);

  // Attempt to start a detached thread. This should succeed.
  thread_ran = 0;
  pthread_attr_t attr;
  BPF_ASSERT(!pthread_attr_init(&attr));
  BPF_ASSERT(!pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED));
  BPF_ASSERT(!pthread_create(&thread, &attr, ThreadFnc, &thread_ran));
  BPF_ASSERT(!pthread_attr_destroy(&attr));
  while (Syscall::Call(__NR_futex, &thread_ran, FUTEX_WAIT, 0, 0, 0, 0) ==
         -EINTR) {
  }
  BPF_ASSERT(thread_ran);

  // Attempt to fork() a process using clone(). This should fail. We use the
  // same flags that glibc uses when calling fork(). But we don't actually
  // try calling the fork() implementation in the C run-time library, as
  // run-time libraries other than glibc might call __NR_fork instead of
  // __NR_clone, and that would introduce a bogus test failure.
  int pid;
  BPF_ASSERT(Syscall::Call(__NR_clone,
                           CLONE_CHILD_CLEARTID | CLONE_CHILD_SETTID | SIGCHLD,
                           0,
                           0,
                           &pid) == -EPERM);
}

BPF_TEST_C(SandboxBPF, PthreadEquality, PthreadPolicyEquality) {
  PthreadTest();
}

BPF_TEST_C(SandboxBPF, PthreadBitMask, PthreadPolicyBitMask) {
  PthreadTest();
}

// libc might not define these even though the kernel supports it.
#ifndef PTRACE_O_TRACESECCOMP
#define PTRACE_O_TRACESECCOMP 0x00000080
#endif

#ifdef PTRACE_EVENT_SECCOMP
#define IS_SECCOMP_EVENT(status) ((status >> 16) == PTRACE_EVENT_SECCOMP)
#else
// When Debian/Ubuntu backported seccomp-bpf support into earlier kernels, they
// changed the value of PTRACE_EVENT_SECCOMP from 7 to 8, since 7 was taken by
// PTRACE_EVENT_STOP (upstream chose to renumber PTRACE_EVENT_STOP to 128).  If
// PTRACE_EVENT_SECCOMP isn't defined, we have no choice but to consider both
// values here.
#define IS_SECCOMP_EVENT(status) ((status >> 16) == 7 || (status >> 16) == 8)
#endif

#if defined(__arm__)
#ifndef PTRACE_SET_SYSCALL
#define PTRACE_SET_SYSCALL 23
#endif
#endif

#if defined(__aarch64__)
#ifndef PTRACE_GETREGS
#if defined(__GLIBC__)
#define PTRACE_GETREGS static_cast<enum __ptrace_request>(12)
#else
#define PTRACE_GETREGS 12
#endif  // defined(__GLIBC__)
#endif  // !defined(PTRACE_GETREGS)
#endif  // defined(__aarch64__)

#if defined(__aarch64__)
#ifndef PTRACE_SETREGS
#if defined(__GLIBC__)
#define PTRACE_SETREGS static_cast<enum __ptrace_request>(13)
#else
#define PTRACE_SETREGS 13
#endif  // defined(__GLIBC__)
#endif  // !defined(PTRACE_SETREGS)
#endif  // defined(__aarch64__)

// Changes the syscall to run for a child being sandboxed using seccomp-bpf with
// PTRACE_O_TRACESECCOMP.  Should only be called when the child is stopped on
// PTRACE_EVENT_SECCOMP.
//
// regs should contain the current set of registers of the child, obtained using
// PTRACE_GETREGS.
//
// Depending on the architecture, this may modify regs, so the caller is
// responsible for committing these changes using PTRACE_SETREGS.
#if !defined(__arm__) && !defined(__aarch64__) && !defined(__mips__)
long SetSyscall(pid_t pid, regs_struct* regs, int syscall_number) {
#if defined(__arm__)
  // On ARM, the syscall is changed using PTRACE_SET_SYSCALL.  We cannot use the
  // libc ptrace call as the request parameter is an enum, and
  // PTRACE_SET_SYSCALL may not be in the enum.
  return syscall(__NR_ptrace, PTRACE_SET_SYSCALL, pid, NULL, syscall_number);
#else
  SECCOMP_PT_SYSCALL(*regs) = syscall_number;
  return 0;
#endif
}
#endif

const uint16_t kTraceData = 0xcc;

class TraceAllPolicy : public Policy {
 public:
  TraceAllPolicy() = default;

  TraceAllPolicy(const TraceAllPolicy&) = delete;
  TraceAllPolicy& operator=(const TraceAllPolicy&) = delete;

  ~TraceAllPolicy() override = default;

  ResultExpr EvaluateSyscall(int system_call_number) const override {
    return Trace(kTraceData);
  }
};

SANDBOX_TEST(SandboxBPF, DISABLE_ON_TSAN(SeccompRetTrace)) {
  if (!SandboxBPF::SupportsSeccompSandbox(
          SandboxBPF::SeccompLevel::SINGLE_THREADED)) {
    return;
  }

// This test is disabled on arm due to a kernel bug.
// See https://code.google.com/p/chromium/issues/detail?id=383977
#if defined(__arm__) || defined(__aarch64__)
  printf("This test is currently disabled on ARM32/64 due to a kernel bug.");
#elif defined(__mips__)
  // TODO: Figure out how to support specificity of handling indirect syscalls
  //        in this test and enable it.
  printf("This test is currently disabled on MIPS.");
#else
  pid_t pid = fork();
  BPF_ASSERT_NE(-1, pid);
  if (pid == 0) {
    pid_t my_pid = getpid();
    BPF_ASSERT_NE(-1, ptrace(PTRACE_TRACEME, -1, NULL, NULL));
    BPF_ASSERT_EQ(0, raise(SIGSTOP));
    SandboxBPF sandbox(std::make_unique<TraceAllPolicy>());
    BPF_ASSERT(sandbox.StartSandbox(SandboxBPF::SeccompLevel::SINGLE_THREADED));

    // getpid is allowed.
    BPF_ASSERT_EQ(my_pid, sys_getpid());

    // write to stdout is skipped and returns a fake value.
    BPF_ASSERT_EQ(kExpectedReturnValue,
                  syscall(__NR_write, STDOUT_FILENO, "A", 1));

    // kill is rewritten to exit(kExpectedReturnValue).
    syscall(__NR_kill, my_pid, SIGKILL);

    // Should not be reached.
    BPF_ASSERT(false);
  }

  int status;
  BPF_ASSERT(HANDLE_EINTR(waitpid(pid, &status, WUNTRACED)) != -1);
  BPF_ASSERT(WIFSTOPPED(status));

  BPF_ASSERT_NE(-1,
                ptrace(PTRACE_SETOPTIONS,
                       pid,
                       NULL,
                       reinterpret_cast<void*>(PTRACE_O_TRACESECCOMP)));
  BPF_ASSERT_NE(-1, ptrace(PTRACE_CONT, pid, NULL, NULL));
  while (true) {
    BPF_ASSERT(HANDLE_EINTR(waitpid(pid, &status, 0)) != -1);
    if (WIFEXITED(status) || WIFSIGNALED(status)) {
      BPF_ASSERT(WIFEXITED(status));
      BPF_ASSERT_EQ(kExpectedReturnValue, WEXITSTATUS(status));
      break;
    }

    if (!WIFSTOPPED(status) || WSTOPSIG(status) != SIGTRAP ||
        !IS_SECCOMP_EVENT(status)) {
      BPF_ASSERT_NE(-1, ptrace(PTRACE_CONT, pid, NULL, NULL));
      continue;
    }

    unsigned long data;
    BPF_ASSERT_NE(-1, ptrace(PTRACE_GETEVENTMSG, pid, NULL, &data));
    BPF_ASSERT_EQ(kTraceData, data);

    regs_struct regs;
    BPF_ASSERT_NE(-1, ptrace(PTRACE_GETREGS, pid, NULL, &regs));
    switch (SECCOMP_PT_SYSCALL(regs)) {
      case __NR_write:
        // Skip writes to stdout, make it return kExpectedReturnValue.  Allow
        // writes to stderr so that BPF_ASSERT messages show up.
        if (SECCOMP_PT_PARM1(regs) == STDOUT_FILENO) {
          BPF_ASSERT_NE(-1, SetSyscall(pid, &regs, -1));
          SECCOMP_PT_RESULT(regs) = kExpectedReturnValue;
          BPF_ASSERT_NE(-1, ptrace(PTRACE_SETREGS, pid, NULL, &regs));
        }
        break;

      case __NR_kill:
        // Rewrite to exit(kExpectedReturnValue).
        BPF_ASSERT_NE(-1, SetSyscall(pid, &regs, __NR_exit));
        SECCOMP_PT_PARM1(regs) = kExpectedReturnValue;
        BPF_ASSERT_NE(-1, ptrace(PTRACE_SETREGS, pid, NULL, &regs));
        break;

      default:
        // Allow all other syscalls.
        break;
    }

    BPF_ASSERT_NE(-1, ptrace(PTRACE_CONT, pid, NULL, NULL));
  }
#endif
}

// Android does not expose pread64 nor pwrite64.
#if !BUILDFLAG(IS_ANDROID)

bool FullPwrite64(int fd, const char* buffer, size_t count, off64_t offset) {
  while (count > 0) {
    const ssize_t transfered =
        HANDLE_EINTR(pwrite64(fd, buffer, count, offset));
    if (transfered <= 0 || static_cast<size_t>(transfered) > count) {
      return false;
    }
    count -= transfered;
    buffer += transfered;
    offset += transfered;
  }
  return true;
}

bool FullPread64(int fd, char* buffer, size_t count, off64_t offset) {
  while (count > 0) {
    const ssize_t transfered = HANDLE_EINTR(pread64(fd, buffer, count, offset));
    if (transfered <= 0 || static_cast<size_t>(transfered) > count) {
      return false;
    }
    count -= transfered;
    buffer += transfered;
    offset += transfered;
  }
  return true;
}

bool pread_64_was_forwarded = false;

class TrapPread64Policy : public Policy {
 public:
  TrapPread64Policy() = default;

  TrapPread64Policy(const TrapPread64Policy&) = delete;
  TrapPread64Policy& operator=(const TrapPread64Policy&) = delete;

  ~TrapPread64Policy() override = default;

  ResultExpr EvaluateSyscall(int system_call_number) const override {
    // Set the global environment for unsafe traps once.
    if (system_call_number == MIN_SYSCALL) {
      EnableUnsafeTraps();
    }

    if (system_call_number == __NR_pread64) {
      return UnsafeTrap(ForwardPreadHandler, nullptr);
    }
    return Allow();
  }

 private:
  static intptr_t ForwardPreadHandler(const struct arch_seccomp_data& args,
                                      void* aux) {
    BPF_ASSERT(args.nr == __NR_pread64);
    pread_64_was_forwarded = true;

    return SandboxBPF::ForwardSyscall(args);
  }
};

// pread(2) takes a 64 bits offset. On 32 bits systems, it will be split
// between two arguments. In this test, we make sure that ForwardSyscall() can
// forward it properly.
BPF_TEST_C(SandboxBPF, Pread64, TrapPread64Policy) {
  ScopedTemporaryFile temp_file;
  const uint64_t kLargeOffset = (static_cast<uint64_t>(1) << 32) | 0xBEEF;
  const char kTestString[] = "This is a test!";
  BPF_ASSERT(FullPwrite64(
      temp_file.fd(), kTestString, sizeof(kTestString), kLargeOffset));

  char read_test_string[sizeof(kTestString)] = {0};
  BPF_ASSERT(FullPread64(temp_file.fd(),
                         read_test_string,
                         sizeof(read_test_string),
                         kLargeOffset));
  BPF_ASSERT_EQ(0, memcmp(kTestString, read_test_string, sizeof(kTestString)));
  BPF_ASSERT(pread_64_was_forwarded);
}

#endif  // !BUILDFLAG(IS_ANDROID)

void* TsyncApplyToTwoThreadsFunc(void* cond_ptr) {
  base::WaitableEvent* event = static_cast<base::WaitableEvent*>(cond_ptr);

  // Wait for the main thread to signal that the filter has been applied.
  if (!event->IsSignaled()) {
    event->Wait();
  }

  BPF_ASSERT(event->IsSignaled());

  DenylistNanosleepPolicy::AssertNanosleepFails();

  return nullptr;
}

SANDBOX_TEST(SandboxBPF, Tsync) {
  const bool supports_multi_threaded = SandboxBPF::SupportsSeccompSandbox(
      SandboxBPF::SeccompLevel::MULTI_THREADED);
// On Chrome OS tsync is mandatory.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (base::SysInfo::IsRunningOnChromeOS()) {
    BPF_ASSERT_EQ(true, supports_multi_threaded);
  }
// else a Chrome OS build not running on a Chrome OS device e.g. Chrome bots.
// In this case fall through.
#endif
  if (!supports_multi_threaded) {
    return;
  }

  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);

  // Create a thread on which to invoke the blocked syscall.
  pthread_t thread;
  BPF_ASSERT_EQ(
      0, pthread_create(&thread, nullptr, &TsyncApplyToTwoThreadsFunc, &event));

  // Test that nanoseelp success.
  const struct timespec ts = {0, 0};
  BPF_ASSERT_EQ(0, HANDLE_EINTR(syscall(__NR_nanosleep, &ts, NULL)));

  // Engage the sandbox.
  SandboxBPF sandbox(std::make_unique<DenylistNanosleepPolicy>());
  BPF_ASSERT(sandbox.StartSandbox(SandboxBPF::SeccompLevel::MULTI_THREADED));

  // This thread should have the filter applied as well.
  DenylistNanosleepPolicy::AssertNanosleepFails();

  // Signal the condition to invoke the system call.
  event.Signal();

  // Wait for the thread to finish.
  BPF_ASSERT_EQ(0, pthread_join(thread, nullptr));
}

class AllowAllPolicy : public Policy {
 public:
  AllowAllPolicy() = default;

  AllowAllPolicy(const AllowAllPolicy&) = delete;
  AllowAllPolicy& operator=(const AllowAllPolicy&) = delete;

  ~AllowAllPolicy() override = default;

  ResultExpr EvaluateSyscall(int sysno) const override { return Allow(); }
};

SANDBOX_DEATH_TEST(
    SandboxBPF,
    StartMultiThreadedAsSingleThreaded,
    DEATH_MESSAGE(
        ThreadHelpers::GetAssertSingleThreadedErrorMessageForTests())) {
  base::Thread thread("sandbox.linux.StartMultiThreadedAsSingleThreaded");
  BPF_ASSERT(thread.Start());

  SandboxBPF sandbox(std::make_unique<AllowAllPolicy>());
  BPF_ASSERT(!sandbox.StartSandbox(SandboxBPF::SeccompLevel::SINGLE_THREADED));
}

// A stub handler for the UnsafeTrap. Never called.
intptr_t NoOpHandler(const struct arch_seccomp_data& args, void*) {
  return -1;
}

class UnsafeTrapWithCondPolicy : public Policy {
 public:
  UnsafeTrapWithCondPolicy() = default;

  UnsafeTrapWithCondPolicy(const UnsafeTrapWithCondPolicy&) = delete;
  UnsafeTrapWithCondPolicy& operator=(const UnsafeTrapWithCondPolicy&) = delete;

  ~UnsafeTrapWithCondPolicy() override = default;

  ResultExpr EvaluateSyscall(int sysno) const override {
    DCHECK(SandboxBPF::IsValidSyscallNumber(sysno));
    setenv(kSandboxDebuggingEnv, "t", 0);
    Die::SuppressInfoMessages(true);

    if (SandboxBPF::IsRequiredForUnsafeTrap(sysno))
      return Allow();

    if (IsSyscallForTestHarness(sysno))
      return Allow();

    switch (sysno) {
      case __NR_uname: {
        const Arg<uint32_t> arg(0);
        return If(arg == 0, Allow()).Else(Error(EPERM));
      }
      case __NR_setgid: {
        const Arg<uint32_t> arg(0);
        return Switch(arg)
            .Case(100, Error(ENOMEM))
            .Case(200, Error(ENOSYS))
            .Default(Error(EPERM));
      }
      case __NR_close:
        return Allow();
      case __NR_getppid:
        return UnsafeTrap(NoOpHandler, nullptr);
      default:
        return Error(EPERM);
    }
  }
};

BPF_TEST_C(SandboxBPF, UnsafeTrapWithCond, UnsafeTrapWithCondPolicy) {
  BPF_ASSERT_EQ(-1, syscall(__NR_uname, 0));
  BPF_ASSERT_EQ(EFAULT, errno);

  BPF_ASSERT_EQ(-1, syscall(__NR_uname, 1));
  BPF_ASSERT_EQ(EPERM, errno);

  BPF_ASSERT_EQ(-1, syscall(__NR_setgid, 100));
  BPF_ASSERT_EQ(ENOMEM, errno);

  BPF_ASSERT_EQ(-1, syscall(__NR_setgid, 200));
  BPF_ASSERT_EQ(ENOSYS, errno);

  BPF_ASSERT_EQ(-1, syscall(__NR_setgid, 300));
  BPF_ASSERT_EQ(EPERM, errno);
}

}  // namespace

}  // namespace bpf_dsl
}  // namespace sandbox
