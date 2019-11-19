// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/seccomp-bpf-helpers/baseline_policy.h"

#include <errno.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/clang_coverage_buildflags.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/seccomp-bpf-helpers/sigsys_handlers.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_parameters_restrictions.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_sets.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/services/syscall_wrappers.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"

#if !defined(SO_PEEK_OFF)
#define SO_PEEK_OFF 42
#endif

// Changing this implementation will have an effect on *all* policies.
// Currently this means: Renderer/Worker, GPU, Flash and NaCl.

using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::Arg;
using sandbox::bpf_dsl::Error;
using sandbox::bpf_dsl::If;
using sandbox::bpf_dsl::ResultExpr;

namespace sandbox {

namespace {

bool IsBaselinePolicyAllowed(int sysno) {
  return SyscallSets::IsAllowedAddressSpaceAccess(sysno) ||
         SyscallSets::IsAllowedBasicScheduler(sysno) ||
         SyscallSets::IsAllowedEpoll(sysno) ||
         SyscallSets::IsAllowedFileSystemAccessViaFd(sysno) ||
         SyscallSets::IsAllowedFutex(sysno) ||
         SyscallSets::IsAllowedGeneralIo(sysno) ||
         SyscallSets::IsAllowedGetOrModifySocket(sysno) ||
         SyscallSets::IsAllowedGettime(sysno) ||
         SyscallSets::IsAllowedProcessStartOrDeath(sysno) ||
         SyscallSets::IsAllowedSignalHandling(sysno) ||
         SyscallSets::IsGetSimpleId(sysno) ||
         SyscallSets::IsKernelInternalApi(sysno) ||
#if defined(__arm__)
         SyscallSets::IsArmPrivate(sysno) ||
#endif
#if defined(__mips__)
         SyscallSets::IsMipsPrivate(sysno) ||
#endif
         SyscallSets::IsAllowedOperationOnFd(sysno);
}

// System calls that will trigger the crashing SIGSYS handler.
bool IsBaselinePolicyWatched(int sysno) {
  return SyscallSets::IsAdminOperation(sysno) ||
         SyscallSets::IsAdvancedScheduler(sysno) ||
         SyscallSets::IsAdvancedTimer(sysno) ||
         SyscallSets::IsAsyncIo(sysno) ||
         SyscallSets::IsDebug(sysno) ||
         SyscallSets::IsEventFd(sysno) ||
         SyscallSets::IsExtendedAttributes(sysno) ||
         SyscallSets::IsFaNotify(sysno) ||
         SyscallSets::IsFsControl(sysno) ||
         SyscallSets::IsGlobalFSViewChange(sysno) ||
         SyscallSets::IsGlobalProcessEnvironment(sysno) ||
         SyscallSets::IsGlobalSystemStatus(sysno) ||
         SyscallSets::IsInotify(sysno) ||
         SyscallSets::IsKernelModule(sysno) ||
         SyscallSets::IsKeyManagement(sysno) ||
         SyscallSets::IsKill(sysno) ||
         SyscallSets::IsMessageQueue(sysno) ||
         SyscallSets::IsMisc(sysno) ||
#if defined(__x86_64__)
         SyscallSets::IsNetworkSocketInformation(sysno) ||
#endif
         SyscallSets::IsNuma(sysno) ||
         SyscallSets::IsPrctl(sysno) ||
         SyscallSets::IsProcessGroupOrSession(sysno) ||
#if defined(__i386__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
         SyscallSets::IsSocketCall(sysno) ||
#endif
#if defined(__arm__)
         SyscallSets::IsArmPciConfig(sysno) ||
#endif
#if defined(__mips__)
         SyscallSets::IsMipsMisc(sysno) ||
#endif
         SyscallSets::IsTimer(sysno);
}

// |fs_denied_errno| is the errno return for denied filesystem access.
ResultExpr EvaluateSyscallImpl(int fs_denied_errno,
                               pid_t current_pid,
                               int sysno) {
#if defined(ADDRESS_SANITIZER) || defined(THREAD_SANITIZER) || \
    defined(MEMORY_SANITIZER)
  // TCGETS is required by the sanitizers on failure.
  if (sysno == __NR_ioctl) {
    return RestrictIoctl();
  }

  if (sysno == __NR_sched_getaffinity) {
    return Allow();
  }

  // Used when RSS limiting is enabled in sanitizers.
  if (sysno == __NR_getrusage) {
    return RestrictGetrusage();
  }

  if (sysno == __NR_sigaltstack) {
    // Required for better stack overflow detection in ASan. Disallowed in
    // non-ASan builds.
    return Allow();
  }
#endif  // defined(ADDRESS_SANITIZER) || defined(THREAD_SANITIZER) ||
        // defined(MEMORY_SANITIZER)

#if BUILDFLAG(CLANG_COVERAGE)
  if (SyscallSets::IsPrctl(sysno)) {
    return Allow();
  }

  if (sysno == __NR_ftruncate) {
    return Allow();
  }
#endif

  if (IsBaselinePolicyAllowed(sysno)) {
    return Allow();
  }

#if defined(OS_ANDROID)
  // Needed for thread creation.
  if (sysno == __NR_sigaltstack)
    return Allow();
#endif

  if (sysno == __NR_clock_gettime) {
    return RestrictClockID();
  }

  if (sysno == __NR_clone) {
    return RestrictCloneToThreadsAndEPERMFork();
  }

  if (sysno == __NR_fcntl)
    return RestrictFcntlCommands();

#if defined(__i386__) || defined(__arm__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
  if (sysno == __NR_fcntl64)
    return RestrictFcntlCommands();
#endif

#if !defined(__aarch64__)
  // fork() is never used as a system call (clone() is used instead), but we
  // have seen it in fallback code on Android.
  if (sysno == __NR_fork) {
    return Error(EPERM);
  }
#endif

#if defined(__NR_vfork)
  // vfork() is almost never used as a system call, but some libc versions (e.g.
  // older versions of bionic) might use it in a posix_spawn() implementation,
  // which is used by system();
  if (sysno == __NR_vfork) {
    return Error(EPERM);
  }
#endif

  if (sysno == __NR_futex)
    return RestrictFutex();

  if (sysno == __NR_set_robust_list)
    return Error(EPERM);

  if (sysno == __NR_getpriority || sysno ==__NR_setpriority)
    return RestrictGetSetpriority(current_pid);

  if (sysno == __NR_getrandom) {
    return RestrictGetRandom();
  }

  if (sysno == __NR_madvise) {
    // Only allow MADV_DONTNEED, MADV_RANDOM, MADV_NORMAL and MADV_FREE.
    const Arg<int> advice(2);
    return If(AnyOf(advice == MADV_DONTNEED,
                    advice == MADV_RANDOM,
                    advice == MADV_NORMAL
#if defined(MADV_FREE)
                    // MADV_FREE was introduced in Linux 4.5 and started being
                    // defined in glibc 2.24.
                    , advice == MADV_FREE
#endif
                    ), Allow()).Else(Error(EPERM));
  }

#if defined(__i386__) || defined(__x86_64__) || defined(__mips__) || \
    defined(__aarch64__)
  if (sysno == __NR_mmap)
    return RestrictMmapFlags();
#endif

#if defined(__i386__) || defined(__arm__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
  if (sysno == __NR_mmap2)
    return RestrictMmapFlags();
#endif

  if (sysno == __NR_mprotect)
    return RestrictMprotectFlags();

  if (sysno == __NR_prctl)
    return RestrictPrctl();

#if defined(__x86_64__) || defined(__arm__) || defined(__mips__) || \
    defined(__aarch64__)
  if (sysno == __NR_socketpair) {
    // Only allow AF_UNIX, PF_UNIX. Crash if anything else is seen.
    static_assert(AF_UNIX == PF_UNIX,
                  "af_unix and pf_unix should not be different");
    const Arg<int> domain(0);
    return If(domain == AF_UNIX, Allow()).Else(CrashSIGSYS());
  }
#endif

  // On Android, for https://crbug.com/701137.
  // On Desktop, for https://crbug.com/741984.
  if (sysno == __NR_mincore) {
    return Allow();
  }

  if (SyscallSets::IsKill(sysno)) {
    return RestrictKillTarget(current_pid, sysno);
  }

  if (SyscallSets::IsFileSystem(sysno) ||
      SyscallSets::IsCurrentDirectory(sysno)) {
    return Error(fs_denied_errno);
  }

  if (SyscallSets::IsSeccomp(sysno))
    return Error(EPERM);

  if (SyscallSets::IsAnySystemV(sysno)) {
    return Error(EPERM);
  }

  if (SyscallSets::IsUmask(sysno) ||
      SyscallSets::IsDeniedFileSystemAccessViaFd(sysno) ||
      SyscallSets::IsDeniedGetOrModifySocket(sysno) ||
      SyscallSets::IsProcessPrivilegeChange(sysno)) {
    return Error(EPERM);
  }

#if defined(__i386__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
  if (SyscallSets::IsSocketCall(sysno))
    return RestrictSocketcallCommand();
#endif

#if !defined(__i386__)
  if (sysno == __NR_getsockopt || sysno ==__NR_setsockopt) {
    // Used by Mojo EDK to catch a message pipe being sent over itself.
    const Arg<int> level(1);
    const Arg<int> optname(2);
    return If(AllOf(level == SOL_SOCKET, optname == SO_PEEK_OFF), Allow())
        .Else(CrashSIGSYS());
  }
#endif

  if (IsBaselinePolicyWatched(sysno)) {
    // Previously unseen syscalls. TODO(jln): some of these should
    // be denied gracefully right away.
    return CrashSIGSYS();
  }

  // In any other case crash the program with our SIGSYS handler.
  return CrashSIGSYS();
}

}  // namespace.

BaselinePolicy::BaselinePolicy() : BaselinePolicy(EPERM) {
  // Allocate crash keys set by Seccomp signal handlers.
  AllocateCrashKeys();
}

BaselinePolicy::BaselinePolicy(int fs_denied_errno)
    : fs_denied_errno_(fs_denied_errno), policy_pid_(sys_getpid()) {
}

BaselinePolicy::~BaselinePolicy() {
  // Make sure that this policy is created, used and destroyed by a single
  // process.
  DCHECK_EQ(sys_getpid(), policy_pid_);
}

ResultExpr BaselinePolicy::EvaluateSyscall(int sysno) const {
  // Sanity check that we're only called with valid syscall numbers.
  DCHECK(SandboxBPF::IsValidSyscallNumber(sysno));
  // Make sure that this policy is used in the creating process.
  if (1 == sysno) {
    DCHECK_EQ(sys_getpid(), policy_pid_);
  }
  return EvaluateSyscallImpl(fs_denied_errno_, policy_pid_, sysno);
}

ResultExpr BaselinePolicy::InvalidSyscall() const {
  return CrashSIGSYS();
}

}  // namespace sandbox.
