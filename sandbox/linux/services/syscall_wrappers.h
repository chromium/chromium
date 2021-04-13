// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SERVICES_SYSCALL_WRAPPERS_H_
#define SANDBOX_LINUX_SERVICES_SYSCALL_WRAPPERS_H_

#include <signal.h>
#include <stdint.h>
#include <sys/types.h>

#include <cstddef>

#include "sandbox/sandbox_export.h"

struct sock_fprog;
struct rlimit64;
struct cap_hdr;
struct cap_data;

namespace sandbox {

// Provide direct system call wrappers for a few common system calls.
// These are guaranteed to perform a system call and do not rely on things such
// as caching the current pid (c.f. getpid()) unless otherwise specified.

SANDBOX_EXPORT pid_t sys_getpid(void);

SANDBOX_EXPORT pid_t sys_gettid(void);

SANDBOX_EXPORT ssize_t sys_write(int fd,
                                 const char* buffer,
                                 size_t buffer_size);

SANDBOX_EXPORT long sys_clone(unsigned long flags);

// |regs| is not supported and must be passed as nullptr. |child_stack| must be
// nullptr, since otherwise this function cannot safely return. As a
// consequence, this function does not support CLONE_VM.
SANDBOX_EXPORT long sys_clone(unsigned long flags,
                              std::nullptr_t child_stack,
                              pid_t* ptid,
                              pid_t* ctid,
                              std::nullptr_t regs);

SANDBOX_EXPORT void sys_exit_group(int status);

// The official system call takes |args| as void*  (in order to be extensible),
// but add more typing for the cases that are currently used.
SANDBOX_EXPORT int sys_seccomp(unsigned int operation,
                               unsigned int flags,
                               const struct sock_fprog* args);

// Some libcs do not expose a prlimit64 wrapper.
SANDBOX_EXPORT int sys_prlimit64(pid_t pid,
                                 int resource,
                                 const struct rlimit64* new_limit,
                                 struct rlimit64* old_limit);

// Some libcs do not expose capget/capset wrappers. We want to use these
// directly in order to avoid pulling in libcap2.
SANDBOX_EXPORT int sys_capget(struct cap_hdr* hdrp, struct cap_data* datap);
SANDBOX_EXPORT int sys_capset(struct cap_hdr* hdrp,
                              const struct cap_data* datap);

// Some libcs do not expose getresuid/getresgid wrappers.
SANDBOX_EXPORT int sys_getresuid(uid_t* ruid, uid_t* euid, uid_t* suid);
SANDBOX_EXPORT int sys_getresgid(gid_t* rgid, gid_t* egid, gid_t* sgid);

// Some libcs do not expose a chroot wrapper.
SANDBOX_EXPORT int sys_chroot(const char* path);

// Some libcs do not expose a unshare wrapper.
SANDBOX_EXPORT int sys_unshare(int flags);

// Some libcs do not expose a sigprocmask. Note that oldset must be a nullptr,
// because of some ABI gap between toolchain's and Linux's.
SANDBOX_EXPORT int sys_sigprocmask(int how,
                                   const sigset_t* set,
                                   std::nullptr_t oldset);

// Some libcs do not expose a sigaction().
SANDBOX_EXPORT int sys_sigaction(int signum,
                                 const struct sigaction* act,
                                 struct sigaction* oldact);

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SERVICES_SYSCALL_WRAPPERS_H_
