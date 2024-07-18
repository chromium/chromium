// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "sandbox/linux/syscall_broker/broker_client.h"

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <utility>

#include "base/check.h"
#include "build/build_config.h"
#include "sandbox/linux/syscall_broker/broker_channel.h"
#include "sandbox/linux/syscall_broker/broker_command.h"
#include "sandbox/linux/syscall_broker/broker_permission_list.h"
#include "sandbox/linux/syscall_broker/broker_simple_message.h"

#if BUILDFLAG(IS_ANDROID) && !defined(MSG_CMSG_CLOEXEC)
#define MSG_CMSG_CLOEXEC 0x40000000
#endif

namespace sandbox {
namespace syscall_broker {

BrokerClient::BrokerClient(const BrokerSandboxConfig& policy,
                           BrokerChannel::EndPoint ipc_channel,
                           bool fast_check_in_client)
    : policy_(policy),
      ipc_channel_(std::move(ipc_channel)),
      fast_check_in_client_(fast_check_in_client) {}

BrokerClient::~BrokerClient() = default;

int BrokerClient::Access(const char* pathname, int mode) const {
  if (!pathname)
    return -EFAULT;

  if (fast_check_in_client_ &&
      !CommandAccessIsSafe(policy_->allowed_command_set,
                           *policy_->file_permissions, pathname, mode)) {
    return -policy_->file_permissions->denied_errno();
  }
  return PathAndFlagsSyscall(COMMAND_ACCESS, pathname, mode);
}

int BrokerClient::Mkdir(const char* pathname, int mode) const {
  if (!pathname)
    return -EFAULT;

  if (fast_check_in_client_ &&
      !CommandMkdirIsSafe(policy_->allowed_command_set,
                          *policy_->file_permissions, pathname)) {
    return -policy_->file_permissions->denied_errno();
  }
  return PathAndFlagsSyscall(COMMAND_MKDIR, pathname, mode);
}

int BrokerClient::Open(const char* pathname, int flags) const {
  if (!pathname)
    return -EFAULT;

  if (fast_check_in_client_ &&
      !CommandOpenIsSafe(policy_->allowed_command_set,
                         *policy_->file_permissions, pathname, flags)
           .first) {
    return -policy_->file_permissions->denied_errno();
  }
  return PathAndFlagsSyscallReturningFD(COMMAND_OPEN, pathname, flags);
}

int BrokerClient::Readlink(const char* path, char* buf, size_t bufsize) const {
  if (!path || !buf)
    return -EFAULT;

  if (fast_check_in_client_ &&
      !CommandReadlinkIsSafe(policy_->allowed_command_set,
                             *policy_->file_permissions, path)) {
    return -policy_->file_permissions->denied_errno();
  }

  // Message structure:
  //   int:    syscall_type
  //   char[]: pathname, including '\0' terminator
  BrokerSimpleMessage message;
  RAW_CHECK(message.AddIntToMessage(COMMAND_READLINK));
  RAW_CHECK(message.AddStringToMessage(path));

  base::ScopedFD returned_fd;
  BrokerSimpleMessage reply;
  ssize_t msg_len =
      message.SendRecvMsgWithFlags(ipc_channel_.get(), 0, &returned_fd, &reply);
  if (msg_len < 0)
    return msg_len;

  int return_value = -1;
  size_t return_length = 0;
  const char* return_data = nullptr;
  if (!reply.ReadInt(&return_value))
    return -ENOMEM;
  if (return_value < 0)
    return return_value;

  if (!reply.ReadData(&return_data, &return_length))
    return -ENOMEM;
  if (return_length < 0)
    return -ENOMEM;
  // Sanity check that our broker is behaving correctly.
  RAW_CHECK(return_length == static_cast<size_t>(return_value));

  if (return_length > bufsize) {
    return_length = bufsize;
  }
  memcpy(buf, return_data, return_length);
  return return_length;
}

int BrokerClient::Rename(const char* oldpath, const char* newpath) const {
  if (!oldpath || !newpath)
    return -EFAULT;

  if (fast_check_in_client_ &&
      !CommandRenameIsSafe(policy_->allowed_command_set,
                           *policy_->file_permissions, oldpath, newpath)
           .first) {
    return -policy_->file_permissions->denied_errno();
  }

  BrokerSimpleMessage message;
  RAW_CHECK(message.AddIntToMessage(COMMAND_RENAME));
  RAW_CHECK(message.AddStringToMessage(oldpath));
  RAW_CHECK(message.AddStringToMessage(newpath));

  base::ScopedFD returned_fd;
  BrokerSimpleMessage reply;
  ssize_t msg_len =
      message.SendRecvMsgWithFlags(ipc_channel_.get(), 0, &returned_fd, &reply);

  if (msg_len < 0)
    return msg_len;

  int return_value = -1;
  if (!reply.ReadInt(&return_value))
    return -ENOMEM;

  return return_value;
}

int BrokerClient::Rmdir(const char* path) const {
  if (!path)
    return -EFAULT;

  if (fast_check_in_client_ &&
      !CommandRmdirIsSafe(policy_->allowed_command_set,
                          *policy_->file_permissions, path)) {
    return -policy_->file_permissions->denied_errno();
  }
  return PathOnlySyscall(COMMAND_RMDIR, path);
}

int BrokerClient::Stat(const char* pathname,
                       bool follow_links,
                       struct kernel_stat* sb) const {
  if (!pathname || !sb)
    return -EFAULT;

  if (fast_check_in_client_ &&
      !CommandStatIsSafe(policy_->allowed_command_set,
                         *policy_->file_permissions, pathname)) {
    return -policy_->file_permissions->denied_errno();
  }
  return StatFamilySyscall(COMMAND_STAT, pathname, follow_links, sb,
                           sizeof(*sb));
}

int BrokerClient::Stat64(const char* pathname,
                         bool follow_links,
                         struct kernel_stat64* sb) const {
  if (!pathname || !sb)
    return -EFAULT;

  if (fast_check_in_client_ &&
      !CommandStatIsSafe(policy_->allowed_command_set,
                         *policy_->file_permissions, pathname)) {
    return -policy_->file_permissions->denied_errno();
  }
  return StatFamilySyscall(COMMAND_STAT64, pathname, follow_links, sb,
                           sizeof(*sb));
}

int BrokerClient::Unlink(const char* path) const {
  if (!path)
    return -EFAULT;

  if (fast_check_in_client_ &&
      !CommandUnlinkIsSafe(policy_->allowed_command_set,
                           *policy_->file_permissions, path)) {
    return -policy_->file_permissions->denied_errno();
  }
  return PathOnlySyscall(COMMAND_UNLINK, path);
}

int BrokerClient::InotifyAddWatch(int fd,
                                  const char* pathname,
                                  uint32_t mask) const {
  if (!pathname)
    return -EFAULT;

  if (fast_check_in_client_ &&
      !CommandInotifyAddWatchIsSafe(policy_->allowed_command_set,
                                    *policy_->file_permissions, pathname,
                                    mask)) {
    return -policy_->file_permissions->denied_errno();
  }

  BrokerSimpleMessage message;
  RAW_CHECK(message.AddIntToMessage(COMMAND_INOTIFY_ADD_WATCH));
  RAW_CHECK(message.AddStringToMessage(pathname));
  RAW_CHECK(message.AddIntToMessage(mask));

  BrokerSimpleMessage reply;
  ssize_t msg_len = message.SendRecvMsgWithFlagsMultipleFds(
      ipc_channel_.get(), 0, base::span<const int>(&fd, 1u), {}, &reply);

  if (msg_len < 0)
    return msg_len;

  int return_value = -1;
  if (!reply.ReadInt(&return_value))
    return -ENOMEM;

  return return_value;
}

int BrokerClient::PathOnlySyscall(BrokerCommand syscall_type,
                                  const char* pathname) const {
  BrokerSimpleMessage message;
  RAW_CHECK(message.AddIntToMessage(syscall_type));
  RAW_CHECK(message.AddStringToMessage(pathname));

  base::ScopedFD returned_fd;
  BrokerSimpleMessage reply;
  ssize_t msg_len =
      message.SendRecvMsgWithFlags(ipc_channel_.get(), 0, &returned_fd, &reply);

  if (msg_len < 0)
    return msg_len;

  int return_value = -1;
  if (!reply.ReadInt(&return_value))
    return -ENOMEM;

  return return_value;
}

// Make a remote system call over IPC for syscalls that take a path and
// flags (currently access() and mkdir()) but do not return a FD.
// Will return -errno like a real system call.
// This function needs to be async signal safe.
int BrokerClient::PathAndFlagsSyscall(BrokerCommand syscall_type,
                                      const char* pathname,
                                      int flags) const {
  BrokerSimpleMessage message;
  RAW_CHECK(message.AddIntToMessage(syscall_type));
  RAW_CHECK(message.AddStringToMessage(pathname));
  RAW_CHECK(message.AddIntToMessage(flags));

  base::ScopedFD returned_fd;
  BrokerSimpleMessage reply;
  ssize_t msg_len =
      message.SendRecvMsgWithFlags(ipc_channel_.get(), 0, &returned_fd, &reply);

  if (msg_len < 0)
    return -ENOMEM;

  int return_value = -1;
  if (!reply.ReadInt(&return_value))
    return -ENOMEM;

  return return_value;
}

// Make a remote system call over IPC for syscalls that take a path and flags
// as arguments and return FDs (currently open()).
// Will return -errno like a real system call.
// This function needs to be async signal safe.
int BrokerClient::PathAndFlagsSyscallReturningFD(BrokerCommand syscall_type,
                                                 const char* pathname,
                                                 int flags) const {
  // For this "remote system call" to work, we need to handle any flag that
  // cannot be sent over a Unix socket in a special way.
  // See the comments around kCurrentProcessOpenFlagsMask.
  int recvmsg_flags = 0;
  if (syscall_type == COMMAND_OPEN && (flags & kCurrentProcessOpenFlagsMask)) {
    // This implementation only knows about O_CLOEXEC, someone needs to look at
    // this code if other flags are added.
    static_assert(kCurrentProcessOpenFlagsMask == O_CLOEXEC,
                  "Must update broker client to handle other flags");

    recvmsg_flags |= MSG_CMSG_CLOEXEC;
    flags &= ~kCurrentProcessOpenFlagsMask;
  }

  BrokerSimpleMessage message;
  RAW_CHECK(message.AddIntToMessage(syscall_type));
  RAW_CHECK(message.AddStringToMessage(pathname));
  RAW_CHECK(message.AddIntToMessage(flags));

  base::ScopedFD returned_fd;
  BrokerSimpleMessage reply;
  ssize_t msg_len = message.SendRecvMsgWithFlags(
      ipc_channel_.get(), recvmsg_flags, &returned_fd, &reply);

  if (msg_len < 0)
    return -ENOMEM;

  int return_value = -1;
  if (!reply.ReadInt(&return_value))
    return -ENOMEM;
  if (return_value < 0)
    return return_value;

  // We have a real file descriptor to return.
  RAW_CHECK(returned_fd.is_valid());
  return returned_fd.release();
}

// Make a remote system call over IPC for syscalls that take a path
// and return stat buffers (currently stat() and stat64()).
// Will return -errno like a real system call.
// This function needs to be async signal safe.
int BrokerClient::StatFamilySyscall(BrokerCommand syscall_type,
                                    const char* pathname,
                                    bool follow_links,
                                    void* result_ptr,
                                    size_t expected_result_size) const {
  BrokerSimpleMessage message;
  RAW_CHECK(message.AddIntToMessage(syscall_type));
  RAW_CHECK(message.AddStringToMessage(pathname));
  RAW_CHECK(message.AddIntToMessage(static_cast<int>(follow_links)));

  base::ScopedFD returned_fd;
  BrokerSimpleMessage reply;
  ssize_t msg_len =
      message.SendRecvMsgWithFlags(ipc_channel_.get(), 0, &returned_fd, &reply);

  if (msg_len < 0)
    return msg_len;

  int return_value = -1;
  size_t return_length = 0;
  const char* return_data = nullptr;

  if (!reply.ReadInt(&return_value))
    return -ENOMEM;
  if (return_value < 0)
    return return_value;
  if (!reply.ReadData(&return_data, &return_length))
    return -ENOMEM;
  if (static_cast<size_t>(return_length) != expected_result_size)
    return -ENOMEM;
  memcpy(result_ptr, return_data, expected_result_size);
  return return_value;
}

// static
intptr_t BrokerClient::SIGSYS_Handler(const arch_seccomp_data& args,
                                      void* aux_broker_client) {
  RAW_CHECK(aux_broker_client);
  auto* broker_client = static_cast<BrokerClient*>(aux_broker_client);
  return broker_client->DispatchSyscall(args);
}

}  // namespace syscall_broker
}  // namespace sandbox
