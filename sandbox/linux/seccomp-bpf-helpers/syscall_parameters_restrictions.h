// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SECCOMP_BPF_HELPERS_SYSCALL_PARAMETERS_RESTRICTIONS_H_
#define SANDBOX_LINUX_SECCOMP_BPF_HELPERS_SYSCALL_PARAMETERS_RESTRICTIONS_H_

#include <unistd.h>

#include "build/build_config.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl_forward.h"
#include "sandbox/sandbox_export.h"

// These are helpers to build seccomp-bpf policies, i.e. policies for a
// sandbox that reduces the Linux kernel's attack surface. They return a
// bpf_dsl::ResultExpr suitable to restrict certain system call parameters.

namespace sandbox {

// Allow clone(2) for threads.
// Reject fork(2) attempts with EPERM.
// Don't restrict on ASAN.
// Crash if anything else is attempted.
SANDBOX_EXPORT bpf_dsl::ResultExpr RestrictCloneToThreadsAndEPERMFork();

// Allow PR_GET_NAME, PR_SET_NAME, PR_SET_DUMPABLE, PR_GET_DUMPABLE.
// On Android allows a few other options.
// Returns EPERM for PR_SET_PTRACER to allow crashpad to try to set itself as
// ptracer at crash time, if it hasn't yet been able to.
// Crash if anything else is attempted.
SANDBOX_EXPORT bpf_dsl::ResultExpr RestrictPrctl();

// Allow TCGETS and FIONREAD.
// Crash if anything else is attempted.
SANDBOX_EXPORT bpf_dsl::ResultExpr RestrictIoctl();

// Restrict the flags argument in mmap(2).
// Only allow: MAP_SHARED | MAP_PRIVATE | MAP_ANONYMOUS |
// MAP_STACK | MAP_NORESERVE | MAP_FIXED | MAP_DENYWRITE.
// Crash if any other flag is used.
SANDBOX_EXPORT bpf_dsl::ResultExpr RestrictMmapFlags();

// Restrict the prot argument in mprotect(2).
// Only allow: PROT_READ | PROT_WRITE | PROT_EXEC.
// PROT_BTI | PROT_MTE is additionally allowed on 64-bit Arm.
SANDBOX_EXPORT bpf_dsl::ResultExpr RestrictMprotectFlags();

// Restrict fcntl(2) cmd argument to:
// We allow F_GETFL, F_SETFL, F_GETFD, F_SETFD, F_DUPFD, F_DUPFD_CLOEXEC,
// F_SETLK, F_SETLKW and F_GETLK.
// Also, in F_SETFL, restrict the allowed flags to: O_ACCMODE | O_APPEND |
// O_NONBLOCK | O_SYNC | O_LARGEFILE | O_CLOEXEC | O_NOATIME.
SANDBOX_EXPORT bpf_dsl::ResultExpr RestrictFcntlCommands();

#if defined(__i386__) || defined(__mips__)
// Restrict socketcall(2) to only allow socketpair(2), send(2), recv(2),
// sendto(2), recvfrom(2), shutdown(2), sendmsg(2) and recvmsg(2).
SANDBOX_EXPORT bpf_dsl::ResultExpr RestrictSocketcallCommand();
#endif

// Restrict |sysno| (which must be kill, tkill or tgkill) by allowing tgkill or
// kill iff the first parameter is |target_pid|, crashing otherwise or if
// |sysno| is tkill.
SANDBOX_EXPORT bpf_dsl::ResultExpr RestrictKillTarget(pid_t target_pid,
                                                      int sysno);

// Crash if FUTEX_CMP_REQUEUE_PI is used in the second argument of futex(2).
SANDBOX_EXPORT bpf_dsl::ResultExpr RestrictFutex();

// Crash if |which| is not PRIO_PROCESS. EPERM if |who| is not 0, neither
// |target_pid| while calling setpriority(2) / getpriority(2).
SANDBOX_EXPORT bpf_dsl::ResultExpr RestrictGetSetpriority(pid_t target_pid);

// Restricts |pid| for sched_* syscalls which take a pid as the first argument.
// We only allow calling these syscalls if the pid argument is equal to the pid
// of the sandboxed process or 0 (indicating the current thread).  The following
// syscalls are supported:
//
// sched_getaffinity(), sched_getattr(), sched_getparam(), sched_getscheduler(),
// sched_rr_get_interval(), sched_setaffinity(), sched_setattr(),
// sched_setparam(), sched_setscheduler()
SANDBOX_EXPORT bpf_dsl::ResultExpr RestrictSchedTarget(pid_t target_pid,
                                                       int sysno);

// Restricts the |pid| argument of prlimit64 to 0 (meaning the calling process)
// or target_pid.
SANDBOX_EXPORT bpf_dsl::ResultExpr RestrictPrlimit64(pid_t target_pid);

// Restricts the |who| argument of getrusage to RUSAGE_SELF (meaning the calling
// process).
SANDBOX_EXPORT bpf_dsl::ResultExpr RestrictGetrusage();

// Restrict |clk_id| for clock_getres(), clock_gettime(), clock_settime(), and
// clock_nanosleep(). We allow accessing only CLOCK_BOOTTIME,
// CLOCK_MONOTONIC{,_RAW,_COARSE}, CLOCK_PROCESS_CPUTIME_ID,
// CLOCK_REALTIME{,_COARSE}, and CLOCK_THREAD_CPUTIME_ID.  In particular, on
// non-Android platforms this disallows access to arbitrary per-{process,thread}
// CPU-time clock IDs (such as those returned by {clock,pthread}_getcpuclockid),
// which can leak information about the state of the host OS.
SANDBOX_EXPORT bpf_dsl::ResultExpr RestrictClockID();

// Restrict the flags argument to getrandom() to allow only no flags, or
// GRND_NONBLOCK.
SANDBOX_EXPORT bpf_dsl::ResultExpr RestrictGetRandom();

// Restrict |pid| to the calling process (or 0) for prlimit64().  This allows
// getting and setting rlimits only on the current process.  Otherwise, fail
// gracefully; see crbug.com/160157.
SANDBOX_EXPORT bpf_dsl::ResultExpr RestrictPrlimit(pid_t target_pid);

// Restrict |pid| to the calling process (or 0) for prlimit64(), and require the
// |new_limit_ argument to be null.  This allows only getting limits on the
// current process. Otherwise fail gracefully.
SANDBOX_EXPORT bpf_dsl::ResultExpr RestrictPrlimitToGetrlimit(pid_t target_pid);

// Restrict ptrace() to just read operations that are needed for crash
// reporting. See https://crbug.com/933418 for details.
SANDBOX_EXPORT bpf_dsl::ResultExpr RestrictPtrace();

// Restrict the flags argument for pkey_alloc. It's specified to always be 0.
SANDBOX_EXPORT bpf_dsl::ResultExpr RestrictPkeyAllocFlags();

// Restrict the which argument to getitimer() and setitimer().
SANDBOX_EXPORT bpf_dsl::ResultExpr RestrictGoogle3Threading(int sysno);

// Restrict the flags of pipe2().
SANDBOX_EXPORT bpf_dsl::ResultExpr RestrictPipe2();

}  // namespace sandbox.

#endif  // SANDBOX_LINUX_SECCOMP_BPF_HELPERS_SYSCALL_PARAMETERS_RESTRICTIONS_H_
