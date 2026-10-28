// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/peer_connection_bpf_policy_linux.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/filter.h>
#include <linux/netlink.h>
#include <sched.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "base/check_op.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/bpf_dsl/linux_syscall_ranges.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_parameters_restrictions.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_sets.h"
#include "sandbox/linux/seccomp-bpf/syscall.h"
#include "sandbox/linux/system_headers/linux_seccomp.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"

namespace remoting {

namespace {

// The newest syscall known to this policy. Syscalls added to the kernel after
// it are denied with ENOSYS, so that new syscalls which act on other processes
// are not silently allowed by this blocklist. libc falls back gracefully when a
// syscall returns ENOSYS. Update this (and review the new syscalls) when
// //sandbox/linux/system_headers is updated.
//
// Note that syscalls above MAX_PUBLIC_SYSCALL (which comes from the sysroot on
// some architectures, e.g. arm64) are already denied with ENOSYS by
// sandbox::bpf_dsl::Policy::InvalidSyscall(), so the effective cutoff may be
// lower on those architectures.
constexpr int kNewestKnownSyscall = __NR_mseal;

// From <linux/ioprio.h> (IOPRIO_WHO_PROCESS), which is not available in all
// sysroots.
constexpr int kIoprioWhoProcess = 1;

// The helpers below are used instead of sandbox::RestrictGetSetpriority() and
// sandbox::RestrictSchedTarget(), since those crash the process for some
// arguments (e.g. PRIO_USER, or targeting another thread), and
// RestrictGetSetpriority() does not allow a thread to target itself by its
// thread ID, which base does when setting thread priorities. Targeting another
// thread of this process fails with EPERM.
//
// Trap handler for syscalls whose argument at `kPidArgIndex` is a pid/tid, and
// which are allowed to target only the calling process. A thread may still pass
// its own thread ID, which is replaced with 0 (meaning "the calling thread").
// Any other target fails with EPERM. The index is a template parameter, since
// trap handlers must have the sandbox::bpf_dsl::TrapRegistry::TrapFnc
// signature.
//
// This runs in a SIGSYS handler, so it must be async-signal-safe.
template <size_t kPidArgIndex>
intptr_t ReplaceOwnThreadIdHandler(const arch_seccomp_data& args,
                                   void* /*aux*/) {
  static_assert(kPidArgIndex <
                std::extent_v<decltype(arch_seccomp_data::args)>);
  if (args.args[kPidArgIndex] !=
      static_cast<uint64_t>(sandbox::Syscall::Call(__NR_gettid))) {
    return -EPERM;
  }
  std::array<intptr_t, 6> new_args = {
      static_cast<intptr_t>(args.args[0]), static_cast<intptr_t>(args.args[1]),
      static_cast<intptr_t>(args.args[2]), static_cast<intptr_t>(args.args[3]),
      static_cast<intptr_t>(args.args[4]), static_cast<intptr_t>(args.args[5])};
  new_args[kPidArgIndex] = 0;
  return sandbox::Syscall::Call(args.nr, new_args[0], new_args[1], new_args[2],
                                new_args[3], new_args[4], new_args[5]);
}

// Allows the syscall if its argument at `kPidArgIndex` (the pid) is 0 or
// `policy_pid`, rewrites it if it is the calling thread's ID, and fails with
// EPERM otherwise.
template <size_t kPidArgIndex>
sandbox::bpf_dsl::ResultExpr RestrictPidArgToSelf(pid_t policy_pid) {
  using sandbox::bpf_dsl::Allow;
  using sandbox::bpf_dsl::Arg;
  using sandbox::bpf_dsl::Switch;
  using sandbox::bpf_dsl::Trap;

  const Arg<pid_t> pid(kPidArgIndex);
  return Switch(pid)
      .Cases({0, policy_pid}, Allow())
      .Default(Trap(&ReplaceOwnThreadIdHandler<kPidArgIndex>, /*aux=*/nullptr));
}

}  // namespace

PeerConnectionBpfPolicyLinux::PeerConnectionBpfPolicyLinux(pid_t policy_pid)
    : policy_pid_(policy_pid) {}

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

  // Make sure that the policy is built for the process it is applied to.
  DCHECK_EQ(getpid(), policy_pid_);

  // Syscalls newer than this policy. See kNewestKnownSyscall. Private and
  // ghost syscall ranges (e.g. ARM-private syscalls) are not affected.
  if (sysno > kNewestKnownSyscall &&
      sysno <= static_cast<int>(MAX_PUBLIC_SYSCALL)) {
    return Error(ENOSYS);
  }

  // Process debugging, tracing, and memory tampering (ptrace, process_vm_*).
  if (SyscallSets::IsDebug(sysno)) {
    return Error(EPERM);
  }

  // All peer connection processes run as the same user, so the kernel's
  // same-user permission checks do not separate them. kill() and tgkill() may
  // only target the calling process, otherwise the caller crashes. tkill() is
  // not allowed.
  if (SyscallSets::IsKill(sysno)) {
    return sandbox::RestrictKillTarget(policy_pid_, sysno);
  }

  // Kernel state that is shared by all processes of the same user and
  // outlives them: keyrings, System V IPC, and POSIX message queues.
  if (SyscallSets::IsKeyManagement(sysno) || SyscallSets::IsAnySystemV(sysno) ||
      SyscallSets::IsMessageQueue(sysno)) {
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
      // Note: open, openat, unlink, and unlinkat are not restricted here, since
      // Seccomp-BPF cannot inspect paths.
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
#if defined(__NR_fchmodat2)
    case __NR_fchmodat2:
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

    // Signals with an explicit target: only allow the calling process. This
    // matches how sandbox::BaselinePolicy restricts rt_tgsigqueueinfo, which is
    // used by Crashpad.
    case __NR_rt_sigqueueinfo:
    case __NR_rt_tgsigqueueinfo: {
      const Arg<pid_t> tgid(0);
      return If(tgid == policy_pid_, Allow()).Else(Error(EPERM));
    }

    // Syscalls that operate on another process identified by a pidfd or a pid.
    // pidfd_open() returns ENOSYS so that callers treat pidfds as unsupported.
    case __NR_pidfd_open:
      return Error(ENOSYS);
    case __NR_pidfd_getfd:
    case __NR_pidfd_send_signal:
    case __NR_process_madvise:
    case __NR_process_mrelease:
    case __NR_get_robust_list:
    case __NR_move_pages:
    case __NR_migrate_pages:
      return Error(EPERM);

    // Resource limits and scheduling: only allow targeting the calling process
    // or thread. Note that PRIO_PGRP and PRIO_USER (which would affect all
    // processes of the user) are denied.
    case __NR_prlimit64:
      return sandbox::RestrictPrlimit(policy_pid_);
    case __NR_getpriority:
    case __NR_setpriority: {
      const Arg<int> which(0);
      return If(which == PRIO_PROCESS,
                RestrictPidArgToSelf</*kPidArgIndex=*/1>(policy_pid_))
          .Else(Error(EPERM));
    }
    case __NR_ioprio_get:
    case __NR_ioprio_set: {
      const Arg<int> which(0);
      return If(which == kIoprioWhoProcess,
                RestrictPidArgToSelf</*kPidArgIndex=*/1>(policy_pid_))
          .Else(Error(EPERM));
    }
    case __NR_sched_getaffinity:
    case __NR_sched_getattr:
    case __NR_sched_getparam:
    case __NR_sched_getscheduler:
    case __NR_sched_rr_get_interval:
#if defined(__NR_sched_rr_get_interval_time64)
    case __NR_sched_rr_get_interval_time64:
#endif
    case __NR_sched_setaffinity:
    case __NR_sched_setattr:
    case __NR_sched_setparam:
    case __NR_sched_setscheduler:
      return RestrictPidArgToSelf</*kPidArgIndex=*/0>(policy_pid_);

    // Deny directing SIGIO/SIGURG to another process (F_SETOWN, F_SETOWN_EX,
    // FIOSETOWN, SIOCSPGRP) or changing the signal (F_SETSIG), since the
    // kernel only checks that the target runs as the same user. Also deny
    // TIOCSTI and TIOCSPGRP in case a terminal is ever inherited.
    // The kernel truncates these arguments to 32 bits, so mask them instead of
    // using 32-bit Args, which crash on unexpected upper bits.
    case __NR_fcntl:
#if defined(__NR_fcntl64)
    case __NR_fcntl64:
#endif
    {
      const Arg<unsigned long> cmd(1);
      const auto cmd32 = cmd & 0xFFFFFFFFul;
      return If(AnyOf(cmd32 == static_cast<unsigned long>(F_SETOWN),
                      cmd32 == static_cast<unsigned long>(F_SETOWN_EX),
                      cmd32 == static_cast<unsigned long>(F_SETSIG)),
                Error(EPERM))
          .Else(Allow());
    }
    case __NR_ioctl: {
      const Arg<unsigned long> request(1);
      const auto request32 = request & 0xFFFFFFFFul;
      return If(AnyOf(request32 == static_cast<unsigned long>(FIOSETOWN),
                      request32 == static_cast<unsigned long>(SIOCSPGRP),
                      request32 == static_cast<unsigned long>(TIOCSTI),
                      request32 == static_cast<unsigned long>(TIOCSPGRP)),
                Error(EPERM))
          .Else(Allow());
    }

    // Don't allow becoming dumpable again. Crashpad's in-process signal handler
    // tries to make the process dumpable while it handles a crash
    // (ScopedPrSetDumpable in crashpad_client_linux.cc), so that a Crashpad
    // handler without CAP_SYS_PTRACE can ptrace it. Blocking this does not
    // affect crash collection: ScopedPrSetDumpable ignores the failure, and the
    // CRD Crashpad handler is launched by the root daemon and keeps
    // CAP_SYS_PTRACE and CAP_DAC_READ_SEARCH (see
    // remoting/host/crash/crashpad_handler.cc), which let it read a
    // non-dumpable process. Allowing it would make a crashing peer connection
    // process accessible to other peer connection processes.
    //
    // Also don't allow changing the parent-death signal set by the launcher,
    // so that this process cannot outlive the daemon process.
    case __NR_prctl: {
      const Arg<unsigned long> option(0);
      const Arg<unsigned long> arg2(1);
      const auto option32 = option & 0xFFFFFFFFul;
      return If(AnyOf(AllOf(option32 ==
                                static_cast<unsigned long>(PR_SET_DUMPABLE),
                            arg2 != 0),
                      option32 == static_cast<unsigned long>(PR_SET_PDEATHSIG)),
                Error(EPERM))
          .Else(Allow());
    }

    // Kernel interfaces that are not needed and have a history of local
    // privilege escalation bugs.
    case __NR_io_uring_setup:
    case __NR_io_uring_enter:
    case __NR_io_uring_register:
      return Error(ENOSYS);
    case __NR_bpf:
    case __NR_userfaultfd:
    case __NR_perf_event_open:
    case __NR_fanotify_init:
      return Error(EPERM);

    // Default: allow all other syscalls.
    default:
      return Allow();
  }
}

}  // namespace remoting
