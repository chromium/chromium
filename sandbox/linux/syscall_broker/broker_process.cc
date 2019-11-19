// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/syscall_broker/broker_process.h"

#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/process_metrics.h"
#include "build/build_config.h"
#include "sandbox/linux/syscall_broker/broker_channel.h"
#include "sandbox/linux/syscall_broker/broker_client.h"
#include "sandbox/linux/syscall_broker/broker_host.h"

namespace sandbox {

namespace syscall_broker {

BrokerProcess::BrokerProcess(
    int denied_errno,
    const syscall_broker::BrokerCommandSet& allowed_command_set,
    const std::vector<syscall_broker::BrokerFilePermission>& permissions,
    bool fast_check_in_client,
    bool quiet_failures_for_tests)
    : initialized_(false),
      broker_pid_(-1),
      fast_check_in_client_(fast_check_in_client),
      quiet_failures_for_tests_(quiet_failures_for_tests),
      allowed_command_set_(allowed_command_set),
      broker_permission_list_(denied_errno, permissions) {}

BrokerProcess::~BrokerProcess() {
  if (initialized_) {
    if (broker_client_.get()) {
      // Closing the socket should be enough to notify the child to die,
      // unless it has been duplicated.
      CloseChannel();
    }
    PCHECK(0 == kill(broker_pid_, SIGKILL));
    siginfo_t process_info;
    // Reap the child.
    int ret = HANDLE_EINTR(waitid(P_PID, broker_pid_, &process_info, WEXITED));
    PCHECK(0 == ret);
  }
}

bool BrokerProcess::Init(
    base::OnceCallback<bool(void)> broker_process_init_callback) {
  CHECK(!initialized_);
  BrokerChannel::EndPoint ipc_reader;
  BrokerChannel::EndPoint ipc_writer;
  BrokerChannel::CreatePair(&ipc_reader, &ipc_writer);

#if !defined(THREAD_SANITIZER)
  DCHECK_EQ(1, base::GetNumberOfThreads(base::GetCurrentProcessHandle()));
#endif
  int child_pid = fork();
  if (child_pid == -1)
    return false;

  if (child_pid) {
    VLOG(3) << "BrokerProcess::Init(), in parent, child is " << child_pid;
    // We are the parent and we have just forked our broker process.
    ipc_reader.reset();
    broker_pid_ = child_pid;
    broker_client_ = std::make_unique<BrokerClient>(
        broker_permission_list_, std::move(ipc_writer), allowed_command_set_,
        fast_check_in_client_);
    initialized_ = true;
    return true;
  }

  // We are the broker process. Make sure to close the writer's end so that
  // we get notified if the client disappears.
  VLOG(3) << "BrokerProcess::Init(), in child";
  ipc_writer.reset();
  CHECK(std::move(broker_process_init_callback).Run());
  BrokerHost broker_host(broker_permission_list_, allowed_command_set_,
                         std::move(ipc_reader));
  for (;;) {
    switch (broker_host.HandleRequest()) {
      case BrokerHost::RequestStatus::LOST_CLIENT:
        _exit(1);
      case BrokerHost::RequestStatus::SUCCESS:
      case BrokerHost::RequestStatus::FAILURE:
        continue;
    }
  }
  _exit(1);
  NOTREACHED();
  return false;
}

bool BrokerProcess::IsSyscallAllowed(int sysno) const {
  switch (sysno) {
#if !defined(__aarch64__)
    case __NR_access:
#endif
    case __NR_faccessat:
      return !fast_check_in_client_ ||
             allowed_command_set_.test(COMMAND_ACCESS);

#if !defined(__aarch64__)
    case __NR_mkdir:
#endif
    case __NR_mkdirat:
      return !fast_check_in_client_ || allowed_command_set_.test(COMMAND_MKDIR);

#if !defined(__aarch64__)
    case __NR_open:
#endif
    case __NR_openat:
      return !fast_check_in_client_ || allowed_command_set_.test(COMMAND_OPEN);

#if !defined(__aarch64__)
    case __NR_readlink:
#endif
    case __NR_readlinkat:
      return !fast_check_in_client_ ||
             allowed_command_set_.test(COMMAND_READLINK);

#if !defined(__aarch64__)
    case __NR_rename:
#endif
    case __NR_renameat:
    case __NR_renameat2:
      return !fast_check_in_client_ ||
             allowed_command_set_.test(COMMAND_RENAME);

#if !defined(__aarch64__)
    case __NR_rmdir:
      return !fast_check_in_client_ || allowed_command_set_.test(COMMAND_RMDIR);
#endif

#if !defined(__aarch64__)
    case __NR_stat:
    case __NR_lstat:
#endif
#if defined(__NR_fstatat)
    case __NR_fstatat:
#endif
#if defined(__x86_64__) || defined(__aarch64__)
    case __NR_newfstatat:
#endif
      return !fast_check_in_client_ || allowed_command_set_.test(COMMAND_STAT);

#if defined(__i386__) || defined(__arm__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
    case __NR_stat64:
    case __NR_lstat64:
      // For security purposes, map stat64 to COMMAND_STAT permission. The
      // separate COMMAND_STAT64 only exists to broker different-sized
      // argument structs.
      return !fast_check_in_client_ || allowed_command_set_.test(COMMAND_STAT);
#endif

#if !defined(__aarch64__)
    case __NR_unlink:
#endif
    case __NR_unlinkat:
      return !fast_check_in_client_ ||
             allowed_command_set_.test(COMMAND_UNLINK);

    default:
      return false;
  }
}

void BrokerProcess::CloseChannel() {
  broker_client_.reset();
}

int BrokerProcess::Access(const char* pathname, int mode) const {
  RAW_CHECK(initialized_);
  return broker_client_->Access(pathname, mode);
}

int BrokerProcess::Mkdir(const char* path, int mode) const {
  RAW_CHECK(initialized_);
  return broker_client_->Mkdir(path, mode);
}

int BrokerProcess::Open(const char* pathname, int flags) const {
  RAW_CHECK(initialized_);
  return broker_client_->Open(pathname, flags);
}

int BrokerProcess::Readlink(const char* path, char* buf, size_t bufsize) const {
  RAW_CHECK(initialized_);
  return broker_client_->Readlink(path, buf, bufsize);
}

int BrokerProcess::Rename(const char* oldpath, const char* newpath) const {
  RAW_CHECK(initialized_);
  return broker_client_->Rename(oldpath, newpath);
}

int BrokerProcess::Rmdir(const char* pathname) const {
  RAW_CHECK(initialized_);
  return broker_client_->Rmdir(pathname);
}

int BrokerProcess::Stat(const char* pathname,
                        bool follow_links,
                        struct stat* sb) const {
  RAW_CHECK(initialized_);
  return broker_client_->Stat(pathname, follow_links, sb);
}

int BrokerProcess::Stat64(const char* pathname,
                          bool follow_links,
                          struct stat64* sb) const {
  RAW_CHECK(initialized_);
  return broker_client_->Stat64(pathname, follow_links, sb);
}

int BrokerProcess::Unlink(const char* pathname) const {
  RAW_CHECK(initialized_);
  return broker_client_->Unlink(pathname);
}

#if defined(MEMORY_SANITIZER)
#define BROKER_UNPOISON_STRING(x) __msan_unpoison_string(x)
#else
#define BROKER_UNPOISON_STRING(x)
#endif

// static
intptr_t BrokerProcess::SIGSYS_Handler(const sandbox::arch_seccomp_data& args,
                                       void* aux_broker_process) {
  RAW_CHECK(aux_broker_process);
  auto* broker_process = static_cast<BrokerProcess*>(aux_broker_process);
  switch (args.nr) {
#if defined(__NR_access)
    case __NR_access:
      return broker_process->Access(reinterpret_cast<const char*>(args.args[0]),
                                    static_cast<int>(args.args[1]));
#endif
#if defined(__NR_faccessat)
    case __NR_faccessat:
      if (static_cast<int>(args.args[0]) != AT_FDCWD)
        return -EPERM;
      return broker_process->Access(reinterpret_cast<const char*>(args.args[1]),
                                    static_cast<int>(args.args[2]));
#endif
#if defined(__NR_mkdir)
    case __NR_mkdir:
      return broker_process->Mkdir(reinterpret_cast<const char*>(args.args[0]),
                                   static_cast<int>(args.args[1]));
#endif
#if defined(__NR_mkdirat)
    case __NR_mkdirat:
      if (static_cast<int>(args.args[0]) != AT_FDCWD)
        return -EPERM;
      return broker_process->Mkdir(reinterpret_cast<const char*>(args.args[1]),
                                   static_cast<int>(args.args[2]));
#endif
#if defined(__NR_open)
    case __NR_open:
      // http://crbug.com/372840
      BROKER_UNPOISON_STRING(reinterpret_cast<const char*>(args.args[0]));
      return broker_process->Open(reinterpret_cast<const char*>(args.args[0]),
                                  static_cast<int>(args.args[1]));
#endif
#if defined(__NR_openat)
    case __NR_openat:
      if (static_cast<int>(args.args[0]) != AT_FDCWD)
        return -EPERM;
      // http://crbug.com/372840
      BROKER_UNPOISON_STRING(reinterpret_cast<const char*>(args.args[1]));
      return broker_process->Open(reinterpret_cast<const char*>(args.args[1]),
                                  static_cast<int>(args.args[2]));
#endif
#if defined(__NR_readlink)
    case __NR_readlink:
      return broker_process->Readlink(
          reinterpret_cast<const char*>(args.args[0]),
          reinterpret_cast<char*>(args.args[1]),
          static_cast<size_t>(args.args[2]));
#endif
#if defined(__NR_readlinkat)
    case __NR_readlinkat:
      if (static_cast<int>(args.args[0]) != AT_FDCWD)
        return -EPERM;
      return broker_process->Readlink(
          reinterpret_cast<const char*>(args.args[1]),
          reinterpret_cast<char*>(args.args[2]),
          static_cast<size_t>(args.args[3]));
#endif
#if defined(__NR_rename)
    case __NR_rename:
      return broker_process->Rename(
          reinterpret_cast<const char*>(args.args[0]),
          reinterpret_cast<const char*>(args.args[1]));
#endif
#if defined(__NR_renameat)
    case __NR_renameat:
      if (static_cast<int>(args.args[0]) != AT_FDCWD ||
          static_cast<int>(args.args[2]) != AT_FDCWD) {
        return -EPERM;
      }
      return broker_process->Rename(
          reinterpret_cast<const char*>(args.args[1]),
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
      return broker_process->Rename(
          reinterpret_cast<const char*>(args.args[1]),
          reinterpret_cast<const char*>(args.args[3]));
#endif
#if defined(__NR_rmdir)
    case __NR_rmdir:
      return broker_process->Rmdir(reinterpret_cast<const char*>(args.args[0]));
#endif
#if defined(__NR_stat)
    case __NR_stat:
      return broker_process->Stat(reinterpret_cast<const char*>(args.args[0]),
                                  true,
                                  reinterpret_cast<struct stat*>(args.args[1]));
#endif
#if defined(__NR_stat64)
    case __NR_stat64:
      return broker_process->Stat64(
          reinterpret_cast<const char*>(args.args[0]), true,
          reinterpret_cast<struct stat64*>(args.args[1]));
#endif
#if defined(__NR_lstat)
    case __NR_lstat:
      // See https://crbug.com/847096
      BROKER_UNPOISON_STRING(reinterpret_cast<const char*>(args.args[0]));
      return broker_process->Stat(reinterpret_cast<const char*>(args.args[0]),
                                  false,
                                  reinterpret_cast<struct stat*>(args.args[1]));
#endif
#if defined(__NR_lstat64)
    case __NR_lstat64:
      // See https://crbug.com/847096
      BROKER_UNPOISON_STRING(reinterpret_cast<const char*>(args.args[0]));
      return broker_process->Stat64(
          reinterpret_cast<const char*>(args.args[0]), false,
          reinterpret_cast<struct stat64*>(args.args[1]));
#endif
#if defined(__NR_fstatat)
    case __NR_fstatat:
      if (static_cast<int>(args.args[0]) != AT_FDCWD)
        return -EPERM;
      if (static_cast<int>(args.args[3]) != 0)
        return -EINVAL;
      return broker_process->Stat(reinterpret_cast<const char*>(args.args[1]),
                                  true,
                                  reinterpret_cast<struct stat*>(args.args[2]));
#endif
#if defined(__NR_newfstatat)
    case __NR_newfstatat:
      if (static_cast<int>(args.args[0]) != AT_FDCWD)
        return -EPERM;
      if (static_cast<int>(args.args[3]) != 0)
        return -EINVAL;
      return broker_process->Stat(reinterpret_cast<const char*>(args.args[1]),
                                  true,
                                  reinterpret_cast<struct stat*>(args.args[2]));
#endif
#if defined(__NR_unlink)
    case __NR_unlink:
      return broker_process->Unlink(
          reinterpret_cast<const char*>(args.args[0]));
#endif
#if defined(__NR_unlinkat)
    case __NR_unlinkat:
      // TODO(tsepez): does not support AT_REMOVEDIR flag.
      if (static_cast<int>(args.args[0]) != AT_FDCWD)
        return -EPERM;
      return broker_process->Unlink(
          reinterpret_cast<const char*>(args.args[1]));
#endif
    default:
      RAW_CHECK(false);
      return -ENOSYS;
  }
}

}  // namespace syscall_broker
}  // namespace sandbox
