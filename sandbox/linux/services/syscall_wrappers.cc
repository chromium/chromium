// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/services/syscall_wrappers.h"

#include <pthread.h>
#include <sched.h>
#include <setjmp.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstring>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "sandbox/linux/system_headers/capability.h"
#include "sandbox/linux/system_headers/linux_signal.h"
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

// When this is built with PNaCl toolchain, we should always use sys_sigaction
// below, because sigaction() provided by the toolchain is incompatible with
// Linux's ABI.
#if !defined(OS_NACL_NONSFI)
int sys_sigaction(int signum,
                  const struct sigaction* act,
                  struct sigaction* oldact) {
  return sigaction(signum, act, oldact);
}
#else
#if defined(ARCH_CPU_X86_FAMILY)

// On x86_64, sa_restorer is required. We specify it on x86 as well in order to
// support kernels with VDSO disabled.
#if !defined(SA_RESTORER)
#define SA_RESTORER 0x04000000
#endif

// XSTR(__NR_foo) expands to a string literal containing the value value of
// __NR_foo.
#define STR(x) #x
#define XSTR(x) STR(x)

// rt_sigreturn is a special system call that interacts with the user land
// stack. Thus, here prologue must not be created, which implies syscall()
// does not work properly, too. Note that rt_sigreturn does not return.
// TODO(rickyz): These assembly functions may still break stack unwinding on
// nonsfi NaCl builds.
#if defined(ARCH_CPU_X86_64)

extern "C" {
  void sys_rt_sigreturn();
}

asm(
    ".text\n"
    "sys_rt_sigreturn:\n"
    "mov $" XSTR(__NR_rt_sigreturn) ", %eax\n"
    "syscall\n");

#elif defined(ARCH_CPU_X86)
extern "C" {
  void sys_sigreturn();
  void sys_rt_sigreturn();
}

asm(
    ".text\n"
    "sys_rt_sigreturn:\n"
    "mov $" XSTR(__NR_rt_sigreturn) ", %eax\n"
    "int $0x80\n"

    "sys_sigreturn:\n"
    "pop %eax\n"
    "mov $" XSTR(__NR_sigreturn) ", %eax\n"
    "int $0x80\n");
#else
#error "Unsupported architecture."
#endif

#undef STR
#undef XSTR

#endif

int sys_sigaction(int signum,
                  const struct sigaction* act,
                  struct sigaction* oldact) {
  LinuxSigAction linux_act = {};
  if (act) {
    linux_act.kernel_handler = act->sa_handler;
    std::memcpy(&linux_act.sa_mask, &act->sa_mask,
                std::min(sizeof(linux_act.sa_mask), sizeof(act->sa_mask)));
    linux_act.sa_flags = act->sa_flags;

#if defined(ARCH_CPU_X86_FAMILY)
    if (!(linux_act.sa_flags & SA_RESTORER)) {
      linux_act.sa_flags |= SA_RESTORER;
#if defined(ARCH_CPU_X86_64)
      linux_act.sa_restorer = sys_rt_sigreturn;
#elif defined(ARCH_CPU_X86)
      linux_act.sa_restorer =
          linux_act.sa_flags & SA_SIGINFO ? sys_rt_sigreturn : sys_sigreturn;
#else
#error "Unsupported architecture."
#endif
    }
#endif
  }

  LinuxSigAction linux_oldact = {};
  int result = syscall(__NR_rt_sigaction, signum, act ? &linux_act : nullptr,
                       oldact ? &linux_oldact : nullptr,
                       sizeof(LinuxSigSet));

  if (result == 0 && oldact) {
    oldact->sa_handler = linux_oldact.kernel_handler;
    sigemptyset(&oldact->sa_mask);
    std::memcpy(&oldact->sa_mask, &linux_oldact.sa_mask,
                std::min(sizeof(linux_act.sa_mask), sizeof(act->sa_mask)));
    oldact->sa_flags = linux_oldact.sa_flags;
  }
  return result;
}

#endif  // defined(MEMORY_SANITIZER)

}  // namespace sandbox
