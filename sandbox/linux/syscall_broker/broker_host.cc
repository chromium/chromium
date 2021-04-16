// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/syscall_broker/broker_host.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>
#include <utility>

#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "sandbox/linux/syscall_broker/broker_command.h"
#include "sandbox/linux/syscall_broker/broker_permission_list.h"
#include "sandbox/linux/syscall_broker/broker_simple_message.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"

namespace sandbox {
namespace syscall_broker {

namespace {

// A little open(2) wrapper to handle some oddities for us. In the general case
// make a direct system call since we want to keep in control of the broker
// process' system calls profile to be able to loosely sandbox it.
int sys_open(const char* pathname, int flags) {
  // Hardcode mode to rw------- when creating files.
  int mode = (flags & O_CREAT) ? 0600 : 0;
  return syscall(__NR_openat, AT_FDCWD, pathname, flags, mode);
}

bool GetPathAndFlags(BrokerSimpleMessage* message,
                     const char** pathname,
                     int* flags) {
  return message->ReadString(pathname) && message->ReadInt(flags);
}

// Perform access(2) on |requested_filename| with mode |mode| if allowed by our
// permission_list. Write the syscall return value (-errno) to |reply|.
void AccessFileForIPC(const BrokerCommandSet& allowed_command_set,
                      const BrokerPermissionList& permission_list,
                      const std::string& requested_filename,
                      int mode,
                      BrokerSimpleMessage* reply) {
  const char* file_to_access = NULL;
  if (!CommandAccessIsSafe(allowed_command_set, permission_list,
                           requested_filename.c_str(), mode, &file_to_access)) {
    RAW_CHECK(reply->AddIntToMessage(-permission_list.denied_errno()));
    return;
  }

  RAW_CHECK(file_to_access);
  if (access(file_to_access, mode) < 0) {
    RAW_CHECK(reply->AddIntToMessage(-errno));
    return;
  }

  RAW_CHECK(reply->AddIntToMessage(0));
}

// Performs mkdir(2) on |filename| with mode |mode| if allowed by our
// permission_list. Write the syscall return value (-errno) to |reply|.
void MkdirFileForIPC(const BrokerCommandSet& allowed_command_set,
                     const BrokerPermissionList& permission_list,
                     const std::string& filename,
                     int mode,
                     BrokerSimpleMessage* reply) {
  const char* file_to_access = nullptr;
  if (!CommandMkdirIsSafe(allowed_command_set, permission_list,
                          filename.c_str(), &file_to_access)) {
    RAW_CHECK(reply->AddIntToMessage(-permission_list.denied_errno()));
    return;
  }
  if (mkdir(file_to_access, mode) < 0) {
    RAW_CHECK(reply->AddIntToMessage(-errno));
    return;
  }
  RAW_CHECK(reply->AddIntToMessage(0));
}

// Open |requested_filename| with |flags| if allowed by our permission list.
// Write the syscall return value (-errno) to |reply| and return the
// file descriptor in the |opened_file| if relevant.
void OpenFileForIPC(const BrokerCommandSet& allowed_command_set,
                    const BrokerPermissionList& permission_list,
                    const std::string& requested_filename,
                    int flags,
                    BrokerSimpleMessage* reply,
                    base::ScopedFD* opened_file) {
  const char* file_to_open = NULL;
  bool unlink_after_open = false;
  if (!CommandOpenIsSafe(allowed_command_set, permission_list,
                         requested_filename.c_str(), flags, &file_to_open,
                         &unlink_after_open)) {
    RAW_CHECK(reply->AddIntToMessage(-permission_list.denied_errno()));
    return;
  }

  CHECK(file_to_open);
  opened_file->reset(sys_open(file_to_open, flags));
  if (!opened_file->is_valid()) {
    RAW_CHECK(reply->AddIntToMessage(-errno));
    return;
  }

  if (unlink_after_open)
    unlink(file_to_open);

  RAW_CHECK(reply->AddIntToMessage(0));
}

// Perform rename(2) on |old_filename| to |new_filename| and write the
// result to |return_val|.
void RenameFileForIPC(const BrokerCommandSet& allowed_command_set,
                      const BrokerPermissionList& permission_list,
                      const std::string& old_filename,
                      const std::string& new_filename,
                      BrokerSimpleMessage* reply) {
  const char* old_file_to_access = nullptr;
  const char* new_file_to_access = nullptr;
  if (!CommandRenameIsSafe(allowed_command_set, permission_list,
                           old_filename.c_str(), new_filename.c_str(),
                           &old_file_to_access, &new_file_to_access)) {
    RAW_CHECK(reply->AddIntToMessage(-permission_list.denied_errno()));
    return;
  }
  if (rename(old_file_to_access, new_file_to_access) < 0) {
    RAW_CHECK(reply->AddIntToMessage(-errno));
    return;
  }
  RAW_CHECK(reply->AddIntToMessage(0));
}

// Perform readlink(2) on |filename| using a buffer of MAX_PATH bytes.
void ReadlinkFileForIPC(const BrokerCommandSet& allowed_command_set,
                        const BrokerPermissionList& permission_list,
                        const std::string& filename,
                        BrokerSimpleMessage* reply) {
  const char* file_to_access = nullptr;
  if (!CommandReadlinkIsSafe(allowed_command_set, permission_list,
                             filename.c_str(), &file_to_access)) {
    RAW_CHECK(reply->AddIntToMessage(-permission_list.denied_errno()));
    return;
  }
  char buf[PATH_MAX];
  ssize_t result = readlink(file_to_access, buf, sizeof(buf));
  if (result < 0) {
    RAW_CHECK(reply->AddIntToMessage(-errno));
    return;
  }
  RAW_CHECK(reply->AddIntToMessage(result));
  RAW_CHECK(reply->AddDataToMessage(buf, result));
}

void RmdirFileForIPC(const BrokerCommandSet& allowed_command_set,
                     const BrokerPermissionList& permission_list,
                     const std::string& requested_filename,
                     BrokerSimpleMessage* reply) {
  const char* file_to_access = nullptr;
  if (!CommandRmdirIsSafe(allowed_command_set, permission_list,
                          requested_filename.c_str(), &file_to_access)) {
    RAW_CHECK(reply->AddIntToMessage(-permission_list.denied_errno()));
    return;
  }
  if (rmdir(file_to_access) < 0) {
    RAW_CHECK(reply->AddIntToMessage(-errno));
    return;
  }
  RAW_CHECK(reply->AddIntToMessage(0));
}

// Perform stat(2) on |requested_filename| and write the result to
// |return_val|.
void StatFileForIPC(const BrokerCommandSet& allowed_command_set,
                    const BrokerPermissionList& permission_list,
                    BrokerCommand command_type,
                    const std::string& requested_filename,
                    bool follow_links,
                    BrokerSimpleMessage* reply) {
  const char* file_to_access = nullptr;
  if (!CommandStatIsSafe(allowed_command_set, permission_list,
                         requested_filename.c_str(), &file_to_access)) {
    RAW_CHECK(reply->AddIntToMessage(-permission_list.denied_errno()));
    return;
  }
  if (command_type == COMMAND_STAT) {
    struct stat sb;
    int sts =
        follow_links ? stat(file_to_access, &sb) : lstat(file_to_access, &sb);
    if (sts < 0) {
      RAW_CHECK(reply->AddIntToMessage(-errno));
      return;
    }
    RAW_CHECK(reply->AddIntToMessage(0));
    RAW_CHECK(
        reply->AddDataToMessage(reinterpret_cast<char*>(&sb), sizeof(sb)));
  } else {
    DCHECK(command_type == COMMAND_STAT64);
    struct stat64 sb;
    int sts = follow_links ? stat64(file_to_access, &sb)
                           : lstat64(file_to_access, &sb);
    if (sts < 0) {
      RAW_CHECK(reply->AddIntToMessage(-errno));
      return;
    }
    RAW_CHECK(reply->AddIntToMessage(0));
    RAW_CHECK(
        reply->AddDataToMessage(reinterpret_cast<char*>(&sb), sizeof(sb)));
  }
}

void UnlinkFileForIPC(const BrokerCommandSet& allowed_command_set,
                      const BrokerPermissionList& permission_list,
                      const std::string& requested_filename,
                      BrokerSimpleMessage* message) {
  const char* file_to_access = nullptr;
  if (!CommandUnlinkIsSafe(allowed_command_set, permission_list,
                           requested_filename.c_str(), &file_to_access)) {
    RAW_CHECK(message->AddIntToMessage(-permission_list.denied_errno()));
    return;
  }
  if (unlink(file_to_access) < 0) {
    RAW_CHECK(message->AddIntToMessage(-errno));
    return;
  }
  RAW_CHECK(message->AddIntToMessage(0));
}

// Handle a |command_type| request contained in |iter| and write the reply
// to |reply|.
bool HandleRemoteCommand(const BrokerCommandSet& allowed_command_set,
                         const BrokerPermissionList& permission_list,
                         BrokerSimpleMessage* message,
                         BrokerSimpleMessage* reply,
                         base::ScopedFD* opened_file) {
  // Message structure:
  //   int:    command type
  //   char[]: pathname
  //   int:    flags
  int command_type;
  if (!message->ReadInt(&command_type))
    return false;

  switch (command_type) {
    case COMMAND_ACCESS: {
      const char* requested_filename;
      int flags = 0;
      if (!GetPathAndFlags(message, &requested_filename, &flags))
        return false;
      AccessFileForIPC(allowed_command_set, permission_list, requested_filename,
                       flags, reply);
      break;
    }
    case COMMAND_MKDIR: {
      const char* requested_filename;
      int mode = 0;
      if (!GetPathAndFlags(message, &requested_filename, &mode))
        return false;
      MkdirFileForIPC(allowed_command_set, permission_list, requested_filename,
                      mode, reply);
      break;
    }
    case COMMAND_OPEN: {
      const char* requested_filename;
      int flags = 0;
      if (!GetPathAndFlags(message, &requested_filename, &flags))
        return false;
      OpenFileForIPC(allowed_command_set, permission_list, requested_filename,
                     flags, reply, opened_file);
      break;
    }
    case COMMAND_READLINK: {
      const char* filename;
      if (!message->ReadString(&filename))
        return false;

      ReadlinkFileForIPC(allowed_command_set, permission_list, filename, reply);
      break;
    }
    case COMMAND_RENAME: {
      const char* old_filename;
      if (!message->ReadString(&old_filename))
        return false;

      const char* new_filename;
      if (!message->ReadString(&new_filename))
        return false;

      RenameFileForIPC(allowed_command_set, permission_list, old_filename,
                       new_filename, reply);
      break;
    }
    case COMMAND_RMDIR: {
      const char* requested_filename;
      if (!message->ReadString(&requested_filename))
        return false;
      RmdirFileForIPC(allowed_command_set, permission_list, requested_filename,
                      reply);
      break;
    }
    case COMMAND_STAT:
    case COMMAND_STAT64: {
      const char* requested_filename;
      if (!message->ReadString(&requested_filename))
        return false;
      int follow_links;
      if (!message->ReadInt(&follow_links))
        return false;
      StatFileForIPC(allowed_command_set, permission_list,
                     static_cast<BrokerCommand>(command_type),
                     requested_filename, !!follow_links, reply);
      break;
    }
    case COMMAND_UNLINK: {
      const char* requested_filename;
      if (!message->ReadString(&requested_filename))
        return false;
      UnlinkFileForIPC(allowed_command_set, permission_list, requested_filename,
                       reply);
      break;
    }
    default:
      LOG(ERROR) << "Invalid IPC command";
      return false;
  }
  return true;
}

}  // namespace

BrokerHost::BrokerHost(const BrokerPermissionList& broker_permission_list,
                       const BrokerCommandSet& allowed_command_set,
                       BrokerChannel::EndPoint ipc_channel)
    : broker_permission_list_(broker_permission_list),
      allowed_command_set_(allowed_command_set),
      ipc_channel_(std::move(ipc_channel)) {}

BrokerHost::~BrokerHost() = default;

// Handle a request on the IPC channel ipc_channel_.
// A request should have a file descriptor attached on which we will reply and
// that we will then close.
// A request should start with an int that will be used as the command type.
void BrokerHost::LoopAndHandleRequests() {
  for (;;) {
    BrokerSimpleMessage message;
    errno = 0;
    base::ScopedFD temporary_ipc;
    const ssize_t msg_len =
        message.RecvMsgWithFlags(ipc_channel_.get(), 0, &temporary_ipc);

    if (msg_len == 0 || (msg_len == -1 && errno == ECONNRESET)) {
      // EOF from the client, or the client died, we should finish looping.
      return;
    }

    // The client sends exactly one file descriptor, on which we
    // will write the reply.
    if (msg_len < 0) {
      PLOG(ERROR) << "Error reading message from the client";
      continue;
    }

    BrokerSimpleMessage reply;
    base::ScopedFD opened_file;
    if (!HandleRemoteCommand(allowed_command_set_, broker_permission_list_,
                             &message, &reply, &opened_file)) {
      // Does not exit if we received a malformed message.
      LOG(ERROR) << "Received malformed message from the client";
      continue;
    }

    ssize_t sent = reply.SendMsg(temporary_ipc.get(), opened_file.get());
    if (sent < 0)
      LOG(ERROR) << "sent failed";
  }
}

}  // namespace syscall_broker

}  // namespace sandbox
