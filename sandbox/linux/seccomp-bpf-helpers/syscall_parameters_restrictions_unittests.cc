// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/seccomp-bpf-helpers/syscall_parameters_restrictions.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/elf.h>
#include <sched.h>
#include <sys/prctl.h>
#include <sys/ptrace.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/user.h>
#include <time.h>
#include <unistd.h>

#include "base/functional/bind.h"
#include "base/posix/eintr_wrapper.h"
#include "base/synchronization/waitable_event.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/bpf_dsl/policy.h"
#include "sandbox/linux/seccomp-bpf-helpers/sigsys_handlers.h"
#include "sandbox/linux/seccomp-bpf/bpf_tests.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/seccomp-bpf/syscall.h"
#include "sandbox/linux/services/syscall_wrappers.h"
#include "sandbox/linux/system_headers/linux_ptrace.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"
#include "sandbox/linux/system_headers/linux_time.h"
#include "sandbox/linux/tests/unit_tests.h"

namespace sandbox {

namespace {

// NOTE: most of the parameter restrictions are tested in
// baseline_policy_unittest.cc as a more end-to-end test.

using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::ResultExpr;

class RestrictClockIdPolicy : public bpf_dsl::Policy {
 public:
  RestrictClockIdPolicy() {}
  ~RestrictClockIdPolicy() override {}

  ResultExpr EvaluateSyscall(int sysno) const override {
    switch (sysno) {
      case __NR_clock_gettime:
#if defined(__NR_clock_gettime64)
      case __NR_clock_gettime64:
#endif
      case __NR_clock_getres:
      case __NR_clock_nanosleep:
#if defined(__NR_clock_nanosleep_time64)
      case __NR_clock_nanosleep_time64:
#endif
        return RestrictClockID();
      default:
        return Allow();
    }
  }
};

void CheckClock(clockid_t clockid) {
  struct timespec ts;
  ts.tv_sec = -1;
  ts.tv_nsec = -1;
  BPF_ASSERT_EQ(0, clock_getres(clockid, &ts));
  BPF_ASSERT_EQ(0, ts.tv_sec);
  BPF_ASSERT_LE(0, ts.tv_nsec);
  ts.tv_sec = -1;
  ts.tv_nsec = -1;
  BPF_ASSERT_EQ(0, clock_gettime(clockid, &ts));
  BPF_ASSERT_LE(0, ts.tv_sec);
  BPF_ASSERT_LE(0, ts.tv_nsec);
}

BPF_TEST_C(ParameterRestrictions,
           clock_gettime_allowed,
           RestrictClockIdPolicy) {
  CheckClock(CLOCK_MONOTONIC);
  CheckClock(CLOCK_MONOTONIC_COARSE);
  CheckClock(CLOCK_MONOTONIC_RAW);
  CheckClock(CLOCK_PROCESS_CPUTIME_ID);
  CheckClock(CLOCK_BOOTTIME);
  CheckClock(CLOCK_REALTIME);
  CheckClock(CLOCK_REALTIME_COARSE);
  CheckClock(CLOCK_THREAD_CPUTIME_ID);
#if BUILDFLAG(IS_ANDROID)
  clockid_t clock_id;
  pthread_getcpuclockid(pthread_self(), &clock_id);
  CheckClock(clock_id);
#endif
}

void CheckClockNanosleep(clockid_t clockid) {
  struct timespec ts;
  struct timespec out_ts;
  ts.tv_sec = 0;
  ts.tv_nsec = 0;
  clock_nanosleep(clockid, 0, &ts, &out_ts);
}

BPF_TEST_C(ParameterRestrictions,
           clock_nanosleep_allowed,
           RestrictClockIdPolicy) {
  CheckClockNanosleep(CLOCK_MONOTONIC);
  CheckClockNanosleep(CLOCK_MONOTONIC_COARSE);
  CheckClockNanosleep(CLOCK_MONOTONIC_RAW);
  CheckClockNanosleep(CLOCK_BOOTTIME);
  CheckClockNanosleep(CLOCK_REALTIME);
  CheckClockNanosleep(CLOCK_REALTIME_COARSE);
}

BPF_DEATH_TEST_C(ParameterRestrictions,
                 clock_gettime_crash_clock_fd,
                 DEATH_SEGV_MESSAGE(sandbox::GetErrorMessageContentForTests()),
                 RestrictClockIdPolicy) {
  struct timespec ts;
  syscall(SYS_clock_gettime, (~0) | CLOCKFD, &ts);
}

BPF_DEATH_TEST_C(ParameterRestrictions,
                 clock_nanosleep_crash_clock_fd,
                 DEATH_SEGV_MESSAGE(sandbox::GetErrorMessageContentForTests()),
                 RestrictClockIdPolicy) {
  struct timespec ts;
  struct timespec out_ts;
  ts.tv_sec = 0;
  ts.tv_nsec = 0;
  syscall(SYS_clock_nanosleep, (~0) | CLOCKFD, 0, &ts, &out_ts);
}

#if !BUILDFLAG(IS_ANDROID)
BPF_DEATH_TEST_C(ParameterRestrictions,
                 clock_gettime_crash_cpu_clock,
                 DEATH_SEGV_MESSAGE(sandbox::GetErrorMessageContentForTests()),
                 RestrictClockIdPolicy) {
  // We can't use clock_getcpuclockid() because it's not implemented in newlib,
  // and it might not work inside the sandbox anyway.
  const pid_t kInitPID = 1;
  const clockid_t kInitCPUClockID =
      MAKE_PROCESS_CPUCLOCK(kInitPID, CPUCLOCK_SCHED);

  struct timespec ts;
  clock_gettime(kInitCPUClockID, &ts);
}
#endif  // !BUILDFLAG(IS_ANDROID)

class RestrictSchedPolicy : public bpf_dsl::Policy {
 public:
  RestrictSchedPolicy() {}
  ~RestrictSchedPolicy() override {}

  ResultExpr EvaluateSyscall(int sysno) const override {
    switch (sysno) {
      case __NR_sched_getparam:
        return RestrictSchedTarget(getpid(), sysno);
      default:
        return Allow();
    }
  }
};

void CheckSchedGetParam(pid_t pid, struct sched_param* param) {
  BPF_ASSERT_EQ(0, sched_getparam(pid, param));
}

void SchedGetParamThread(base::WaitableEvent* thread_run) {
  const pid_t pid = getpid();
  const pid_t tid = sys_gettid();
  BPF_ASSERT_NE(pid, tid);

  struct sched_param current_pid_param;
  CheckSchedGetParam(pid, &current_pid_param);

  struct sched_param zero_param;
  CheckSchedGetParam(0, &zero_param);

  struct sched_param tid_param;
  CheckSchedGetParam(tid, &tid_param);

  BPF_ASSERT_EQ(zero_param.sched_priority, tid_param.sched_priority);

  // Verify that the SIGSYS handler sets errno properly.
  errno = 0;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonnull"
  BPF_ASSERT_EQ(-1, sched_getparam(tid, NULL));
#pragma clang diagnostic pop
  BPF_ASSERT_EQ(EINVAL, errno);

  thread_run->Signal();
}

BPF_TEST_C(ParameterRestrictions,
           sched_getparam_allowed,
           RestrictSchedPolicy) {
  base::WaitableEvent thread_run(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  // Run the actual test in a new thread so that the current pid and tid are
  // different.
  base::Thread getparam_thread("sched_getparam_thread");
  BPF_ASSERT(getparam_thread.Start());
  getparam_thread.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&SchedGetParamThread, &thread_run));
  BPF_ASSERT(thread_run.TimedWait(base::Milliseconds(5000)));
  getparam_thread.Stop();
}

BPF_DEATH_TEST_C(ParameterRestrictions,
                 sched_getparam_crash_non_zero,
                 DEATH_SEGV_MESSAGE(sandbox::GetErrorMessageContentForTests()),
                 RestrictSchedPolicy) {
  const pid_t kInitPID = 1;
  struct sched_param param;
  sched_getparam(kInitPID, &param);
}

class RestrictPrlimit64Policy : public bpf_dsl::Policy {
 public:
  RestrictPrlimit64Policy() {}
  ~RestrictPrlimit64Policy() override {}

  ResultExpr EvaluateSyscall(int sysno) const override {
    switch (sysno) {
      case __NR_prlimit64:
        return RestrictPrlimit64(getpid());
      default:
        return Allow();
    }
  }
};

BPF_TEST_C(ParameterRestrictions, prlimit64_allowed, RestrictPrlimit64Policy) {
  BPF_ASSERT_EQ(0, sys_prlimit64(0, RLIMIT_AS, NULL, NULL));
  BPF_ASSERT_EQ(0, sys_prlimit64(getpid(), RLIMIT_AS, NULL, NULL));
}

BPF_DEATH_TEST_C(ParameterRestrictions,
                 prlimit64_crash_not_self,
                 DEATH_SEGV_MESSAGE(sandbox::GetErrorMessageContentForTests()),
                 RestrictPrlimit64Policy) {
  const pid_t kInitPID = 1;
  BPF_ASSERT_NE(kInitPID, getpid());
  sys_prlimit64(kInitPID, RLIMIT_AS, NULL, NULL);
}

class RestrictGetrusagePolicy : public bpf_dsl::Policy {
 public:
  RestrictGetrusagePolicy() {}
  ~RestrictGetrusagePolicy() override {}

  ResultExpr EvaluateSyscall(int sysno) const override {
    switch (sysno) {
      case __NR_getrusage:
        return RestrictGetrusage();
      default:
        return Allow();
    }
  }
};

BPF_TEST_C(ParameterRestrictions, getrusage_allowed, RestrictGetrusagePolicy) {
  struct rusage usage;
  BPF_ASSERT_EQ(0, getrusage(RUSAGE_SELF, &usage));
}

BPF_DEATH_TEST_C(ParameterRestrictions,
                 getrusage_crash_not_self,
                 DEATH_SEGV_MESSAGE(sandbox::GetErrorMessageContentForTests()),
                 RestrictGetrusagePolicy) {
  struct rusage usage;
  getrusage(RUSAGE_CHILDREN, &usage);
}

// The following ptrace() tests do not actually set up a tracer/tracee
// relationship, so allowed operations return ESRCH errors. Blocked operations
// are tested with death tests.

class RestrictPtracePolicy : public bpf_dsl::Policy {
 public:
  RestrictPtracePolicy() = default;
  ~RestrictPtracePolicy() override = default;

  ResultExpr EvaluateSyscall(int sysno) const override {
    switch (sysno) {
      case __NR_ptrace:
        return RestrictPtrace();
      default:
        return Allow();
    }
  }
};

BPF_TEST_C(ParameterRestrictions,
           ptrace_getregs_allowed,
           RestrictPtracePolicy) {
#if defined(__arm__)
  user_regs regs;
#else
  user_regs_struct regs;
#endif
  iovec iov;
  iov.iov_base = &regs;
  iov.iov_len = sizeof(regs);
  errno = 0;
  int rv = ptrace(PTRACE_GETREGSET, getpid(),
                  reinterpret_cast<void*>(NT_PRSTATUS), &iov);
  BPF_ASSERT_EQ(-1, rv);
  BPF_ASSERT_EQ(ESRCH, errno);
}

BPF_DEATH_TEST_C(
    ParameterRestrictions,
    ptrace_syscall_blocked,
    DEATH_SEGV_MESSAGE(sandbox::GetPtraceErrorMessageContentForTests()),
    RestrictPtracePolicy) {
  ptrace(PTRACE_SYSCALL, getpid(), nullptr, nullptr);
}

BPF_DEATH_TEST_C(
    ParameterRestrictions,
    ptrace_setregs_blocked,
    DEATH_SEGV_MESSAGE(sandbox::GetPtraceErrorMessageContentForTests()),
    RestrictPtracePolicy) {
#if defined(__arm__)
  user_regs regs{};
#else
  user_regs_struct regs{};
#endif
  iovec iov;
  iov.iov_base = &regs;
  iov.iov_len = sizeof(regs);
  errno = 0;
  ptrace(PTRACE_SETREGSET, getpid(), reinterpret_cast<void*>(NT_PRSTATUS),
         &iov);
}

#if defined(__aarch64__)
BPF_DEATH_TEST_C(
    ParameterRestrictions,
    ptrace_getregs_nt_arm_paca_keys_blocked,
    DEATH_SEGV_MESSAGE(sandbox::GetPtraceErrorMessageContentForTests()),
    RestrictPtracePolicy) {
  user_regs_struct regs{};
  iovec iov;
  iov.iov_base = &regs;
  iov.iov_len = sizeof(regs);
  errno = 0;
  ptrace(PTRACE_GETREGSET, getpid(), reinterpret_cast<void*>(NT_ARM_PACA_KEYS),
         &iov);
}

BPF_DEATH_TEST_C(
    ParameterRestrictions,
    ptrace_getregs_nt_arm_pacg_keys_blocked,
    DEATH_SEGV_MESSAGE(sandbox::GetPtraceErrorMessageContentForTests()),
    RestrictPtracePolicy) {
  user_regs_struct regs{};
  iovec iov;
  iov.iov_base = &regs;
  iov.iov_len = sizeof(regs);
  errno = 0;
  ptrace(PTRACE_GETREGSET, getpid(), reinterpret_cast<void*>(NT_ARM_PACG_KEYS),
         &iov);
}
#endif

}  // namespace

}  // namespace sandbox
