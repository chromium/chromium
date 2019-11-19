// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/syscall_broker/broker_client.h"

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <utility>

#include "base/logging.h"
#include "base/posix/unix_domain_socket.h"
#include "build/build_config.h"
#include "sandbox/linux/syscall_broker/broker_channel.h"
#include "sandbox/linux/syscall_broker/broker_command.h"
#include "sandbox/linux/syscall_broker/broker_permission_list.h"
#include "sandbox/linux/syscall_broker/broker_simple_message.h"

#if defined(OS_ANDROID) && !defined(MSG_CMSG_CLOEXEC)
#define MSG_CMSG_CLOEXEC 0x40000000
#endif

namespace sandbox {
namespace syscall_broker {

BrokerClient::BrokerClient(const BrokerPermissionList& broker_permission_list,
                           BrokerChannel::EndPoint ipc_channel,
                           const BrokerCommandSet& allowed_command_set,
                           bool fast_check_in_client)
    : broker_permission_list_(broker_permission_list),
      ipc_channel_(std::move(ipc_channel)),
      allowed_command_set_(allowed_command_set),
      fast_check_in_client_(fast_check_in_client) {}

BrokerClient::~BrokerClient() {}

int BrokerClient::Access(const char* pathname, int mode) const {
  if (!pathname)
    return -EFAULT;

  if (fast_check_in_client_ &&
      !CommandAccessIsSafe(allowed_command_set_, broker_permission_list_,
                           pathname, mode, nullptr)) {
    return -broker_permission_list_.denied_errno();
  }
  return PathAndFlagsSyscall(COMMAND_ACCESS, pathname, mode);
}

int BrokerClient::Mkdir(const char* pathname, int mode) const {
  if (!pathname)
    return -EFAULT;

  if (fast_check_in_client_ &&
      !CommandMkdirIsSafe(allowed_command_set_, broker_permission_list_,
                          pathname, nullptr)) {
    return -broker_permission_list_.denied_errno();
  }
  return PathAndFlagsSyscall(COMMAND_MKDIR, pathname, mode);
}

int BrokerClient::Open(const char* pathname, int flags) const {
  if (!pathname)
    return -EFAULT;

  if (fast_check_in_client_ &&
      !CommandOpenIsSafe(allowed_command_set_, broker_permission_list_,
                         pathname, flags, nullptr, nullptr)) {
    return -broker_permission_list_.denied_errno();
  }
  return PathAndFlagsSyscallReturningFD(COMMAND_OPEN, pathname, flags);
}

int BrokerClient::Readlink(const char* path, char* buf, size_t bufsize) const {
  if (!path || !buf)
    return -EFAULT;

  if (fast_check_in_client_ &&
      !CommandReadlinkIsSafe(allowed_command_set_, broker_permission_list_,
                             path, nullptr)) {
    return -broker_permission_list_.denied_errno();
  }

  // Message structure:
  //   int:    syscall_type
  //   char[]: pathname, including '\0' terminator
  BrokerSimpleMessage message;
  RAW_CHECK(message.AddIntToMessage(COMMAND_READLINK));
  RAW_CHECK(message.AddStringToMessage(path));

  int returned_fd = -1;
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

  if (static_cast<size_t>(return_length) > bufsize)
    return -ENAMETOOLONG;
  memcpy(buf, return_data, return_length);
  return return_value;
}

int BrokerClient::Rename(const char* oldpath, const char* newpath) const {
  if (!oldpath || !newpath)
    return -EFAULT;

  if (fast_check_in_client_ &&
      !CommandRenameIsSafe(allowed_command_set_, broker_permission_list_,
                           oldpath, newpath, nullptr, nullptr)) {
    return -broker_permission_list_.denied_errno();
  }

  BrokerSimpleMessage message;
  RAW_CHECK(message.AddIntToMessage(COMMAND_RENAME));
  RAW_CHECK(message.AddStringToMessage(oldpath));
  RAW_CHECK(message.AddStringToMessage(newpath));

  int returned_fd = -1;
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
      !CommandRmdirIsSafe(allowed_command_set_, broker_permission_list_, path,
                          nullptr)) {
    return -broker_permission_list_.denied_errno();
  }
  return PathOnlySyscall(COMMAND_RMDIR, path);
}

int BrokerClient::Stat(const char* pathname,
                       bool follow_links,
                       struct stat* sb) const {
  if (!pathname || !sb)
    return -EFAULT;

  if (fast_check_in_client_ &&
      !CommandStatIsSafe(allowed_command_set_, broker_permission_list_,
                         pathname, nullptr)) {
    return -broker_permission_list_.denied_errno();
  }
  return StatFamilySyscall(COMMAND_STAT, pathname, follow_links, sb,
                           sizeof(*sb));
}

int BrokerClient::Stat64(const char* pathname,
                         bool follow_links,
                         struct stat64* sb) const {
  if (!pathname || !sb)
    return -EFAULT;

  if (fast_check_in_client_ &&
      !CommandStatIsSafe(allowed_command_set_, broker_permission_list_,
                         pathname, nullptr)) {
    return -broker_permission_list_.denied_errno();
  }
  return StatFamilySyscall(COMMAND_STAT64, pathname, follow_links, sb,
                           sizeof(*sb));
}

int BrokerClient::Unlink(const char* path) const {
  if (!path)
    return -EFAULT;

  if (fast_check_in_client_ &&
      !CommandUnlinkIsSafe(allowed_command_set_, broker_permission_list_, path,
                           nullptr)) {
    return -broker_permission_list_.denied_errno();
  }
  return PathOnlySyscall(COMMAND_UNLINK, path);
}

int BrokerClient::PathOnlySyscall(BrokerCommand syscall_type,
                                  const char* pathname) const {
  BrokerSimpleMessage message;
  RAW_CHECK(message.AddIntToMessage(syscall_type));
  RAW_CHECK(message.AddStringToMessage(pathname));

  int returned_fd = -1;
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

  int returned_fd = -1;
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

  int returned_fd = -1;
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
  RAW_CHECK(returned_fd >= 0);
  return returned_fd;
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

  int returned_fd = -1;
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

}  // namespace syscall_broker
}  // namespace sandbox
