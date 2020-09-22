// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SYSCALL_BROKER_SYSCALL_DISPATCHER_H_
#define SANDBOX_LINUX_SYSCALL_BROKER_SYSCALL_DISPATCHER_H_

#include <sys/stat.h>
#include <cstddef>

#include "sandbox/linux/system_headers/linux_seccomp.h"

namespace sandbox {
namespace syscall_broker {

// An abstract class that defines all the system calls we perform for the
// sandboxed process.
class SyscallDispatcher {
 public:
  // Emulates access()/faccessat().
  // X_OK will always return an error in practice since the broker process
  // doesn't support execute permissions.
  virtual int Access(const char* pathname, int mode) const = 0;

  // Emulates mkdir()/mkdirat.
  virtual int Mkdir(const char* path, int mode) const = 0;

  // Emulates open()/openat().
  // The implementation only supports certain white listed flags and will
  // return -EPERM on other flags.
  virtual int Open(const char* pathname, int flags) const = 0;

  // Emulates readlink()/readlinkat().
  virtual int Readlink(const char* path, char* buf, size_t bufsize) const = 0;

  // Emulates rename()/renameat()/renameat2().
  virtual int Rename(const char* oldpath, const char* newpath) const = 0;

  // Emulates rmdir().
  virtual int Rmdir(const char* path) const = 0;

  // Emulates stat()/stat64()/lstat()/lstat64()/fstatat()/newfstatat().
  virtual int Stat(const char* pathname,
                   bool follow_links,
                   struct stat* sb) const = 0;
  virtual int Stat64(const char* pathname,
                     bool follow_links,
                     struct stat64* sb) const = 0;

  // Emulates unlink()/unlinkat().
  virtual int Unlink(const char* unlink) const = 0;

  // Validates the args passed to a *statat*() syscall and performs the syscall
  // using Stat() or Stat64().
  int PerformStatat(const arch_seccomp_data& args, bool arch64);

  // Reads the syscall number and arguments, imposes some policy (e.g. the *at()
  // system calls must only allow AT_FDCWD as the first argument), and
  // dispatches to the correct method from above.
  // Async-signal-safe since this might be called in a signal handler.
  int DispatchSyscall(const arch_seccomp_data& args);

 protected:
  virtual ~SyscallDispatcher() = default;
};

}  // namespace syscall_broker
}  // namespace sandbox

#endif  // SANDBOX_LINUX_SYSCALL_BROKER_SYSCALL_DISPATCHER_H_
