// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/seccomp-bpf-helpers/baseline_policy_android.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/android/binder.h>
#include <linux/ashmem.h>
#include <linux/incrementalfs.h>
#include <linux/nbd.h>
#include <linux/net.h>
#include <linux/userfaultfd.h>
#include <sched.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/types.h>

#include "build/build_config.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/seccomp-bpf-helpers/sigsys_handlers.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_parameters_restrictions.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_sets.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"

#if defined(__x86_64__)
#include <asm/prctl.h>
#endif

using sandbox::bpf_dsl::AllOf;
using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::AnyOf;
using sandbox::bpf_dsl::Arg;
using sandbox::bpf_dsl::BoolConst;
using sandbox::bpf_dsl::BoolExpr;
using sandbox::bpf_dsl::Error;
using sandbox::bpf_dsl::If;
using sandbox::bpf_dsl::ResultExpr;

namespace sandbox {

#ifndef SOCK_CLOEXEC
#define SOCK_CLOEXEC O_CLOEXEC
#endif

#ifndef SOCK_NONBLOCK
#define SOCK_NONBLOCK O_NONBLOCK
#endif

namespace {

#if !defined(__i386__)
// Restricts the arguments to sys_socket() to AF_UNIX. Returns a BoolExpr that
// evaluates to true if the syscall should be allowed.
BoolExpr RestrictSocketArguments(const Arg<int>& domain,
                                 const Arg<int>& type,
                                 const Arg<int>& protocol) {
  const int kSockFlags = SOCK_CLOEXEC | SOCK_NONBLOCK;
  return AllOf(domain == AF_UNIX,
               AnyOf((type & ~kSockFlags) == SOCK_DGRAM,
                     (type & ~kSockFlags) == SOCK_STREAM),
               protocol == 0);
}
#endif  // !defined(__i386__)

ResultExpr RestrictAndroidIoctl(bool allow_userfaultfd_ioctls) {
  const Arg<unsigned int> request(1);

  // There is no way at runtime to test if the system is running with
  // BINDER_IPC_32BIT. Instead, compute the corresponding bitness' ioctl
  // request number, so that either are allowed in the case of mixed-bitness
  // systems.
#ifdef BINDER_IPC_32BIT
  const unsigned int kBinderWriteRead32 = BINDER_WRITE_READ;
  const unsigned int kBinderWriteRead64 =
      (BINDER_WRITE_READ & ~IOCSIZE_MASK) |
      ((sizeof(binder_write_read) * 2) << _IOC_SIZESHIFT);
#else
  const unsigned int kBinderWriteRead64 = BINDER_WRITE_READ;
  const unsigned int kBinderWriteRead32 =
      (BINDER_WRITE_READ & ~IOCSIZE_MASK) |
      ((sizeof(binder_write_read) / 2) << _IOC_SIZESHIFT);
#endif

  // ANDROID_ALARM_GET_TIME(ANDROID_ALARM_ELAPSED_REALTIME), a legacy interface
  // for getting clock information from /dev/alarm. It was removed in Android O
  // (https://android-review.googlesource.com/c/221812), and it can be safely
  // blocked in earlier releases because there is a fallback. Constant expanded
  // from
  // https://cs.android.com/android/platform/superproject/+/android-7.0.0_r1:external/kernel-headers/original/uapi/linux/android_alarm.h;l=57.
  // The size is a `struct timespec`, which has a different width on 32- and
  // 64-bit systems, so handle both.
  const unsigned int kAndroidAlarmGetTimeElapsedRealtime32 = 0x40086134;
  const unsigned int kAndroidAlarmGetTimeElapsedRealtime64 = 0x40106134;
  return Switch(request)
      .Cases(
          {// Android shared memory.
           ASHMEM_SET_NAME, ASHMEM_GET_NAME, ASHMEM_SET_SIZE, ASHMEM_GET_SIZE,
           ASHMEM_SET_PROT_MASK, ASHMEM_GET_PROT_MASK, ASHMEM_PIN, ASHMEM_UNPIN,
           ASHMEM_GET_PIN_STATUS,
           // Binder.
           kBinderWriteRead32, kBinderWriteRead64, BINDER_SET_MAX_THREADS,
           BINDER_THREAD_EXIT, BINDER_VERSION,
           BINDER_ENABLE_ONEWAY_SPAM_DETECTION, BINDER_GET_EXTENDED_ERROR,
           // incfs read ops.
           INCFS_IOC_READ_FILE_SIGNATURE, INCFS_IOC_GET_FILLED_BLOCKS,
           INCFS_IOC_GET_READ_TIMEOUTS, INCFS_IOC_GET_LAST_READ_ERROR,
           INCFS_IOC_GET_BLOCK_COUNT, INCFS_IOC_SET_READ_TIMEOUTS},
          Allow())
      .Cases(
          {// userfaultfd ART GC (https://crbug.com/1300653).
           UFFDIO_REGISTER, UFFDIO_UNREGISTER, UFFDIO_WAKE, UFFDIO_COPY,
           UFFDIO_ZEROPAGE, UFFDIO_CONTINUE},
          If(BoolConst(allow_userfaultfd_ioctls), Allow())
              .Else(RestrictIoctl()))
      .Cases(
          {// Deprecated Android /dev/alarm interface.
           kAndroidAlarmGetTimeElapsedRealtime32,
           kAndroidAlarmGetTimeElapsedRealtime64,
           // Linux Network Block Device requests observed in the field
           // https://crbug.com/1314105.
           NBD_CLEAR_SOCK, NBD_SET_BLKSIZE},
          Error(EINVAL))
      .Default(RestrictIoctl());
}

ResultExpr RestrictCloneParameters() {
  const Arg<unsigned long> flags(0);

  const uint64_t kForkFlags =
      CLONE_CHILD_SETTID | CLONE_CHILD_CLEARTID | SIGCHLD;
  const uint64_t kPthreadCreateFlags =
      CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD |
      CLONE_SYSVSEM | CLONE_SETTLS | CLONE_PARENT_SETTID | CLONE_CHILD_CLEARTID;

  const BoolExpr is_fork_or_pthread =
      AnyOf(flags == kForkFlags, flags == kPthreadCreateFlags);
  return If(is_fork_or_pthread, Allow()).Else(CrashSIGSYSClone());
}

bool IsBaselinePolicyAllowed(int sysno) {
  // The following syscalls are used in the renderer policy on Android but still
  // need to be allowed on Android and should not be filtered out of other
  // processes: mremap, ftruncate, ftruncate64, pwrite64, getcpu, fdatasync,
  // fsync, getrlimit, ugetrlimit, setrlimit.

  switch (sysno) {
    case __NR_epoll_pwait:
    case __NR_fdatasync:
    case __NR_flock:
    case __NR_fsync:
    case __NR_ftruncate:
#if defined(__i386__) || defined(__arm__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
    case __NR_ftruncate64:
#endif
#if defined(__x86_64__) || defined(__aarch64__)
    case __NR_newfstatat:
    case __NR_fstatfs:
#elif defined(__i386__) || defined(__arm__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
    case __NR_fstatat64:
    case __NR_fstatfs64:
#endif
#if defined(__arm__) || defined(__aarch64__)
    // getcpu() is allowed on ARM chips because it is used in
    // //third_party/cpuinfo/ on those chips.
    case __NR_getcpu:
#endif
#if defined(__i386__) || defined(__arm__) || defined(__mips__)
    case __NR_getdents:
#endif
    case __NR_getdents64:
    case __NR_getpriority:
    case __NR_membarrier:  // https://crbug.com/966433
#if defined(__i386__)
    // Used on pre-N to initialize threads in ART.
    case __NR_modify_ldt:
#endif
    case __NR_mremap:
    case __NR_msync:
      // File system access cannot be restricted with seccomp-bpf on Android,
      // since the JVM classloader and other Framework features require file
      // access. It may be possible to restrict the filesystem with SELinux.
      // Currently we rely on the app/service UID isolation to create a
      // filesystem "sandbox".
#if !defined(ARCH_CPU_ARM64)
    case __NR_open:
#endif
    case __NR_openat:
    case __NR_pwrite64:
    case __NR_rt_sigtimedwait:
#if defined(__i386__) || defined(__arm__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
    case __NR_rt_sigtimedwait_time64:
#endif
    case __NR_sched_getparam:
    case __NR_sched_getscheduler:
    case __NR_sched_setscheduler:
    case __NR_setpriority:
#if defined(__i386__)
    // Used on N+ instead of __NR_modify_ldt to initialize threads in ART.
    case __NR_set_thread_area:
#endif
    case __NR_set_tid_address:
#if defined(__i386__) || defined(__arm__)
    case __NR_ugetrlimit:
#else
    case __NR_getrlimit:
#endif

      // Permit socket operations so that renderers can connect to logd and
      // debuggerd. The arguments to socket() are further restricted below.
      // Note that on i386, both of these calls map to __NR_socketcall, which
      // is demultiplexed below.
#if defined(__x86_64__) || defined(__arm__) || defined(__aarch64__) || \
    defined(__mips__)
    case __NR_getsockopt:
    case __NR_connect:
#endif

      return true;
    default:
      return false;
  }
}

}  // namespace

BaselinePolicyAndroid::BaselinePolicyAndroid() = default;

BaselinePolicyAndroid::BaselinePolicyAndroid(const RuntimeOptions& options)
    : options_(options) {}

BaselinePolicyAndroid::~BaselinePolicyAndroid() = default;

ResultExpr BaselinePolicyAndroid::EvaluateSyscall(int sysno) const {
  if (sysno == __NR_clone) {
    if (options_.should_restrict_clone_params) {
      return RestrictCloneParameters();
    }
    return Allow();
  }
  if (sysno == __NR_sched_setaffinity || sysno == __NR_sched_getaffinity) {
    return Error(EPERM);
  }

  if (sysno == __NR_ioctl) {
    return RestrictAndroidIoctl(options_.allow_userfaultfd_ioctls);
  }

  // Ptrace is allowed so the crash reporter can fork in a renderer
  // and then ptrace the parent. https://crbug.com/933418
  if (sysno == __NR_ptrace) {
    return RestrictPtrace();
  }

  // https://crbug.com/766245
  if (sysno == __NR_process_vm_readv) {
    const Arg<pid_t> pid(0);
    return If(pid == policy_pid(), Allow())
           .Else(Error(EPERM));
  }

  if (!options_.should_restrict_renderer_syscalls) {
    if (sysno == __NR_sysinfo) {
      return Allow();
    }
    // https://crbug.com/655299
    if (sysno == __NR_clock_getres
#if defined(__i386__) || defined(__arm__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
        || sysno == __NR_clock_getres_time64
#endif
    ) {
    return RestrictClockID();
    }
  }

  // https://crbug.com/826289
  if (sysno == __NR_getrusage) {
    return RestrictGetrusage();
  }

#if defined(__x86_64__)
  if (sysno == __NR_arch_prctl) {
    const Arg<int> code(0);
    return If(code == ARCH_SET_GS, Allow()).Else(Error(EPERM));
  }
#endif

  // Restrict socket-related operations. On non-i386 platforms, these are
  // individual syscalls. On i386, the socketcall syscall demultiplexes many
  // socket operations.
#if defined(__x86_64__) || defined(__arm__) || defined(__aarch64__) || \
      defined(__mips__)
  if (sysno == __NR_socket) {
    const Arg<int> domain(0);
    const Arg<int> type(1);
    const Arg<int> protocol(2);
    return If(RestrictSocketArguments(domain, type, protocol), Allow())
           .Else(Error(EPERM));
  }

  // https://crbug.com/655300
  if (sysno == __NR_getsockname) {
    // Rather than blocking with SIGSYS, just return an error. This is not
    // documented to be a valid errno, but we will use it anyways.
    return Error(EPERM);
  }

  // https://crbug.com/682488, https://crbug.com/701137
  if (sysno == __NR_setsockopt) {
    // The baseline policy applies other restrictions to setsockopt.
    const Arg<int> level(1);
    const Arg<int> option(2);
    return If(AllOf(level == SOL_SOCKET,
                    AnyOf(option == SO_SNDTIMEO,
                          option == SO_RCVTIMEO,
                          option == SO_SNDBUF,
                          option == SO_REUSEADDR,
                          option == SO_PASSCRED)),
              Allow())
           .Else(BaselinePolicy::EvaluateSyscall(sysno));
  }
#elif defined(__i386__)
  if (sysno == __NR_socketcall) {
    // The baseline policy allows other socketcall sub-calls.
    const Arg<int> socketcall(0);
    return Switch(socketcall)
        .Cases({SYS_CONNECT,
                SYS_SOCKET,
                SYS_SETSOCKOPT,
                SYS_GETSOCKOPT},
               Allow())
        .Default(BaselinePolicy::EvaluateSyscall(sysno));
  }
#endif

  if (IsBaselinePolicyAllowed(sysno)) {
    return Allow();
  }

  return BaselinePolicy::EvaluateSyscall(sysno);
}

}  // namespace sandbox
