// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "sandbox/linux/syscall_broker/syscall_dispatcher.h"

#include <fcntl.h>

#include "base/check.h"
#include "base/logging.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"

namespace sandbox {
namespace syscall_broker {

#if defined(MEMORY_SANITIZER)
#define BROKER_UNPOISON_STRING(x) __msan_unpoison_string(x)
#else
#define BROKER_UNPOISON_STRING(x)
#endif

int SyscallDispatcher::DefaultStatForTesting(const char* pathname,
                                             bool follow_links,
                                             default_stat_struct* sb) {
#if defined(__NR_fstatat64)
  return Stat64(pathname, follow_links, sb);
#elif defined(__NR_newfstatat)
  return Stat(pathname, follow_links, sb);
#endif
}

int SyscallDispatcher::PerformStatat(const arch_seccomp_data& args,
                                     bool stat64) {
  if (static_cast<int>(args.args[0]) != AT_FDCWD)
    return -EPERM;
  // Only allow the AT_SYMLINK_NOFOLLOW flag which is used by some libc
  // implementations for lstat().
  if ((static_cast<int>(args.args[3]) & ~AT_SYMLINK_NOFOLLOW) != 0)
    return -EINVAL;

  const bool follow_links =
      !(static_cast<int>(args.args[3]) & AT_SYMLINK_NOFOLLOW);
  if (stat64) {
    return Stat64(reinterpret_cast<const char*>(args.args[1]), follow_links,
                  reinterpret_cast<struct kernel_stat64*>(args.args[2]));
  }

  return Stat(reinterpret_cast<const char*>(args.args[1]), follow_links,
              reinterpret_cast<struct kernel_stat*>(args.args[2]));
}

int SyscallDispatcher::PerformUnlinkat(const arch_seccomp_data& args) {
  if (static_cast<int>(args.args[0]) != AT_FDCWD)
    return -EPERM;

  int flags = static_cast<int>(args.args[2]);

  if (flags == AT_REMOVEDIR) {
    return Rmdir(reinterpret_cast<const char*>(args.args[1]));
  }

  if (flags != 0)
    return -EPERM;

  return Unlink(reinterpret_cast<const char*>(args.args[1]));
}

int SyscallDispatcher::DispatchSyscall(const arch_seccomp_data& args) {
  switch (args.nr) {
#if defined(__NR_access)
    case __NR_access:
      return Access(reinterpret_cast<const char*>(args.args[0]),
                    static_cast<int>(args.args[1]));
#endif
#if defined(__NR_faccessat)
    case __NR_faccessat:
#endif
#if defined(__NR_faccessat2)
    case __NR_faccessat2:
#endif
#if defined(__NR_faccessat) || defined(__NR_faccessat2)
      if (static_cast<int>(args.args[0]) != AT_FDCWD)
        return -EPERM;
      return Access(reinterpret_cast<const char*>(args.args[1]),
                    static_cast<int>(args.args[2]));
#endif
#if defined(__NR_mkdir)
    case __NR_mkdir:
      return Mkdir(reinterpret_cast<const char*>(args.args[0]),
                   static_cast<int>(args.args[1]));
#endif
#if defined(__NR_mkdirat)
    case __NR_mkdirat:
      if (static_cast<int>(args.args[0]) != AT_FDCWD)
        return -EPERM;
      return Mkdir(reinterpret_cast<const char*>(args.args[1]),
                   static_cast<int>(args.args[2]));
#endif
#if defined(__NR_open)
    case __NR_open:
      // http://crbug.com/372840
      BROKER_UNPOISON_STRING(reinterpret_cast<const char*>(args.args[0]));
      return Open(reinterpret_cast<const char*>(args.args[0]),
                  static_cast<int>(args.args[1]));
#endif
#if defined(__NR_openat)
    case __NR_openat:
      if (static_cast<int>(args.args[0]) != AT_FDCWD)
        return -EPERM;
      // http://crbug.com/372840
      BROKER_UNPOISON_STRING(reinterpret_cast<const char*>(args.args[1]));
      return Open(reinterpret_cast<const char*>(args.args[1]),
                  static_cast<int>(args.args[2]));
#endif
#if defined(__NR_readlink)
    case __NR_readlink:
      // http://crbug.com/372840
      BROKER_UNPOISON_STRING(reinterpret_cast<const char*>(args.args[0]));
      return Readlink(reinterpret_cast<const char*>(args.args[0]),
                      reinterpret_cast<char*>(args.args[1]),
                      static_cast<size_t>(args.args[2]));
#endif
#if defined(__NR_readlinkat)
    case __NR_readlinkat:
      if (static_cast<int>(args.args[0]) != AT_FDCWD)
        return -EPERM;
      // http://crbug.com/372840
      BROKER_UNPOISON_STRING(reinterpret_cast<const char*>(args.args[1]));
      return Readlink(reinterpret_cast<const char*>(args.args[1]),
                      reinterpret_cast<char*>(args.args[2]),
                      static_cast<size_t>(args.args[3]));
#endif
#if defined(__NR_rename)
    case __NR_rename:
      return Rename(reinterpret_cast<const char*>(args.args[0]),
                    reinterpret_cast<const char*>(args.args[1]));
#endif
#if defined(__NR_renameat)
    case __NR_renameat:
      if (static_cast<int>(args.args[0]) != AT_FDCWD ||
          static_cast<int>(args.args[2]) != AT_FDCWD) {
        return -EPERM;
      }
      return Rename(reinterpret_cast<const char*>(args.args[1]),
                    reinterpret_cast<const char*>(args.args[3]));
#endif
#if defined(__NR_renameat2)
    case __NR_renameat2:
      if (static_cast<int>(args.args[0]) != AT_FDCWD ||
          static_cast<int>(args.args[2]) != AT_FDCWD) {
        return -EPERM;
      }
      if (static_cast<int>(args.args[4]) != 0)
        return -EINVAL;
      return Rename(reinterpret_cast<const char*>(args.args[1]),
                    reinterpret_cast<const char*>(args.args[3]));
#endif
#if defined(__NR_rmdir)
    case __NR_rmdir:
      return Rmdir(reinterpret_cast<const char*>(args.args[0]));
#endif
#if defined(__NR_stat)
    case __NR_stat:
      return Stat(reinterpret_cast<const char*>(args.args[0]), true,
                  reinterpret_cast<struct kernel_stat*>(args.args[1]));
#endif
#if defined(__NR_stat64)
    case __NR_stat64:
      return Stat64(reinterpret_cast<const char*>(args.args[0]), true,
                    reinterpret_cast<struct kernel_stat64*>(args.args[1]));
#endif
#if defined(__NR_lstat)
    case __NR_lstat:
      // See https://crbug.com/847096
      BROKER_UNPOISON_STRING(reinterpret_cast<const char*>(args.args[0]));
      return Stat(reinterpret_cast<const char*>(args.args[0]), false,
                  reinterpret_cast<struct kernel_stat*>(args.args[1]));
#endif
#if defined(__NR_lstat64)
    case __NR_lstat64:
      // See https://crbug.com/847096
      BROKER_UNPOISON_STRING(reinterpret_cast<const char*>(args.args[0]));
      return Stat64(reinterpret_cast<const char*>(args.args[0]), false,
                    reinterpret_cast<struct kernel_stat64*>(args.args[1]));
#endif
#if defined(__NR_fstatat64)
    case __NR_fstatat64:
      return PerformStatat(args, /*stat64=*/true);
#endif
#if defined(__NR_newfstatat)
    case __NR_newfstatat:
      return PerformStatat(args, /*stat64=*/false);
#endif
#if defined(__NR_unlink)
    case __NR_unlink:
      return Unlink(reinterpret_cast<const char*>(args.args[0]));
#endif
#if defined(__NR_unlinkat)
    case __NR_unlinkat:
      return PerformUnlinkat(args);
#endif  // defined(__NR_unlinkat)
#if defined(__NR_inotify_add_watch)
    case __NR_inotify_add_watch:
      return InotifyAddWatch(static_cast<int>(args.args[0]),
                             reinterpret_cast<const char*>(args.args[1]),
                             static_cast<uint32_t>(args.args[2]));
#endif
    default:
      RAW_CHECK(false);
      return -ENOSYS;
  }
}

}  // namespace syscall_broker
}  // namespace sandbox
