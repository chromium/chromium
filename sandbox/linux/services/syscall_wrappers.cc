// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/services/syscall_wrappers.h"

#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <setjmp.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstring>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "sandbox/linux/system_headers/capability.h"
#include "sandbox/linux/system_headers/linux_signal.h"
#include "sandbox/linux/system_headers/linux_stat.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"

namespace sandbox {

pid_t sys_getpid(void) {
  return syscall(__NR_getpid);
}

pid_t sys_gettid(void) {
  return syscall(__NR_gettid);
}

ssize_t sys_write(int fd, const char* buffer, size_t buffer_size) {
  return syscall(__NR_write, fd, buffer, buffer_size);
}

long sys_clone(unsigned long flags,
               std::nullptr_t child_stack,
               pid_t* ptid,
               pid_t* ctid,
               std::nullptr_t tls) {
  const bool clone_tls_used = flags & CLONE_SETTLS;
  const bool invalid_ctid =
      (flags & (CLONE_CHILD_SETTID | CLONE_CHILD_CLEARTID)) && !ctid;
  const bool invalid_ptid = (flags & CLONE_PARENT_SETTID) && !ptid;

  // We do not support CLONE_VM.
  const bool clone_vm_used = flags & CLONE_VM;
  if (clone_tls_used || invalid_ctid || invalid_ptid || clone_vm_used) {
    RAW_LOG(FATAL, "Invalid usage of sys_clone");
  }

  if (ptid) MSAN_UNPOISON(ptid, sizeof(*ptid));
  if (ctid) MSAN_UNPOISON(ctid, sizeof(*ctid));
  // See kernel/fork.c in Linux. There is different ordering of sys_clone
  // parameters depending on CONFIG_CLONE_BACKWARDS* configuration options.
#if defined(ARCH_CPU_X86_64)
  return syscall(__NR_clone, flags, child_stack, ptid, ctid, tls);
#elif defined(ARCH_CPU_X86) || defined(ARCH_CPU_ARM_FAMILY) || \
    defined(ARCH_CPU_MIPS_FAMILY)
  // CONFIG_CLONE_BACKWARDS defined.
  return syscall(__NR_clone, flags, child_stack, ptid, tls, ctid);
#endif
}

long sys_clone(unsigned long flags) {
  return sys_clone(flags, nullptr, nullptr, nullptr, nullptr);
}

void sys_exit_group(int status) {
  syscall(__NR_exit_group, status);
}

int sys_seccomp(unsigned int operation,
                unsigned int flags,
                const struct sock_fprog* args) {
  return syscall(__NR_seccomp, operation, flags, args);
}

int sys_prlimit64(pid_t pid,
                  int resource,
                  const struct rlimit64* new_limit,
                  struct rlimit64* old_limit) {
  int res = syscall(__NR_prlimit64, pid, resource, new_limit, old_limit);
  if (res == 0 && old_limit) MSAN_UNPOISON(old_limit, sizeof(*old_limit));
  return res;
}

int sys_capget(cap_hdr* hdrp, cap_data* datap) {
  int res = syscall(__NR_capget, hdrp, datap);
  if (res == 0) {
    if (hdrp) MSAN_UNPOISON(hdrp, sizeof(*hdrp));
    if (datap) MSAN_UNPOISON(datap, sizeof(*datap));
  }
  return res;
}

int sys_capset(cap_hdr* hdrp, const cap_data* datap) {
  return syscall(__NR_capset, hdrp, datap);
}

int sys_getresuid(uid_t* ruid, uid_t* euid, uid_t* suid) {
  int res;
#if defined(ARCH_CPU_X86) || defined(ARCH_CPU_ARMEL)
  // On 32-bit x86 or 32-bit arm, getresuid supports 16bit values only.
  // Use getresuid32 instead.
  res = syscall(__NR_getresuid32, ruid, euid, suid);
#else
  res = syscall(__NR_getresuid, ruid, euid, suid);
#endif
  if (res == 0) {
    if (ruid) MSAN_UNPOISON(ruid, sizeof(*ruid));
    if (euid) MSAN_UNPOISON(euid, sizeof(*euid));
    if (suid) MSAN_UNPOISON(suid, sizeof(*suid));
  }
  return res;
}

int sys_getresgid(gid_t* rgid, gid_t* egid, gid_t* sgid) {
  int res;
#if defined(ARCH_CPU_X86) || defined(ARCH_CPU_ARMEL)
  // On 32-bit x86 or 32-bit arm, getresgid supports 16bit values only.
  // Use getresgid32 instead.
  res = syscall(__NR_getresgid32, rgid, egid, sgid);
#else
  res = syscall(__NR_getresgid, rgid, egid, sgid);
#endif
  if (res == 0) {
    if (rgid) MSAN_UNPOISON(rgid, sizeof(*rgid));
    if (egid) MSAN_UNPOISON(egid, sizeof(*egid));
    if (sgid) MSAN_UNPOISON(sgid, sizeof(*sgid));
  }
  return res;
}

int sys_chroot(const char* path) {
  return syscall(__NR_chroot, path);
}

int sys_unshare(int flags) {
  return syscall(__NR_unshare, flags);
}

int sys_sigprocmask(int how, const sigset_t* set, std::nullptr_t oldset) {
  // In some toolchain (in particular Android and PNaCl toolchain),
  // sigset_t is 32 bits, but the Linux ABI uses more.
  LinuxSigSet linux_value;
  std::memset(&linux_value, 0, sizeof(LinuxSigSet));
  std::memcpy(&linux_value, set, std::min(sizeof(sigset_t),
                                          sizeof(LinuxSigSet)));

  return syscall(__NR_rt_sigprocmask, how, &linux_value, nullptr,
                 sizeof(linux_value));
}

int sys_sigaction(int signum,
                  const struct sigaction* act,
                  struct sigaction* oldact) {
  return sigaction(signum, act, oldact);
}

int sys_stat(const char* path, struct kernel_stat* stat_buf) {
  int res;
#if !defined(__NR_stat)
  res = syscall(__NR_newfstatat, AT_FDCWD, path, stat_buf, 0);
#else
  res = syscall(__NR_stat, path, stat_buf);
#endif
  if (res == 0)
    MSAN_UNPOISON(stat_buf, sizeof(*stat_buf));
  return res;
}

int sys_lstat(const char* path, struct kernel_stat* stat_buf) {
  int res;
#if !defined(__NR_lstat)
  res = syscall(__NR_newfstatat, AT_FDCWD, path, stat_buf, AT_SYMLINK_NOFOLLOW);
#else
  res = syscall(__NR_lstat, path, stat_buf);
#endif
  if (res == 0)
    MSAN_UNPOISON(stat_buf, sizeof(*stat_buf));
  return res;
}

int sys_fstatat64(int dirfd,
                  const char* pathname,
                  struct kernel_stat64* stat_buf,
                  int flags) {
#if defined(__NR_fstatat64)
  int res = syscall(__NR_fstatat64, dirfd, pathname, stat_buf, flags);
  if (res == 0)
    MSAN_UNPOISON(stat_buf, sizeof(*stat_buf));
  return res;
#else  // defined(__NR_fstatat64)
  // We should not reach here on 64-bit systems, as the *stat*64() are only
  // necessary on 32-bit.
  RAW_CHECK(false);
  return -ENOSYS;
#endif
}

int landlock_create_ruleset(const struct landlock_ruleset_attr* const attr,
                            const size_t size,
                            const uint32_t flags) {
  return syscall(__NR_landlock_create_ruleset, attr, size, flags);
}

}  // namespace sandbox
