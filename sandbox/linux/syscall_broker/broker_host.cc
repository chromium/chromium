// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "sandbox/linux/syscall_broker/broker_host.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stddef.h>
#include <sys/inotify.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include <array>
#include <string>
#include <tuple>
#include <utility>

#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "sandbox/linux/services/syscall_wrappers.h"
#include "sandbox/linux/syscall_broker/broker_command.h"
#include "sandbox/linux/syscall_broker/broker_permission_list.h"
#include "sandbox/linux/syscall_broker/broker_simple_message.h"
#include "sandbox/linux/system_headers/linux_stat.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"

namespace sandbox {
namespace syscall_broker {

namespace {

const char kProcSelf[] = "/proc/self/";
const size_t kProcSelfNumChars = sizeof(kProcSelf) - 1;

// A little open(2) wrapper to handle some oddities for us. In the general case
// make a direct system call since we want to keep in control of the broker
// process' system calls profile to be able to loosely sandbox it.
int sys_open(const char* pathname, int flags) {
  // Hardcode mode to rw------- when creating files.
  int mode = (flags & O_CREAT) ? 0600 : 0;
  return syscall(__NR_openat, AT_FDCWD, pathname, flags, mode);
}

}  // namespace

// Applies a rewrite from /proc/self/ to /proc/[pid of sandboxed process]/.
// Returns either a rewritten or the original pathname.
std::optional<std::string> BrokerHost::RewritePathname(const char* pathname) {
  if (base::StartsWith(pathname, kProcSelf)) {
    return base::StringPrintf("/proc/%d/%s", sandboxed_process_pid_,
                              pathname + kProcSelfNumChars);
  }

  return std::nullopt;
}

std::optional<std::pair<const char*, int>> BrokerHost::GetPathAndFlags(
    BrokerSimpleMessage* message) {
  const char* pathname;
  int flags;
  if (!message->ReadString(&pathname) || !message->ReadInt(&flags)) {
    return std::nullopt;
  }
  return {{pathname, flags}};
}

// Perform access(2) on |requested_filename| with mode |mode| if allowed by our
// permission_list. Write the syscall return value (-errno) to |reply|.
void BrokerHost::AccessFileForIPC(const char* requested_filename,
                                  int mode,
                                  BrokerSimpleMessage* reply) {
  const char* file_to_access =
      CommandAccessIsSafe(policy_->allowed_command_set,
                          *policy_->file_permissions, requested_filename, mode);
  if (!file_to_access) {
    RAW_CHECK(
        reply->AddIntToMessage(-policy_->file_permissions->denied_errno()));
    return;
  }

  std::optional<std::string> rewritten_filename =
      RewritePathname(file_to_access);
  if (rewritten_filename.has_value()) {
    file_to_access = rewritten_filename.value().c_str();
  }

  if (access(file_to_access, mode) < 0) {
    RAW_CHECK(reply->AddIntToMessage(-errno));
    return;
  }

  RAW_CHECK(reply->AddIntToMessage(0));
}

// Performs mkdir(2) on |requested_filename| with mode |mode| if allowed by our
// permission_list. Write the syscall return value (-errno) to |reply|.
void BrokerHost::MkdirFileForIPC(const char* requested_filename,
                                 int mode,
                                 BrokerSimpleMessage* reply) {
  const char* file_to_access =
      CommandMkdirIsSafe(policy_->allowed_command_set,
                         *policy_->file_permissions, requested_filename);
  if (!file_to_access) {
    RAW_CHECK(
        reply->AddIntToMessage(-policy_->file_permissions->denied_errno()));
    return;
  }

  std::optional<std::string> rewritten_filename =
      RewritePathname(file_to_access);
  if (rewritten_filename.has_value()) {
    file_to_access = rewritten_filename.value().c_str();
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
void BrokerHost::OpenFileForIPC(const char* requested_filename,
                                int flags,
                                BrokerSimpleMessage* reply,
                                base::ScopedFD* opened_file) {
  const char* file_to_open = nullptr;
  bool unlink_after_open = false;
  std::tie(file_to_open, unlink_after_open) =
      CommandOpenIsSafe(policy_->allowed_command_set,
                        *policy_->file_permissions, requested_filename, flags);
  if (!file_to_open) {
    RAW_CHECK(
        reply->AddIntToMessage(-policy_->file_permissions->denied_errno()));
    return;
  }

  std::optional<std::string> rewritten_filename = RewritePathname(file_to_open);
  if (rewritten_filename.has_value()) {
    file_to_open = rewritten_filename.value().c_str();
  }

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
void BrokerHost::RenameFileForIPC(const char* old_filename,
                                  const char* new_filename,
                                  BrokerSimpleMessage* reply) {
  const char* old_file_to_access = nullptr;
  const char* new_file_to_access = nullptr;
  std::tie(old_file_to_access, new_file_to_access) = CommandRenameIsSafe(
      policy_->allowed_command_set, *policy_->file_permissions, old_filename,
      new_filename);
  if (!old_file_to_access || !new_file_to_access) {
    RAW_CHECK(
        reply->AddIntToMessage(-policy_->file_permissions->denied_errno()));
    return;
  }

  std::optional<std::string> old_rewritten_filename =
      RewritePathname(old_file_to_access);
  if (old_rewritten_filename) {
    old_file_to_access = old_rewritten_filename.value().c_str();
  }

  std::optional<std::string> new_rewritten_filename =
      RewritePathname(new_file_to_access);
  if (new_rewritten_filename) {
    new_file_to_access = new_rewritten_filename.value().c_str();
  }

  if (rename(old_file_to_access, new_file_to_access) < 0) {
    RAW_CHECK(reply->AddIntToMessage(-errno));
    return;
  }
  RAW_CHECK(reply->AddIntToMessage(0));
}

// Perform readlink(2) on |filename| using a buffer of MAX_PATH bytes.
void BrokerHost::ReadlinkFileForIPC(const char* requested_filename,
                                    BrokerSimpleMessage* reply) {
  const char* file_to_access =
      CommandReadlinkIsSafe(policy_->allowed_command_set,
                            *policy_->file_permissions, requested_filename);
  if (!file_to_access) {
    RAW_CHECK(
        reply->AddIntToMessage(-policy_->file_permissions->denied_errno()));
    return;
  }

  std::optional<std::string> rewritten_filename =
      RewritePathname(file_to_access);
  if (rewritten_filename.has_value()) {
    file_to_access = rewritten_filename.value().c_str();
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

void BrokerHost::RmdirFileForIPC(const char* requested_filename,
                                 BrokerSimpleMessage* reply) {
  const char* file_to_access =
      CommandRmdirIsSafe(policy_->allowed_command_set,
                         *policy_->file_permissions, requested_filename);
  if (!file_to_access) {
    RAW_CHECK(
        reply->AddIntToMessage(-policy_->file_permissions->denied_errno()));
    return;
  }

  std::optional<std::string> rewritten_filename =
      RewritePathname(file_to_access);
  if (rewritten_filename.has_value()) {
    file_to_access = rewritten_filename.value().c_str();
  }

  if (rmdir(file_to_access) < 0) {
    RAW_CHECK(reply->AddIntToMessage(-errno));
    return;
  }
  RAW_CHECK(reply->AddIntToMessage(0));
}

// Perform stat(2) on |requested_filename| and write the result to
// |return_val|.
void BrokerHost::StatFileForIPC(BrokerCommand command_type,

                                const char* requested_filename,
                                bool follow_links,
                                BrokerSimpleMessage* reply) {
  const char* file_to_access =
      CommandStatIsSafe(policy_->allowed_command_set,
                        *policy_->file_permissions, requested_filename);
  if (!file_to_access) {
    RAW_CHECK(
        reply->AddIntToMessage(-policy_->file_permissions->denied_errno()));
    return;
  }

  std::optional<std::string> rewritten_filename =
      RewritePathname(file_to_access);
  if (rewritten_filename.has_value()) {
    file_to_access = rewritten_filename.value().c_str();
  }

  if (command_type == COMMAND_STAT) {
    struct kernel_stat sb;

    int sts = follow_links ? sandbox::sys_stat(file_to_access, &sb)
                           : sandbox::sys_lstat(file_to_access, &sb);
    if (sts < 0) {
      RAW_CHECK(reply->AddIntToMessage(-errno));
      return;
    }
    RAW_CHECK(reply->AddIntToMessage(0));
    RAW_CHECK(
        reply->AddDataToMessage(reinterpret_cast<char*>(&sb), sizeof(sb)));
  } else {
#if defined(__NR_fstatat64)
    DCHECK(command_type == COMMAND_STAT64);
    struct kernel_stat64 sb;

    int sts = sandbox::sys_fstatat64(AT_FDCWD, file_to_access, &sb,
                                     follow_links ? 0 : AT_SYMLINK_NOFOLLOW);
    if (sts < 0) {
      RAW_CHECK(reply->AddIntToMessage(-errno));
      return;
    }
    RAW_CHECK(reply->AddIntToMessage(0));
    RAW_CHECK(
        reply->AddDataToMessage(reinterpret_cast<char*>(&sb), sizeof(sb)));
#else  // defined(__NR_fstatat64)
    // We should not reach here on 64-bit systems, as the *stat*64() are only
    // necessary on 32-bit.
    RAW_CHECK(false);
#endif
  }
}

void BrokerHost::UnlinkFileForIPC(const char* requested_filename,
                                  BrokerSimpleMessage* reply) {
  const char* file_to_access =
      CommandUnlinkIsSafe(policy_->allowed_command_set,
                          *policy_->file_permissions, requested_filename);
  if (!file_to_access) {
    RAW_CHECK(
        reply->AddIntToMessage(-policy_->file_permissions->denied_errno()));
    return;
  }

  std::optional<std::string> rewritten_filename =
      RewritePathname(file_to_access);
  if (rewritten_filename.has_value()) {
    file_to_access = rewritten_filename.value().c_str();
  }

  if (unlink(file_to_access) < 0) {
    RAW_CHECK(reply->AddIntToMessage(-errno));
    return;
  }
  RAW_CHECK(reply->AddIntToMessage(0));
}

void BrokerHost::InotifyAddWatchForIPC(base::ScopedFD inotify_fd,
                                       const char* requested_filename,
                                       uint32_t mask,
                                       BrokerSimpleMessage* message) {
  const char* file_to_access = CommandInotifyAddWatchIsSafe(
      policy_->allowed_command_set, *policy_->file_permissions,
      requested_filename, mask);
  if (!file_to_access) {
    RAW_CHECK(
        message->AddIntToMessage(-policy_->file_permissions->denied_errno()));
    return;
  }

  std::optional<std::string> rewritten_filename =
      RewritePathname(file_to_access);
  if (rewritten_filename.has_value()) {
    file_to_access = rewritten_filename.value().c_str();
  }

  int wd = inotify_add_watch(inotify_fd.get(), file_to_access, mask);
  if (wd < 0) {
    RAW_CHECK(message->AddIntToMessage(-errno));
    return;
  }
  RAW_CHECK(message->AddIntToMessage(wd));
}

// Handle a |command_type| request contained in |iter| and write the reply
// to |reply|.
bool BrokerHost::HandleRemoteCommand(BrokerSimpleMessage* message,
                                     base::span<base::ScopedFD> recv_fds,
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
      auto result = GetPathAndFlags(message);
      if (!result) {
        return false;
      }
      std::tie(requested_filename, flags) = *result;
      AccessFileForIPC(requested_filename, flags, reply);
      break;
    }
    case COMMAND_MKDIR: {
      const char* requested_filename;
      int mode = 0;
      auto result = GetPathAndFlags(message);
      if (!result) {
        return false;
      }
      std::tie(requested_filename, mode) = *result;
      MkdirFileForIPC(requested_filename, mode, reply);
      break;
    }
    case COMMAND_OPEN: {
      const char* requested_filename;
      int flags = 0;
      auto result = GetPathAndFlags(message);
      if (!result) {
        return false;
      }
      std::tie(requested_filename, flags) = *result;
      OpenFileForIPC(requested_filename, flags, reply, opened_file);
      break;
    }
    case COMMAND_READLINK: {
      const char* filename;
      if (!message->ReadString(&filename)) {
        return false;
      }

      ReadlinkFileForIPC(filename, reply);
      break;
    }
    case COMMAND_RENAME: {
      const char* old_filename;
      if (!message->ReadString(&old_filename)) {
        return false;
      }

      const char* new_filename;
      if (!message->ReadString(&new_filename)) {
        return false;
      }

      RenameFileForIPC(old_filename, new_filename, reply);
      break;
    }
    case COMMAND_RMDIR: {
      const char* requested_filename;
      if (!message->ReadString(&requested_filename)) {
        return false;
      }
      RmdirFileForIPC(requested_filename, reply);
      break;
    }
    case COMMAND_STAT:
    case COMMAND_STAT64: {
      const char* requested_filename;
      if (!message->ReadString(&requested_filename)) {
        return false;
      }
      int follow_links;
      if (!message->ReadInt(&follow_links)) {
        return false;
      }
      StatFileForIPC(static_cast<BrokerCommand>(command_type),
                     requested_filename, !!follow_links, reply);
      break;
    }
    case COMMAND_UNLINK: {
      const char* requested_filename;
      if (!message->ReadString(&requested_filename)) {
        return false;
      }
      UnlinkFileForIPC(requested_filename, reply);
      break;
    }
    case COMMAND_INOTIFY_ADD_WATCH: {
      const char* requested_filename;
      if (!message->ReadString(&requested_filename)) {
        return false;
      }
      int mask;
      if (!message->ReadInt(&mask)) {
        return false;
      }
      if (!recv_fds[0].is_valid()) {
        return false;
      }
      InotifyAddWatchForIPC(std::move(recv_fds[0]), requested_filename, mask,
                            reply);
      break;
    }
    default:
      LOG(ERROR) << "Invalid IPC command";
      return false;
  }
  return true;
}

BrokerHost::BrokerHost(const BrokerSandboxConfig& policy,
                       BrokerChannel::EndPoint ipc_channel,
                       pid_t sandboxed_process_pid)
    : policy_(policy),
      ipc_channel_(std::move(ipc_channel)),
      sandboxed_process_pid_(sandboxed_process_pid) {}

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
    std::array<base::ScopedFD, 2> recv_fds_arr;
    base::span<base::ScopedFD> recv_fds(recv_fds_arr);
    const ssize_t msg_len =
        message.RecvMsgWithFlagsMultipleFds(ipc_channel_.get(), 0, recv_fds);

    if (msg_len == 0 || (msg_len == -1 && errno == ECONNRESET)) {
      // EOF from the client, or the client died, we should finish looping.
      return;
    }

    // This indicates an error occurred in IPC. For example, too many fds were
    // sent along with the message.
    if (msg_len < 0 || !recv_fds[0].is_valid()) {
      if (!recv_fds[0].is_valid()) {
        errno = EBADF;
      }
      PLOG(ERROR) << "Error reading message from the client";
      continue;
    }

    temporary_ipc = std::move(recv_fds[0]);

    BrokerSimpleMessage reply;
    base::ScopedFD opened_file;
    if (!HandleRemoteCommand(&message, recv_fds.subspan(1), &reply,
                             &opened_file)) {
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
