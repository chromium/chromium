// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/seccomp-bpf-helpers/syscall_parameters_restrictions.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/net.h>
#include <sched.h>
#include <signal.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/ptrace.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "base/notreached.h"
#include "base/synchronization/synchronization_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/bpf_dsl/seccomp_macros.h"
#include "sandbox/linux/seccomp-bpf-helpers/sigsys_handlers.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/system_headers/linux_futex.h"
#include "sandbox/linux/system_headers/linux_prctl.h"
#include "sandbox/linux/system_headers/linux_ptrace.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"
#include "sandbox/linux/system_headers/linux_time.h"

#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) && \
    !defined(__arm__) && !defined(__aarch64__) &&             \
    !defined(PTRACE_GET_THREAD_AREA)
// Also include asm/ptrace-abi.h since ptrace.h in older libc (for instance
// the one in Ubuntu 16.04 LTS) is missing PTRACE_GET_THREAD_AREA.
// asm/ptrace-abi.h doesn't exist on arm32 and PTRACE_GET_THREAD_AREA isn't
// defined on aarch64, so don't try to include this on those platforms.
#include <asm/ptrace-abi.h>
#endif

#if BUILDFLAG(IS_ANDROID)

#if !defined(F_DUPFD_CLOEXEC)
#define F_DUPFD_CLOEXEC (F_LINUX_SPECIFIC_BASE + 6)
#endif

#endif  // BUILDFLAG(IS_ANDROID)

#if defined(__arm__) && !defined(MAP_STACK)
#define MAP_STACK 0x20000  // Daisy build environment has old headers.
#endif

#if defined(__mips__) && !defined(MAP_STACK)
#define MAP_STACK 0x40000
#endif

// Temporary definitions for Arm's Memory Tagging Extension (MTE) and Branch
// Target Identification (BTI).
#if defined(ARCH_CPU_ARM64)
#define PROT_MTE 0x20
#define PROT_BTI 0x10
#endif

namespace {

inline bool IsArchitectureX86_64() {
#if defined(__x86_64__)
  return true;
#else
  return false;
#endif
}

inline bool IsArchitectureI386() {
#if defined(__i386__)
  return true;
#else
  return false;
#endif
}

inline bool IsAndroid() {
#if BUILDFLAG(IS_ANDROID)
  return true;
#else
  return false;
#endif
}

inline bool IsArchitectureMips() {
#if defined(__mips__)
  return true;
#else
  return false;
#endif
}

// Ubuntu's version of glibc has a race condition in sem_post that can cause
// it to call futex(2) with bogus op arguments. To workaround this, we need
// to allow those futex(2) calls to fail with EINVAL, instead of crashing the
// process. See crbug.com/598471.
inline bool IsBuggyGlibcSemPost() {
#if defined(LIBC_GLIBC) && !BUILDFLAG(IS_CHROMEOS_ASH)
  return true;
#else
  return false;
#endif
}

}  // namespace.

using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::Arg;
using sandbox::bpf_dsl::BoolExpr;
using sandbox::bpf_dsl::Error;
using sandbox::bpf_dsl::If;
using sandbox::bpf_dsl::ResultExpr;

namespace sandbox {

// Allow Glibc's and Android pthread creation flags, crash on any other
// thread creation attempts and EPERM attempts to use neither
// CLONE_VM nor CLONE_THREAD (all fork implementations), unless CLONE_VFORK is
// present (as in newer versions of posix_spawn).
ResultExpr RestrictCloneToThreadsAndEPERMFork() {
  const Arg<unsigned long> flags(0);

  // TODO(mdempsky): Extend DSL to support (flags & ~mask1) == mask2.
  const uint64_t kAndroidCloneMask = CLONE_VM | CLONE_FS | CLONE_FILES |
                                     CLONE_SIGHAND | CLONE_THREAD |
                                     CLONE_SYSVSEM;
  const uint64_t kObsoleteAndroidCloneMask = kAndroidCloneMask | CLONE_DETACHED;

  const uint64_t kGlibcPthreadFlags =
      CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD |
      CLONE_SYSVSEM | CLONE_SETTLS | CLONE_PARENT_SETTID | CLONE_CHILD_CLEARTID;
  const BoolExpr glibc_test = flags == kGlibcPthreadFlags;

  const BoolExpr android_test =
      AnyOf(flags == kAndroidCloneMask, flags == kObsoleteAndroidCloneMask,
            flags == kGlibcPthreadFlags);

  // The following two flags are the two important flags in any vfork-emulating
  // clone call. EPERM any clone call that contains both of them.
  const uint64_t kImportantCloneVforkFlags = CLONE_VFORK | CLONE_VM;

  const BoolExpr is_fork_or_clone_vfork =
      AnyOf((flags & (CLONE_VM | CLONE_THREAD)) == 0,
            (flags & kImportantCloneVforkFlags) == kImportantCloneVforkFlags);

  return If(IsAndroid() ? android_test : glibc_test, Allow())
      .ElseIf(is_fork_or_clone_vfork, Error(EPERM))
      .Else(CrashSIGSYSClone());
}

#ifndef PR_PAC_RESET_KEYS
#define PR_PAC_RESET_KEYS 54
#endif

ResultExpr RestrictPrctl() {
  // Will need to add seccomp compositing in the future. PR_SET_PTRACER is
  // used by breakpad but not needed anymore.
  const Arg<int> option(0), arg(1);
  return Switch(option)
      .Cases({PR_GET_NAME, PR_SET_NAME, PR_GET_DUMPABLE, PR_SET_DUMPABLE
#if BUILDFLAG(IS_ANDROID)
              , PR_SET_PTRACER, PR_SET_TIMERSLACK
              , PR_GET_NO_NEW_PRIVS
#if defined(ARCH_CPU_ARM64)
                ,
                PR_PAC_RESET_KEYS
                // PR_GET_TAGGED_ADDR_CTRL is used by debuggerd to report
                // whether memory tagging is active.
                ,
                PR_GET_TAGGED_ADDR_CTRL
                // PR_PAC_GET_ENABLED_KEYS is used by debuggerd to report
                // whether pointer authentication is enabled and which keys (A
                // or B) are active.
                ,
                PR_PAC_GET_ENABLED_KEYS
#endif

// Enable PR_SET_TIMERSLACK_PID, an Android custom prctl which is used in:
// https://android.googlesource.com/platform/system/core/+/lollipop-release/libcutils/sched_policy.c.
// Depending on the Android kernel version, this prctl may have different
// values. Since we don't know the correct value for the running kernel, we must
// allow them all.
//
// The effect is:
// On 3.14 kernels, this allows PR_SET_TIMERSLACK_PID and 43 and 127 (invalid
// prctls which will return EINVAL)
// On 3.18 kernels, this allows PR_SET_TIMERSLACK_PID, PR_SET_THP_DISABLE, and
// 127 (invalid).
// On 4.1 kernels and up, this allows PR_SET_TIMERSLACK_PID, PR_SET_THP_DISABLE,
// and PR_MPX_ENABLE_MANAGEMENT.

// https://android.googlesource.com/kernel/common/+/android-3.14/include/uapi/linux/prctl.h
#define PR_SET_TIMERSLACK_PID_1 41

// https://android.googlesource.com/kernel/common/+/android-3.18/include/uapi/linux/prctl.h
#define PR_SET_TIMERSLACK_PID_2 43

// https://android.googlesource.com/kernel/common/+/android-4.1/include/uapi/linux/prctl.h and up
#define PR_SET_TIMERSLACK_PID_3 127

              , PR_SET_TIMERSLACK_PID_1
              , PR_SET_TIMERSLACK_PID_2
              , PR_SET_TIMERSLACK_PID_3
#endif  // BUILDFLAG(IS_ANDROID)
              },
             Allow())
      .Cases({PR_SET_VMA},
             If(arg == PR_SET_VMA_ANON_NAME, Allow()).Else(CrashSIGSYSPrctl()))
      .Default(
          If(option == PR_SET_PTRACER, Error(EPERM)).Else(CrashSIGSYSPrctl()));
}

ResultExpr RestrictIoctl() {
  const Arg<int> request(1);
  return Switch(request).Cases({TCGETS, FIONREAD}, Allow()).Default(
      CrashSIGSYSIoctl());
}

ResultExpr RestrictMmapFlags() {
#if BUILDFLAG(IS_ANDROID) && defined(__x86_64__)
  const uint64_t kArchSpecificAllowedMask = MAP_32BIT;
#else
  const uint64_t kArchSpecificAllowedMask = 0;
#endif
  // The flags MAP_HUGETLB and MAP_POPULATE are specifically not permitted.
  // TODO(davidung), remove MAP_DENYWRITE with updated Tegra libraries.
  const uint64_t kAllowedMask = MAP_SHARED | MAP_PRIVATE | MAP_ANONYMOUS |
                                MAP_STACK | MAP_NORESERVE | MAP_FIXED |
                                MAP_DENYWRITE | MAP_LOCKED |
                                kArchSpecificAllowedMask;
  const Arg<int> flags(3);
  return If((flags & ~kAllowedMask) == 0, Allow()).Else(CrashSIGSYS());
}

ResultExpr RestrictMprotectFlags() {
  // The flags you see are actually the allowed ones, and the variable is a
  // "denied" mask because of the negation operator.
  // Significantly, we don't permit weird undocumented flags such as
  // PROT_GROWSDOWN.
#if defined(ARCH_CPU_ARM64)
  // Allows PROT_MTE and PROT_BTI (as explained higher up) on only Arm
  // platforms.
  const uint64_t kArchSpecificFlags = PROT_MTE | PROT_BTI;
#else
  const uint64_t kArchSpecificFlags = 0;
#endif
  const uint64_t kAllowedMask =
      PROT_READ | PROT_WRITE | PROT_EXEC | kArchSpecificFlags;
  const Arg<int> prot(2);
  return If((prot & ~kAllowedMask) == 0, Allow()).Else(CrashSIGSYS());
}

ResultExpr RestrictFcntlCommands() {
  // We also restrict the flags in F_SETFL. We don't want to permit flags with
  // a history of trouble such as O_DIRECT. The flags you see are actually the
  // allowed ones, and the variable is a "denied" mask because of the negation
  // operator.
  // Glibc overrides the kernel's O_LARGEFILE value. Account for this.
  uint64_t kOLargeFileFlag = O_LARGEFILE;
  if (IsArchitectureX86_64() || IsArchitectureI386() || IsArchitectureMips())
    kOLargeFileFlag = 0100000;

  const Arg<int> cmd(1);
  const Arg<long> long_arg(2);

  const uint64_t kAllowedMask = O_ACCMODE | O_APPEND | O_NONBLOCK | O_SYNC |
                                kOLargeFileFlag | O_CLOEXEC | O_NOATIME;
#if BUILDFLAG(IS_ANDROID)
  const uint64_t kOsSpecificSeals = F_SEAL_FUTURE_WRITE;
#else
  const uint64_t kOsSpecificSeals = 0;
#endif
  const uint64_t kAllowedSeals = F_SEAL_SEAL | F_SEAL_GROW | F_SEAL_SHRINK |
                                 kOsSpecificSeals;
  // clang-format off
  return Switch(cmd)
      .Cases({F_GETFL,
              F_GETFD,
              F_GET_SEALS,
              F_SETFD,
              F_SETLK,
              F_SETLKW,
              F_GETLK,
              F_DUPFD,
              F_DUPFD_CLOEXEC},
             Allow())
      .Case(F_SETFL,
            If((long_arg & ~kAllowedMask) == 0, Allow()).Else(CrashSIGSYS()))
      .Case(F_ADD_SEALS,
            If((long_arg & ~kAllowedSeals) == 0, Allow()).Else(CrashSIGSYS()))
      .Default(CrashSIGSYS());
  // clang-format on
}

#if defined(__i386__) || defined(__mips__)
ResultExpr RestrictSocketcallCommand() {
  // Unfortunately, we are unable to restrict the first parameter to
  // socketpair(2). Whilst initially sounding bad, it's noteworthy that very
  // few protocols actually support socketpair(2). The scary call that we're
  // worried about, socket(2), remains blocked.
  const Arg<int> call(0);
  return Switch(call)
      .Cases({SYS_SOCKETPAIR,
              SYS_SHUTDOWN,
              SYS_RECV,
              SYS_SEND,
              SYS_RECVFROM,
              SYS_SENDTO,
              SYS_RECVMSG,
              SYS_SENDMSG},
             Allow())
      .Default(Error(EPERM));
}
#endif

ResultExpr RestrictKillTarget(pid_t target_pid, int sysno) {
  switch (sysno) {
    case __NR_kill:
    case __NR_tgkill: {
      const Arg<pid_t> pid(0);
      return If(pid == target_pid, Allow()).Else(CrashSIGSYSKill());
    }
    case __NR_tkill:
      return CrashSIGSYSKill();
    default:
      NOTREACHED();
  }
}

ResultExpr RestrictFutex() {
  const uint64_t kAllowedFutexFlags = FUTEX_PRIVATE_FLAG | FUTEX_CLOCK_REALTIME;
  const Arg<int> op(1);
  return Switch(op & ~kAllowedFutexFlags)
      .Cases({FUTEX_WAIT, FUTEX_WAKE, FUTEX_REQUEUE, FUTEX_CMP_REQUEUE,
#if BUILDFLAG(ENABLE_MUTEX_PRIORITY_INHERITANCE)
              // Enable priority-inheritance operations.
              FUTEX_LOCK_PI, FUTEX_UNLOCK_PI, FUTEX_TRYLOCK_PI,
              FUTEX_WAIT_REQUEUE_PI, FUTEX_CMP_REQUEUE_PI,
#endif  // BUILDFLAG(ENABLE_MUTEX_PRIORITY_INHERITANCE)
              FUTEX_WAKE_OP, FUTEX_WAIT_BITSET, FUTEX_WAKE_BITSET},
             Allow())
      .Default(IsBuggyGlibcSemPost() ? Error(EINVAL) : CrashSIGSYSFutex());
}

ResultExpr RestrictGetSetpriority(pid_t target_pid) {
  const Arg<int> which(0);
  const Arg<int> who(1);
  return If(which == PRIO_PROCESS,
            Switch(who).Cases({0, target_pid}, Allow()).Default(Error(EPERM)))
      .Else(CrashSIGSYS());
}

ResultExpr RestrictSchedTarget(pid_t target_pid, int sysno) {
  switch (sysno) {
    case __NR_sched_getaffinity:
    case __NR_sched_getattr:
    case __NR_sched_getparam:
    case __NR_sched_getscheduler:
    case __NR_sched_rr_get_interval:
#if defined(__i386__) || defined(__arm__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
    case __NR_sched_rr_get_interval_time64:
#endif
    case __NR_sched_setaffinity:
    case __NR_sched_setattr:
    case __NR_sched_setparam:
    case __NR_sched_setscheduler: {
      const Arg<pid_t> pid(0);
      return Switch(pid)
          .Cases({0, target_pid}, Allow())
          .Default(RewriteSchedSIGSYS());
    }
    default:
      NOTREACHED();
  }
}

ResultExpr RestrictPrlimit64(pid_t target_pid) {
  const Arg<pid_t> pid(0);
  return Switch(pid).Cases({0, target_pid}, Allow()).Default(CrashSIGSYS());
}

ResultExpr RestrictGetrusage() {
  const Arg<int> who(0);
  return If(AnyOf(who == RUSAGE_SELF, who == RUSAGE_THREAD), Allow())
         .Else(CrashSIGSYS());
}

ResultExpr RestrictClockID() {
  static_assert(4 == sizeof(clockid_t), "clockid_t is not 32bit");
  const Arg<clockid_t> clockid(0);

  // Clock IDs < 0 are per pid/tid or are clockfds.
  const unsigned int kIsPidBit = 1u<<31;

  return
    If((clockid & kIsPidBit) == 0,
      Switch(clockid).Cases({
              CLOCK_BOOTTIME,
              CLOCK_MONOTONIC,
              CLOCK_MONOTONIC_COARSE,
              CLOCK_MONOTONIC_RAW,
              CLOCK_PROCESS_CPUTIME_ID,
              CLOCK_REALTIME,
              CLOCK_REALTIME_COARSE,
              CLOCK_THREAD_CPUTIME_ID},
             Allow())
      .Default(CrashSIGSYS()))
#if BUILDFLAG(IS_ANDROID)
    // Allow per-pid and per-tid clocks.
    .ElseIf((clockid & CPUCLOCK_CLOCK_MASK) != CLOCKFD, Allow())
#endif
    .Else(CrashSIGSYS());
}

#if !defined(GRND_NONBLOCK)
#define GRND_NONBLOCK 1
#endif

#if !defined(GRND_INSECURE)
#define GRND_INSECURE 4
#endif

ResultExpr RestrictGetRandom() {
  const Arg<unsigned int> flags(2);
  const unsigned int kGoodFlags = GRND_NONBLOCK | GRND_INSECURE;
  return If((flags & ~kGoodFlags) == 0, Allow()).Else(CrashSIGSYS());
}

ResultExpr RestrictPrlimit(pid_t target_pid) {
  const Arg<pid_t> pid(0);
  // Only allow operations for the current process.
  return If(AnyOf(pid == 0, pid == target_pid), Allow()).Else(Error(EPERM));
}

ResultExpr RestrictPrlimitToGetrlimit(pid_t target_pid) {
  const Arg<pid_t> pid(0);
  const Arg<uintptr_t> new_limit(2);
  // Only allow operations for the current process, and only with |new_limit|
  // set to null.
  return If(AllOf(new_limit == 0, AnyOf(pid == 0, pid == target_pid)), Allow())
      .Else(Error(EPERM));
}

ResultExpr RestrictPtrace() {
  const Arg<int> request(0);
#if defined(__aarch64__)
  const Arg<uintptr_t> addr(2);
#endif
  return Switch(request)
      .Cases({
#if !defined(__aarch64__)
                 PTRACE_GETREGS, PTRACE_GETFPREGS, PTRACE_GET_THREAD_AREA,
                 PTRACE_GETREGSET,
#endif
#if defined(__arm__)
                 PTRACE_GETVFPREGS,
#endif
                 PTRACE_PEEKDATA, PTRACE_ATTACH, PTRACE_DETACH},
             Allow())
#if defined(__aarch64__)
      .Case(
          PTRACE_GETREGSET,
          If(AllOf(addr != NT_ARM_PACA_KEYS, addr != NT_ARM_PACG_KEYS), Allow())
              .Else(CrashSIGSYSPtrace()))
#endif
      .Default(CrashSIGSYSPtrace());
}

ResultExpr RestrictPkeyAllocFlags() {
  const Arg<int> flags(0);
  return If(flags == 0, Allow()).Else(CrashSIGSYS());
}

ResultExpr RestrictGoogle3Threading(int sysno) {
  DCHECK(sysno == __NR_getitimer || sysno == __NR_setitimer);

  const Arg<int> which(0);
  return If(which == ITIMER_PROF, Allow()).Else(Error(EPERM));
}

ResultExpr RestrictPipe2() {
  const Arg<int> flags(1);
  return If((flags & ~(O_CLOEXEC|O_DIRECT|O_NONBLOCK)) == 0, Allow())
      .Else(CrashSIGSYS());
}

}  // namespace sandbox.
