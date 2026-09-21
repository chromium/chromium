// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SYSCALL_BROKER_SYSCALL_DISPATCHER_H_
#define SANDBOX_LINUX_SYSCALL_BROKER_SYSCALL_DISPATCHER_H_

#include <sys/stat.h>
#include <cstddef>

#include "sandbox/linux/system_headers/linux_seccomp.h"
#include "sandbox/linux/system_headers/linux_stat.h"
#include "sandbox/sandbox_export.h"

namespace sandbox {
namespace syscall_broker {

// An abstract class that defines all the system calls we perform for the
// sandboxed process.
class SANDBOX_EXPORT SyscallDispatcher {
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
  // Stat64 is only available on 32-bit systems.
  virtual int Stat(const char* pathname,
                   bool follow_links,
                   struct kernel_stat* sb) const = 0;
  virtual int Stat64(const char* pathname,
                     bool follow_links,
                     struct kernel_stat64* sb) const = 0;

  // Emulates unlink()/unlinkat().
  virtual int Unlink(const char* unlink) const = 0;

  // Emulates inotify_add_watch().
  virtual int InotifyAddWatch(int fd,
                              const char* pathname,
                              uint32_t mask) const = 0;

  // Different architectures use a different syscall from the stat family by
  // default in glibc. E.g. 32-bit systems use *stat*64() and fill out struct
  // kernel_stat64, whereas 64-bit systems use *stat*() and fill out struct
  // kernel_stat. Some tests want to call the SyscallDispatcher directly, and
  // should be using the default stat in order to test against glibc.
  int DefaultStatForTesting(const char* pathname,
                            bool follow_links,
                            default_stat_struct* sb);

  // Validates the args passed to a *statat*() syscall and performs the syscall
  // using Stat(), or on 32-bit systems it uses Stat64() for the *statat64()
  // syscalls.
  int PerformStatat(const arch_seccomp_data& args, bool stat64);

  // Validates the args passed to an unlinkat() syscall and performs the syscall
  // using either Unlink() or Rmdir().
  int PerformUnlinkat(const arch_seccomp_data& args);

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
