// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SECCOMP_BPF_HELPERS_SYSCALL_SETS_H_
#define SANDBOX_LINUX_SECCOMP_BPF_HELPERS_SYSCALL_SETS_H_

#include "build/build_config.h"
#include "sandbox/sandbox_export.h"

// These are helpers to build seccomp-bpf policies, i.e. policies for a
// sandbox that reduces the Linux kernel's attack surface. Given their
// nature, they don't have any clear semantics and are completely
// "implementation-defined".

namespace sandbox {

class SANDBOX_EXPORT SyscallSets {
 public:
  SyscallSets() = delete;
  SyscallSets(const SyscallSets&) = delete;
  SyscallSets& operator=(const SyscallSets&) = delete;

  static bool IsKill(int sysno);
  static bool IsAllowedGettime(int sysno);
  static bool IsCurrentDirectory(int sysno);
  static bool IsUmask(int sysno);
  // System calls that directly access the file system. They might acquire
  // a new file descriptor or otherwise perform an operation directly
  // via a path.
  static bool IsFileSystem(int sysno);
  static bool IsTruncate(int sysno);
  static bool IsAllowedFileSystemAccessViaFd(int sysno);
  static bool IsDeniedFileSystemAccessViaFd(int sysno);
  static bool IsGetSimpleId(int sysno);
  static bool IsProcessPrivilegeChange(int sysno);
  static bool IsProcessGroupOrSession(int sysno);
  static bool IsAllowedSignalHandling(int sysno);
  static bool IsAllowedOperationOnFd(int sysno);
  static bool IsKernelInternalApi(int sysno);
  // This should be thought through in conjunction with IsFutex().
  static bool IsAllowedProcessStartOrDeath(int sysno);
  // It's difficult to restrict those, but there is attack surface here.
  static bool IsAllowedFutex(int sysno);
  static bool IsAllowedEpoll(int sysno);
  static bool IsDeniedGetOrModifySocket(int sysno);

#if defined(__i386__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
  // Big multiplexing system call for sockets.
  static bool IsSocketCall(int sysno);
#endif

#if defined(__x86_64__) || defined(__arm__) || defined(__mips__) || \
    defined(__aarch64__)
  static bool IsNetworkSocketInformation(int sysno);
#endif

  static bool IsAllowedAddressSpaceAccess(int sysno);
  static bool IsAllowedGeneralIo(int sysno);
  static bool IsPrctl(int sysno);
  static bool IsSeccomp(int sysno);
  static bool IsAllowedBasicScheduler(int sysno);
  static bool IsAdminOperation(int sysno);
  static bool IsKernelModule(int sysno);
  static bool IsGlobalFSViewChange(int sysno);
  static bool IsFsControl(int sysno);
  static bool IsSendfile(int sysno);
  static bool IsNuma(int sysno);
  static bool IsMessageQueue(int sysno);
  static bool IsGlobalProcessEnvironment(int sysno);
  static bool IsDebug(int sysno);
  static bool IsGlobalSystemStatus(int sysno);
  static bool IsEventFd(int sysno);
  // System calls used for dlopen(), which loads shared libraries. May overlap
  // with other syscall sets.
  static bool IsDlopen(int sysno);
  // Asynchronous I/O API.
  static bool IsAsyncIo(int sysno);
  static bool IsKeyManagement(int sysno);
#if defined(__x86_64__) || defined(__arm__) || defined(__aarch64__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_64_BITS))
  static bool IsSystemVSemaphores(int sysno);
#endif
#if defined(__i386__) || defined(__x86_64__) || defined(__arm__) || \
    defined(__aarch64__) ||                                         \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_64_BITS))
  // These give a lot of ambient authority and bypass the setuid sandbox.
  static bool IsSystemVSharedMemory(int sysno);
#endif

#if defined(__x86_64__) || defined(__arm__) || defined(__aarch64__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_64_BITS))
  static bool IsSystemVMessageQueue(int sysno);
#endif

#if defined(__i386__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
  // Big system V multiplexing system call.
  static bool IsSystemVIpc(int sysno);
#endif

  static bool IsAnySystemV(int sysno);
  static bool IsAdvancedScheduler(int sysno);
  static bool IsInotify(int sysno);
  static bool IsFaNotify(int sysno);
  static bool IsTimer(int sysno);
  static bool IsAdvancedTimer(int sysno);
  static bool IsClockApi(int sysno);
  static bool IsExtendedAttributes(int sysno);
  static bool IsMisc(int sysno);
#if defined(__arm__)
  static bool IsArmPciConfig(int sysno);
  static bool IsArmPrivate(int sysno);
#endif  // defined(__arm__)
#if defined(__mips__)
  static bool IsMipsPrivate(int sysno);
  static bool IsMipsMisc(int sysno);
#endif  // defined(__mips__)
  static bool IsGoogle3Threading(int sysno);
};

}  // namespace sandbox.

#endif  // SANDBOX_LINUX_SECCOMP_BPF_HELPERS_SYSCALL_SETS_H_
