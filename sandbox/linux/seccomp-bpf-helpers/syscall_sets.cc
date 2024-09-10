// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/seccomp-bpf-helpers/syscall_sets.h"

#include "build/build_config.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"

namespace sandbox {

// The functions below cover all existing i386, x86_64, and ARM system calls;
// excluding syscalls made obsolete in ARM EABI.
// The implicitly defined sets form a partition of the sets of
// system calls.

bool SyscallSets::IsKill(int sysno) {
  switch (sysno) {
    case __NR_kill:
    case __NR_tgkill:
    case __NR_tkill:  // Deprecated.
      return true;
    default:
      return false;
  }
}

bool SyscallSets::IsAllowedGettime(int sysno) {
  switch (sysno) {
    case __NR_gettimeofday:
#if defined(__i386__) || defined(__x86_64__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
    case __NR_time:
#endif
      return true;
    case __NR_adjtimex:         // Privileged.
    case __NR_clock_gettime:    // Parameters filtered by RestrictClockID().
    case __NR_clock_settime:    // Privileged.
    case __NR_clock_adjtime:    // Privileged.
    case __NR_clock_getres:     // Allowed only on Android with parameters
                                // filtered by RestrictClockID().
    case __NR_clock_nanosleep:  // Parameters filtered by RestrictClockID().

      // time64 versions are available on 32-bit systems.
#if defined(__i386__) || defined(__arm__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
    case __NR_clock_gettime64:      // Parameters filtered by RestrictClockID().
    case __NR_clock_settime64:      // Privileged.
    case __NR_clock_adjtime64:      // Privileged.
    case __NR_clock_getres_time64:  // Allowed only on Android with parameters
                                    // filtered by RestrictClockID().
    case __NR_clock_nanosleep_time64:  // Parameters filtered by RestrictClockID().
#endif
#if defined(__i386__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
    case __NR_ftime:  // Obsolete.
#endif
    case __NR_settimeofday:  // Privileged.
#if defined(__i386__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
    case __NR_stime:
#endif
    default:
      return false;
  }
}

bool SyscallSets::IsSendfile(int sysno) {
  if (sysno == __NR_sendfile) {
    return true;
  }
#if defined(__NR_sendfile64)
  if (sysno == __NR_sendfile64) {
    return true;
  }
#endif
  return false;
}

bool SyscallSets::IsCurrentDirectory(int sysno) {
  switch (sysno) {
    case __NR_getcwd:
    case __NR_chdir:
    case __NR_fchdir:
      return true;
    default:
      return false;
  }
}

bool SyscallSets::IsUmask(int sysno) {
  switch (sysno) {
    case __NR_umask:
      return true;
    default:
      return false;
  }
}

// System calls that directly access the file system. They might acquire
// a new file descriptor or otherwise perform an operation directly
// via a path.
// Both EPERM and ENOENT are valid errno unless otherwise noted in comment.
bool SyscallSets::IsFileSystem(int sysno) {
  switch (sysno) {
#if !defined(__aarch64__)
    case __NR_access:  // EPERM not a valid errno.
    case __NR_chmod:
    case __NR_chown:
#if defined(__i386__) || defined(__arm__)
    case __NR_chown32:
#endif
    case __NR_creat:
    case __NR_futimesat:  // Should be called utimesat ?
    case __NR_lchown:
    case __NR_link:
    case __NR_lstat:  // EPERM not a valid errno.
    case __NR_mkdir:
    case __NR_mknod:
    case __NR_open:
    case __NR_readlink:  // EPERM not a valid errno.
    case __NR_rename:
    case __NR_rmdir:
    case __NR_stat:  // EPERM not a valid errno.
    case __NR_symlink:
    case __NR_unlink:
#if !(defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_64_BITS))
    case __NR_uselib:  // Neither EPERM, nor ENOENT are valid errno.
#endif
    case __NR_ustat:   // Same as above. Deprecated.
    case __NR_utimes:
#endif  // !defined(__aarch64__)

    case __NR_execve:
    case __NR_faccessat:  // EPERM not a valid errno.
    case __NR_faccessat2:
    case __NR_fchmodat:
    case __NR_fchownat:  // Should be called chownat ?
#if defined(__x86_64__) || defined(__aarch64__)
    case __NR_newfstatat:  // fstatat(). EPERM not a valid errno.
#elif defined(__i386__) || defined(__arm__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
    case __NR_fstatat64:
#endif
#if defined(__i386__) || defined(__arm__)
    case __NR_lchown32:
#endif
    case __NR_linkat:
    case __NR_lookup_dcookie:  // ENOENT not a valid errno.

#if defined(__i386__) || defined(__arm__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
    case __NR_lstat64:
#endif
    case __NR_memfd_create:
    case __NR_mkdirat:
    case __NR_mknodat:
#if defined(__i386__)
    case __NR_oldlstat:
    case __NR_oldstat:
#endif
    case __NR_openat:
    case __NR_readlinkat:
    case __NR_renameat:
    case __NR_renameat2:
#if defined(__i386__) || defined(__arm__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
    case __NR_stat64:
#endif
    case __NR_statfs:  // EPERM not a valid errno.
#if defined(__i386__) || defined(__arm__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
    case __NR_statfs64:
#endif
    case __NR_statx:  // EPERM not a valid errno.
    case __NR_symlinkat:
    case __NR_truncate:
#if defined(__i386__) || defined(__arm__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
    case __NR_truncate64:
#endif
    case __NR_unlinkat:
#if defined(__i386__) || defined(__x86_64__) || defined(__mips__)
    case __NR_utime:
#endif
    case __NR_utimensat:  // New.
#if defined(__i386__) || defined(__arm__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
    case __NR_utimensat_time64:
#endif
      return true;
    default:
      return false;
  }
}

bool SyscallSets::IsTruncate(int sysno) {
  switch (sysno) {
    case __NR_ftruncate:
    case __NR_truncate:
#if defined(__i386__) || defined(__arm__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
    case __NR_ftruncate64:
    case __NR_truncate64:
#endif
      return true;
    default:
      return false;
  }
}

bool SyscallSets::IsAllowedFileSystemAccessViaFd(int sysno) {
  switch (sysno) {
    case __NR_fstat:
    case __NR_ftruncate:
#if defined(__i386__) || defined(__arm__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
    case __NR_fstat64:
    case __NR_ftruncate64:
#endif
      return true;
// TODO(jln): these should be denied gracefully as well (moved below).
#if defined(__i386__) || defined(__x86_64__) || defined(__mips__)
    case __NR_fadvise64:  // EPERM not a valid errno.
#endif
#if defined(__i386__)
    case __NR_fadvise64_64:
#endif
#if defined(__arm__)
    case __NR_arm_fadvise64_64:
#endif
    case __NR_fdatasync:  // EPERM not a valid errno.
    case __NR_flock:      // EPERM not a valid errno.
    case __NR_fstatfs:    // Give information about the whole filesystem.
#if defined(__i386__) || defined(__arm__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
    case __NR_fstatfs64:
#endif
    case __NR_fsync:  // EPERM not a valid errno.
#if defined(__i386__)
    case __NR_oldfstat:
#endif
#if defined(__i386__) || defined(__x86_64__) || defined(__mips__) || \
    defined(__aarch64__)
    case __NR_sync_file_range:  // EPERM not a valid errno.
#elif defined(__arm__)
    case __NR_arm_sync_file_range:  // EPERM not a valid errno.
#endif
    default:
      return false;
  }
}

// EPERM is a good errno for any of these.
bool SyscallSets::IsDeniedFileSystemAccessViaFd(int sysno) {
  switch (sysno) {
    case __NR_fallocate:
    case __NR_fchmod:
    case __NR_fchown:
#if defined(__i386__) || defined(__arm__)
    case __NR_fchown32:
#endif
#if !defined(__aarch64__)
    case __NR_getdents:    // EPERM not a valid errno.
#endif
    case __NR_getdents64:  // EPERM not a valid errno.
#if defined(__i386__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
    case __NR_readdir:
#endif
      return true;
    default:
      return false;
  }
}

bool SyscallSets::IsGetSimpleId(int sysno) {
  switch (sysno) {
    case __NR_capget:
    case __NR_getegid:
    case __NR_geteuid:
    case __NR_getgid:
    case __NR_getgroups:
    case __NR_getpid:
    case __NR_getppid:
    case __NR_getresgid:
    case __NR_getsid:
    case __NR_gettid:
    case __NR_getuid:
    case __NR_getresuid:
#if defined(__i386__) || defined(__arm__)
    case __NR_getegid32:
    case __NR_geteuid32:
    case __NR_getgid32:
    case __NR_getgroups32:
    case __NR_getresgid32:
    case __NR_getresuid32:
    case __NR_getuid32:
#endif
      return true;
    default:
      return false;
  }
}

bool SyscallSets::IsProcessPrivilegeChange(int sysno) {
  switch (sysno) {
    case __NR_capset:
#if defined(__i386__) || defined(__x86_64__)
    case __NR_ioperm:  // Intel privilege.
    case __NR_iopl:    // Intel privilege.
#endif
    case __NR_setfsgid:
    case __NR_setfsuid:
    case __NR_setgid:
    case __NR_setgroups:
    case __NR_setregid:
    case __NR_setresgid:
    case __NR_setresuid:
    case __NR_setreuid:
    case __NR_setuid:
#if defined(__i386__) || defined(__arm__)
    case __NR_setfsgid32:
    case __NR_setfsuid32:
    case __NR_setgid32:
    case __NR_setgroups32:
    case __NR_setregid32:
    case __NR_setresgid32:
    case __NR_setresuid32:
    case __NR_setreuid32:
    case __NR_setuid32:
#endif
      return true;
    default:
      return false;
  }
}

bool SyscallSets::IsProcessGroupOrSession(int sysno) {
  switch (sysno) {
    case __NR_setpgid:
#if !defined(__aarch64__)
    case __NR_getpgrp:
#endif
    case __NR_setsid:
    case __NR_getpgid:
      return true;
    default:
      return false;
  }
}

bool SyscallSets::IsAllowedSignalHandling(int sysno) {
  switch (sysno) {
    case __NR_rt_sigaction:
    case __NR_rt_sigprocmask:
    case __NR_rt_sigreturn:
    case __NR_rt_sigtimedwait:
    // Used by Crashpad or Bionic to set up signal handler stacks. An alternate
    // signal handler stack allows the kernel to deliver signals to threads
    // whose stack pointers no longer point to their main stack, e.g. stack
    // overflow.
    case __NR_sigaltstack:
#if defined(__i386__) || defined(__arm__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
    case __NR_rt_sigtimedwait_time64:
    case __NR_sigaction:
    case __NR_sigprocmask:
    case __NR_sigreturn:
#endif
      return true;
    case __NR_rt_sigpending:
    case __NR_rt_sigqueueinfo:
    case __NR_rt_sigsuspend:
    case __NR_rt_tgsigqueueinfo:
#if !defined(__aarch64__)
    case __NR_signalfd:
#endif
    case __NR_signalfd4:
#if defined(__i386__) || defined(__arm__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
    case __NR_sigpending:
    case __NR_sigsuspend:
#endif
#if defined(__i386__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
    case __NR_signal:
    case __NR_sgetmask:  // Obsolete.
    case __NR_ssetmask:
#endif
    default:
      return false;
  }
}

bool SyscallSets::IsAllowedOperationOnFd(int sysno) {
  switch (sysno) {
    case __NR_close:
    case __NR_dup:
#if !defined(__aarch64__)
    case __NR_dup2:
#endif
    case __NR_dup3:
#if defined(__x86_64__) || defined(__arm__) || defined(__mips__) || \
    defined(__aarch64__)
    case __NR_shutdown:
#endif
      return true;
    case __NR_fcntl:
#if defined(__i386__) || defined(__arm__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
    case __NR_fcntl64:
#endif
    default:
      return false;
  }
}

bool SyscallSets::IsKernelInternalApi(int sysno) {
  switch (sysno) {
    case __NR_restart_syscall:
#if defined(__arm__)
    case __ARM_NR_cmpxchg:
#endif
      return true;
    default:
      return false;
  }
}

// This should be thought through in conjunction with IsFutex().
bool SyscallSets::IsAllowedProcessStartOrDeath(int sysno) {
  switch (sysno) {
    case __NR_exit:
    case __NR_exit_group:
    case __NR_wait4:
    case __NR_waitid:
#if defined(__i386__)
    case __NR_waitpid:
#endif
      return true;
    case __NR_clone:  // Should be parameter-restricted.
    case __NR_setns:  // Privileged.
#if !defined(__aarch64__)
    case __NR_fork:
#endif
#if defined(__i386__) || defined(__x86_64__)
    case __NR_get_thread_area:
#endif
#if defined(__i386__) || defined(__x86_64__) || defined(__mips__)
    case __NR_set_thread_area:
#endif
    case __NR_set_tid_address:
    case __NR_unshare:
#if !defined(__mips__) && !defined(__aarch64__)
    case __NR_vfork:
#endif
    default:
      return false;
  }
}

// It's difficult to restrict those, but there is attack surface here.
bool SyscallSets::IsAllowedFutex(int sysno) {
  switch (sysno) {
    case __NR_get_robust_list:
    case __NR_set_robust_list:
    case __NR_futex:
#if defined(__i386__) || defined(__arm__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
    case __NR_futex_time64:
#endif
    default:
      return false;
  }
}

bool SyscallSets::IsAllowedEpoll(int sysno) {
  switch (sysno) {
#if !defined(__aarch64__)
    case __NR_epoll_create:
    case __NR_epoll_wait:
#endif
    case __NR_epoll_pwait:
    case __NR_epoll_create1:
    case __NR_epoll_ctl:
      return true;
    default:
#if defined(__x86_64__)
    case __NR_epoll_ctl_old:
#endif
#if defined(__x86_64__)
    case __NR_epoll_wait_old:
#endif
      return false;
  }
}

bool SyscallSets::IsDeniedGetOrModifySocket(int sysno) {
  switch (sysno) {
#if defined(__x86_64__) || defined(__arm__) || defined(__mips__) || \
    defined(__aarch64__)
    case __NR_accept:
    case __NR_accept4:
    case __NR_bind:
    case __NR_connect:
    case __NR_socket:
    case __NR_listen:
      return true;
#endif
    default:
      return false;
  }
}

#if defined(__i386__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
// Big multiplexing system call for sockets.
bool SyscallSets::IsSocketCall(int sysno) {
  switch (sysno) {
#if !(defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_64_BITS))
    case __NR_socketcall:
      return true;
#endif
    default:
      return false;
  }
}
#endif

#if defined(__x86_64__) || defined(__arm__) || defined(__mips__)
bool SyscallSets::IsNetworkSocketInformation(int sysno) {
  switch (sysno) {
    case __NR_getpeername:
    case __NR_getsockname:
    case __NR_getsockopt:
    case __NR_setsockopt:
      return true;
    default:
      return false;
  }
}
#endif

bool SyscallSets::IsAllowedAddressSpaceAccess(int sysno) {
  switch (sysno) {
    case __NR_brk:
    case __NR_mlock:
    case __NR_munlock:
    case __NR_munmap:
    case __NR_mseal:
      return true;
    case __NR_madvise:
    case __NR_mincore:
    case __NR_mlockall:
#if defined(__i386__) || defined(__x86_64__) || defined(__mips__) || \
    defined(__aarch64__)
    case __NR_mmap:
#endif
#if defined(__i386__) || defined(__arm__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
    case __NR_mmap2:
#endif
#if defined(__i386__) || defined(__x86_64__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
    case __NR_modify_ldt:
#endif
    case __NR_mprotect:
    case __NR_mremap:
    case __NR_msync:
    case __NR_munlockall:
    case __NR_readahead:
    case __NR_remap_file_pages:
#if defined(__i386__)
    case __NR_vm86:
    case __NR_vm86old:
#endif
    default:
      return false;
  }
}

bool SyscallSets::IsAllowedGeneralIo(int sysno) {
  switch (sysno) {
    case __NR_lseek:
#if defined(__i386__) || defined(__arm__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
    case __NR__llseek:
#endif
#if !defined(__aarch64__)
    case __NR_poll:
#endif
    case __NR_ppoll:
#if defined(__i386__) || defined(__arm__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
    case __NR_ppoll_time64:
#endif
    case __NR_pselect6:
#if defined(__i386__) || defined(__arm__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
    case __NR_pselect6_time64:
#endif
    case __NR_read:
    case __NR_readv:
    case __NR_pread64:
#if defined(__arm__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
    case __NR_recv:
#endif
#if defined(__x86_64__) || defined(__arm__) || defined(__mips__) || \
    defined(__aarch64__)
    case __NR_recvfrom:  // Could specify source.
    case __NR_recvmsg:   // Could specify source.
#endif
#if defined(__i386__) || defined(__x86_64__)
    case __NR_select:
#endif
#if defined(__i386__) || defined(__arm__) || defined(__mips__)
    case __NR__newselect:
#endif
#if defined(__arm__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
    case __NR_send:
#endif
#if defined(__x86_64__) || defined(__arm__) || defined(__mips__) || \
    defined(__aarch64__)
    case __NR_sendmsg:  // Could specify destination.
    case __NR_sendto:   // Could specify destination.
#endif
    case __NR_write:
    case __NR_writev:
      return true;
    case __NR_ioctl:  // Can be very powerful.
    case __NR_preadv:
    case __NR_pwrite64:
    case __NR_pwritev:
    case __NR_recvmmsg:  // Could specify source.
#if defined(__i386__) || defined(__arm__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
    case __NR_recvmmsg_time64:  // Could specify source.
#endif
    case __NR_sendmmsg:  // Could specify destination.
    case __NR_splice:
    case __NR_tee:
    case __NR_vmsplice:
    default:
      return false;
  }
}

bool SyscallSets::IsPrctl(int sysno) {
  switch (sysno) {
#if defined(__x86_64__)
    case __NR_arch_prctl:
#endif
    case __NR_prctl:
      return true;
    default:
      return false;
  }
}

bool SyscallSets::IsSeccomp(int sysno) {
  switch (sysno) {
    case __NR_seccomp:
      return true;
    default:
      return false;
  }
}

bool SyscallSets::IsAllowedBasicScheduler(int sysno) {
  switch (sysno) {
    case __NR_sched_yield:
#if !defined(__aarch64__)
    case __NR_pause:
#endif
    case __NR_nanosleep:
      return true;
    case __NR_getpriority:
#if defined(__i386__) || defined(__arm__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
    case __NR_nice:
#endif
    case __NR_setpriority:
    default:
      return false;
  }
}

bool SyscallSets::IsAdminOperation(int sysno) {
  switch (sysno) {
#if defined(__i386__) || defined(__arm__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
    case __NR_bdflush:
#endif
    case __NR_kexec_load:
    case __NR_reboot:
    case __NR_setdomainname:
    case __NR_sethostname:
    case __NR_syslog:
      return true;
    default:
      return false;
  }
}

bool SyscallSets::IsKernelModule(int sysno) {
  switch (sysno) {
#if defined(__i386__) || defined(__x86_64__) || defined(__mips__)
    case __NR_create_module:
    case __NR_get_kernel_syms:  // Should ENOSYS.
    case __NR_query_module:
#endif
    case __NR_delete_module:
    case __NR_init_module:
    case __NR_finit_module:
      return true;
    default:
      return false;
  }
}

bool SyscallSets::IsGlobalFSViewChange(int sysno) {
  switch (sysno) {
    case __NR_pivot_root:
    case __NR_chroot:
    case __NR_sync:
      return true;
    default:
      return false;
  }
}

bool SyscallSets::IsFsControl(int sysno) {
  switch (sysno) {
    case __NR_mount:
    case __NR_nfsservctl:
    case __NR_quotactl:
    case __NR_swapoff:
    case __NR_swapon:
#if defined(__i386__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
    case __NR_umount:
#endif
    case __NR_umount2:
      return true;
    default:
      return false;
  }
}

bool SyscallSets::IsNuma(int sysno) {
  switch (sysno) {
    case __NR_get_mempolicy:
    case __NR_getcpu:
    case __NR_mbind:
#if defined(__i386__) || defined(__x86_64__) || defined(__mips__) || \
    defined(__aarch64__)
    case __NR_migrate_pages:
#endif
    case __NR_move_pages:
    case __NR_set_mempolicy:
      return true;
    default:
      return false;
  }
}

bool SyscallSets::IsMessageQueue(int sysno) {
  switch (sysno) {
    case __NR_mq_getsetattr:
    case __NR_mq_notify:
    case __NR_mq_open:
    case __NR_mq_timedreceive:
    case __NR_mq_timedsend:
    case __NR_mq_unlink:
      // time64 versions available on 32-bit systems.
#if defined(__i386__) || defined(__arm__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
    case __NR_mq_timedreceive_time64:
    case __NR_mq_timedsend_time64:
#endif
      return true;
    default:
      return false;
  }
}

bool SyscallSets::IsGlobalProcessEnvironment(int sysno) {
  switch (sysno) {
    case __NR_acct:  // Privileged.
#if defined(__i386__) || defined(__x86_64__) || defined(__mips__) || \
    defined(__aarch64__)
    case __NR_getrlimit:
#endif
#if defined(__i386__) || defined(__arm__)
    case __NR_ugetrlimit:
#endif
#if defined(__i386__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
    case __NR_ulimit:
#endif
    case __NR_getrusage:
    case __NR_personality:  // Can change its personality as well.
    case __NR_prlimit64:    // Like setrlimit / getrlimit.
    case __NR_setrlimit:
    case __NR_times:
      return true;
    default:
      return false;
  }
}

bool SyscallSets::IsDebug(int sysno) {
  switch (sysno) {
    case __NR_ptrace:
    case __NR_process_vm_readv:
    case __NR_process_vm_writev:
    case __NR_kcmp:
      return true;
    default:
      return false;
  }
}

bool SyscallSets::IsGlobalSystemStatus(int sysno) {
  switch (sysno) {
#if !defined(__aarch64__)
    case __NR__sysctl:
    case __NR_sysfs:
#endif
    case __NR_sysinfo:
    case __NR_uname:
#if defined(__i386__)
    case __NR_olduname:
    case __NR_oldolduname:
#endif
      return true;
    default:
      return false;
  }
}

bool SyscallSets::IsEventFd(int sysno) {
  switch (sysno) {
#if !defined(__aarch64__)
    case __NR_eventfd:
#endif
    case __NR_eventfd2:
      return true;
    default:
      return false;
  }
}

bool SyscallSets::IsDlopen(int sysno) {
  switch (sysno) {
    // Chrome OS needs fstatfs for supporting a local glibc patch
    // which hooks into dlopen(), LD_PRELOAD, and --preload.
    // https://chromium-review.googlesource.com/c/chromiumos/overlays/chromiumos-overlay/+/2910526
    case __NR_fstatfs:
#if defined(__i386__) || defined(__arm__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
    case __NR_fstatfs64:
#endif
      return true;
    default:
      return false;
  }
}

// Asynchronous I/O API.
bool SyscallSets::IsAsyncIo(int sysno) {
  switch (sysno) {
    case __NR_io_cancel:
    case __NR_io_destroy:
    case __NR_io_getevents:
    case __NR_io_setup:
    case __NR_io_submit:
      return true;
    default:
      return false;
  }
}

bool SyscallSets::IsKeyManagement(int sysno) {
  switch (sysno) {
    case __NR_add_key:
    case __NR_keyctl:
    case __NR_request_key:
      return true;
    default:
      return false;
  }
}

#if defined(__x86_64__) || defined(__arm__) || defined(__aarch64__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_64_BITS))
bool SyscallSets::IsSystemVSemaphores(int sysno) {
  switch (sysno) {
    case __NR_semctl:
    case __NR_semget:
    case __NR_semop:
    case __NR_semtimedop:
#if defined(__i386__) || defined(__arm__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
    case __NR_semtimedop_time64:
#endif
      return true;
    default:
      return false;
  }
}
#endif

#if defined(__i386__) || defined(__x86_64__) || defined(__arm__) || \
    defined(__aarch64__) ||                                         \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_64_BITS))
// These give a lot of ambient authority and bypass the setuid sandbox.
bool SyscallSets::IsSystemVSharedMemory(int sysno) {
  switch (sysno) {
    case __NR_shmat:
    case __NR_shmctl:
    case __NR_shmdt:
    case __NR_shmget:
      return true;
    default:
      return false;
  }
}
#endif

#if defined(__x86_64__) || defined(__arm__) || defined(__aarch64__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_64_BITS))
bool SyscallSets::IsSystemVMessageQueue(int sysno) {
  switch (sysno) {
    case __NR_msgctl:
    case __NR_msgget:
    case __NR_msgrcv:
    case __NR_msgsnd:
      return true;
    default:
      return false;
  }
}
#endif

#if defined(__i386__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
// Big system V multiplexing system call.
bool SyscallSets::IsSystemVIpc(int sysno) {
  switch (sysno) {
#if !(defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_64_BITS))
    case __NR_ipc:
      return true;
#endif
    default:
      return false;
  }
}
#endif

bool SyscallSets::IsAnySystemV(int sysno) {
#if defined(__x86_64__) || defined(__arm__) || defined(__aarch64__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_64_BITS))
  return IsSystemVMessageQueue(sysno) || IsSystemVSemaphores(sysno) ||
         IsSystemVSharedMemory(sysno);
#elif defined(__i386__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
  return IsSystemVIpc(sysno);
#endif
}

bool SyscallSets::IsAdvancedScheduler(int sysno) {
  switch (sysno) {
    case __NR_ioprio_get:  // IO scheduler.
    case __NR_ioprio_set:
    case __NR_sched_get_priority_max:
    case __NR_sched_get_priority_min:
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
    case __NR_sched_setscheduler:
      return true;
    default:
      return false;
  }
}

bool SyscallSets::IsInotify(int sysno) {
  switch (sysno) {
    case __NR_inotify_add_watch:
#if !defined(__aarch64__)
    case __NR_inotify_init:
#endif
    case __NR_inotify_init1:
    case __NR_inotify_rm_watch:
      return true;
    default:
      return false;
  }
}

bool SyscallSets::IsFaNotify(int sysno) {
  switch (sysno) {
    case __NR_fanotify_init:
    case __NR_fanotify_mark:
      return true;
    default:
      return false;
  }
}

bool SyscallSets::IsTimer(int sysno) {
  switch (sysno) {
    case __NR_getitimer:
#if defined(__i386__) || defined(__x86_64__) || defined(__mips__)
    case __NR_alarm:
#endif
    case __NR_setitimer:
      return true;
    default:
      return false;
  }
}

bool SyscallSets::IsAdvancedTimer(int sysno) {
  switch (sysno) {
    case __NR_timer_create:
    case __NR_timer_delete:
    case __NR_timer_getoverrun:
    case __NR_timer_gettime:
    case __NR_timer_settime:
    case __NR_timerfd_create:
    case __NR_timerfd_gettime:
    case __NR_timerfd_settime:
// time64 versions are available on 32-bit systems.
#if defined(__i386__) || defined(__arm__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
    case __NR_timer_gettime64:
    case __NR_timer_settime64:
    case __NR_timerfd_gettime64:
    case __NR_timerfd_settime64:
#endif
      return true;
    default:
      return false;
  }
}

bool SyscallSets::IsClockApi(int sysno) {
  switch (sysno) {
    case __NR_clock_gettime:
    case __NR_clock_nanosleep:
#if defined(__i386__) || defined(__arm__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
    case __NR_clock_gettime64:
    case __NR_clock_nanosleep_time64:
#endif
      return true;
    default:
      return false;
  }
}

bool SyscallSets::IsExtendedAttributes(int sysno) {
  switch (sysno) {
    case __NR_fgetxattr:
    case __NR_flistxattr:
    case __NR_fremovexattr:
    case __NR_fsetxattr:
    case __NR_getxattr:
    case __NR_lgetxattr:
    case __NR_listxattr:
    case __NR_llistxattr:
    case __NR_lremovexattr:
    case __NR_lsetxattr:
    case __NR_removexattr:
    case __NR_setxattr:
      return true;
    default:
      return false;
  }
}

// Various system calls that need to be researched.
// TODO(jln): classify this better.
bool SyscallSets::IsMisc(int sysno) {
  switch (sysno) {
    case __NR_name_to_handle_at:
    case __NR_open_by_handle_at:
    case __NR_perf_event_open:
    case __NR_syncfs:
    case __NR_vhangup:
// The system calls below are not implemented.
#if defined(__i386__) || defined(__x86_64__) || defined(__mips__)
    case __NR_afs_syscall:
#endif
#if defined(__i386__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
    case __NR_break:
#endif
#if defined(__i386__) || defined(__x86_64__) || defined(__mips__)
    case __NR_getpmsg:
#endif
#if defined(__i386__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
    case __NR_gtty:
    case __NR_idle:
    case __NR_lock:
    case __NR_mpx:
    case __NR_prof:
    case __NR_profil:
#endif
#if defined(__i386__) || defined(__x86_64__) || defined(__mips__)
    case __NR_putpmsg:
#endif
#if defined(__x86_64__)
    case __NR_security:
#endif
#if defined(__i386__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
    case __NR_stty:
#endif
#if defined(__x86_64__)
    case __NR_tuxcall:
#endif
#if !defined(__aarch64__)
    case __NR_vserver:
#endif
      return true;
    default:
      return false;
  }
}

#if defined(__arm__)
bool SyscallSets::IsArmPciConfig(int sysno) {
  switch (sysno) {
    case __NR_pciconfig_iobase:
    case __NR_pciconfig_read:
    case __NR_pciconfig_write:
      return true;
    default:
      return false;
  }
}

bool SyscallSets::IsArmPrivate(int sysno) {
  switch (sysno) {
    case __ARM_NR_breakpoint:
    case __ARM_NR_cacheflush:
    case __ARM_NR_set_tls:
    case __ARM_NR_usr26:
    case __ARM_NR_usr32:
      return true;
    default:
      return false;
  }
}
#endif  // defined(__arm__)

#if defined(__mips__)
bool SyscallSets::IsMipsPrivate(int sysno) {
  switch (sysno) {
    case __NR_cacheflush:
    case __NR_cachectl:
      return true;
    default:
      return false;
  }
}

bool SyscallSets::IsMipsMisc(int sysno) {
  switch (sysno) {
    case __NR_sysmips:
#if !(defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_64_BITS))
    case __NR_unused150:
#endif
      return true;
    default:
      return false;
  }
}
#endif  // defined(__mips__)

bool SyscallSets::IsGoogle3Threading(int sysno) {
  switch (sysno) {
    case __NR_getitimer:
    case __NR_setitimer:
      return true;
    default:
      return false;
  }
}
}  // namespace sandbox.
