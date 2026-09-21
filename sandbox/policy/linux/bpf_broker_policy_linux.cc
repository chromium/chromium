// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/linux/bpf_broker_policy_linux.h"

#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/syscall_broker/broker_command.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"

using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::ResultExpr;

namespace sandbox {
namespace policy {

BrokerProcessPolicy::BrokerProcessPolicy(
    const syscall_broker::BrokerCommandSet& allowed_command_set)
    : allowed_command_set_(allowed_command_set) {}

BrokerProcessPolicy::~BrokerProcessPolicy() {}

ResultExpr BrokerProcessPolicy::EvaluateSyscall(int sysno) const {
  switch (sysno) {
#if defined(__NR_access)
    case __NR_access:
      if (allowed_command_set_.test(syscall_broker::COMMAND_ACCESS))
        return Allow();
      break;
#endif
#if defined(__NR_faccessat)
    case __NR_faccessat:
#endif
#if defined(__NR_faccessat2)
    case __NR_faccessat2:
#endif
#if defined(__NR_faccessat) || defined(__NR_faccessat2)
      if (allowed_command_set_.test(syscall_broker::COMMAND_ACCESS))
        return Allow();
      break;
#endif
#if defined(__NR_mkdir)
    case __NR_mkdir:
      if (allowed_command_set_.test(syscall_broker::COMMAND_MKDIR))
        return Allow();
      break;
#endif
#if defined(__NR_mkdirat)
    case __NR_mkdirat:
      if (allowed_command_set_.test(syscall_broker::COMMAND_MKDIR))
        return Allow();
      break;
#endif
#if defined(__NR_open)
    case __NR_open:
      if (allowed_command_set_.test(syscall_broker::COMMAND_OPEN))
        return Allow();
      break;
#endif
#if defined(__NR_openat)
    case __NR_openat:
      if (allowed_command_set_.test(syscall_broker::COMMAND_OPEN))
        return Allow();
      break;
#endif
#if defined(__NR_rename)
    case __NR_rename:
      if (allowed_command_set_.test(syscall_broker::COMMAND_RENAME))
        return Allow();
      break;
#endif
#if defined(__NR_renameat)
    case __NR_renameat:
      if (allowed_command_set_.test(syscall_broker::COMMAND_RENAME))
        return Allow();
      break;
#endif
#if defined(__NR_stat)
    case __NR_stat:
      if (allowed_command_set_.test(syscall_broker::COMMAND_STAT))
        return Allow();
      break;
#endif
#if defined(__NR_stat64)
    case __NR_stat64:
      if (allowed_command_set_.test(syscall_broker::COMMAND_STAT))
        return Allow();
      break;
#endif
#if defined(__NR_lstat)
    case __NR_lstat:
      if (allowed_command_set_.test(syscall_broker::COMMAND_STAT))
        return Allow();
      break;
#endif
#if defined(__NR_lstat64)
    case __NR_lstat64:
      if (allowed_command_set_.test(syscall_broker::COMMAND_STAT))
        return Allow();
      break;
#endif
#if defined(__NR_fstatat64)
    case __NR_fstatat64:
      if (allowed_command_set_.test(syscall_broker::COMMAND_STAT))
        return Allow();
      break;
#endif
#if defined(__NR_newfstatat)
    case __NR_newfstatat:
      if (allowed_command_set_.test(syscall_broker::COMMAND_STAT))
        return Allow();
      break;
#endif
#if defined(__NR_readlink)
    case __NR_readlink:
      if (allowed_command_set_.test(syscall_broker::COMMAND_READLINK))
        return Allow();
      break;
#endif
#if defined(__NR_readlinkat)
    case __NR_readlinkat:
      if (allowed_command_set_.test(syscall_broker::COMMAND_READLINK))
        return Allow();
      break;
#endif
#if defined(__NR_rmdir)
    case __NR_rmdir:
      if (allowed_command_set_.test(syscall_broker::COMMAND_RMDIR))
        return Allow();
      break;
#endif
#if defined(__NR_unlink)
    case __NR_unlink:
      // NOTE: Open() uses unlink() to make "temporary" files.
      if (allowed_command_set_.test(syscall_broker::COMMAND_OPEN) ||
          allowed_command_set_.test(syscall_broker::COMMAND_UNLINK)) {
        return Allow();
      }
      break;
#endif
#if defined(__NR_unlinkat)
    case __NR_unlinkat:
      // NOTE: Open() uses unlink() to make "temporary" files.
      if (allowed_command_set_.test(syscall_broker::COMMAND_OPEN) ||
          allowed_command_set_.test(syscall_broker::COMMAND_UNLINK)) {
        return Allow();
      }
      break;
#endif
#if defined(__NR_inotify_add_watch)
    case __NR_inotify_add_watch:
      if (allowed_command_set_.test(
              syscall_broker::COMMAND_INOTIFY_ADD_WATCH)) {
        return Allow();
      }
      break;
#endif
    default:
      break;
  }
  return BPFBasePolicy::EvaluateSyscall(sysno);
}

}  // namespace policy
}  // namespace sandbox
