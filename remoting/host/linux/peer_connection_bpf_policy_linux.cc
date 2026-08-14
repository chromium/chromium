// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/peer_connection_bpf_policy_linux.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/filter.h>
#include <linux/netlink.h>
#include <sched.h>
#include <sys/socket.h>
#include <sys/syscall.h>

#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_parameters_restrictions.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_sets.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"

namespace remoting {

PeerConnectionBpfPolicyLinux::PeerConnectionBpfPolicyLinux() = default;

PeerConnectionBpfPolicyLinux::~PeerConnectionBpfPolicyLinux() = default;

sandbox::bpf_dsl::ResultExpr PeerConnectionBpfPolicyLinux::EvaluateSyscall(
    int sysno) const {
  using sandbox::SyscallSets;
  using sandbox::bpf_dsl::AllOf;
  using sandbox::bpf_dsl::Allow;
  using sandbox::bpf_dsl::AnyOf;
  using sandbox::bpf_dsl::Arg;
  using sandbox::bpf_dsl::Error;
  using sandbox::bpf_dsl::If;

  // Process debugging, tracing, and memory tampering (ptrace, process_vm_*).
  if (SyscallSets::IsDebug(sysno)) {
    return Error(EPERM);
  }

  // Filesystem control (mount, umount, swapon).
  if (SyscallSets::IsFsControl(sysno)) {
    return Error(EPERM);
  }

  // Kernel module loading and administrative operations (reboot, kexec_load).
  if (SyscallSets::IsKernelModule(sysno) ||
      SyscallSets::IsAdminOperation(sysno)) {
    return Error(EPERM);
  }

  // Thread creation vs process/namespace creation.
  if (sysno == __NR_clone) {
    return sandbox::RestrictCloneToThreadsAndEPERMFork();
  }

  switch (sysno) {
    // Process execution and spawning.
    case __NR_execve:
#if defined(__NR_execveat)
    case __NR_execveat:
#endif
#if defined(__NR_fork)
    case __NR_fork:
#endif
#if defined(__NR_vfork)
    case __NR_vfork:
#endif
      return Error(EPERM);

#if defined(__NR_clone3)
    case __NR_clone3:
      // clone3 takes a pointer argument which cannot be inspected via
      // Seccomp-BPF. Returning ENOSYS forces libc to fall back to clone.
      // See https://crbug.com/40768772.
      return Error(ENOSYS);
#endif

    // Namespace and root directory changes.
    case __NR_chroot:
#if defined(__NR_pivot_root)
    case __NR_pivot_root:
#endif
#if defined(__NR_unshare)
    case __NR_unshare:
#endif
#if defined(__NR_setns)
    case __NR_setns:
#endif
      return Error(EPERM);

      // Filesystem creation, deletion, and permission modification.
      // Note: open, openat, unlink, and unlinkat are allowed by default so
      // that POSIX shared memory allocations (/dev/shm via mkstemp/unlink for
      // Mojo DataPipes) succeed without a broker process.
#if defined(__NR_mkdir)
    case __NR_mkdir:
#endif
#if defined(__NR_mkdirat)
    case __NR_mkdirat:
#endif
#if defined(__NR_mknod)
    case __NR_mknod:
#endif
#if defined(__NR_mknodat)
    case __NR_mknodat:
#endif
#if defined(__NR_rmdir)
    case __NR_rmdir:
#endif
#if defined(__NR_rename)
    case __NR_rename:
#endif
#if defined(__NR_renameat)
    case __NR_renameat:
#endif
#if defined(__NR_renameat2)
    case __NR_renameat2:
#endif
#if defined(__NR_link)
    case __NR_link:
#endif
#if defined(__NR_linkat)
    case __NR_linkat:
#endif
#if defined(__NR_symlink)
    case __NR_symlink:
#endif
#if defined(__NR_symlinkat)
    case __NR_symlinkat:
#endif
#if defined(__NR_chmod)
    case __NR_chmod:
#endif
#if defined(__NR_fchmod)
    case __NR_fchmod:
#endif
#if defined(__NR_fchmodat)
    case __NR_fchmodat:
#endif
#if defined(__NR_chown)
    case __NR_chown:
#endif
#if defined(__NR_fchown)
    case __NR_fchown:
#endif
#if defined(__NR_fchownat)
    case __NR_fchownat:
#endif
#if defined(__NR_lchown)
    case __NR_lchown:
#endif
      return Error(EACCES);

    // Socket filtering: allow standard stream and datagram sockets (TCP, UDP,
    // UNIX) while denying AF_PACKET, SOCK_RAW, SOCK_PACKET, and non-route
    // netlink sockets. AF_NETLINK with NETLINK_ROUTE is allowed for
    // getifaddrs() and AddressTrackerLinux.
    case __NR_socket: {
      const Arg<int> domain(0);
      const Arg<int> type(1);
      const Arg<int> protocol(2);
      const int kAllowedTypeMask = ~(SOCK_NONBLOCK | SOCK_CLOEXEC);
      const auto base_type = type & kAllowedTypeMask;
      return If(domain == AF_PACKET, Error(EACCES))
          .ElseIf(AllOf(domain == AF_NETLINK, protocol == NETLINK_ROUTE),
                  Allow())
          .ElseIf(domain == AF_NETLINK, Error(EACCES))
          .ElseIf(AnyOf(base_type == SOCK_STREAM, base_type == SOCK_DGRAM),
                  Allow())
          .Else(Error(EACCES));
    }

    // Default: allow all other syscalls.
    default:
      return Allow();
  }
}

}  // namespace remoting
