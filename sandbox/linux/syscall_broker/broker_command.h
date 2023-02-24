// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SYSCALL_BROKER_BROKER_COMMAND_H_
#define SANDBOX_LINUX_SYSCALL_BROKER_BROKER_COMMAND_H_

#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>

#include <bitset>
#include <initializer_list>
#include <utility>

namespace sandbox {
namespace syscall_broker {

class BrokerPermissionList;

// Some flags are local to the current process and cannot be sent over a Unix
// socket. They need special treatment from the client.
// O_CLOEXEC is tricky because in theory another thread could call execve()
// before special treatment is made on the client, so a client needs to call
// recvmsg(2) with MSG_CMSG_CLOEXEC.
// To make things worse, there are two CLOEXEC related flags, FD_CLOEXEC (see
// F_GETFD in fcntl(2)) and O_CLOEXEC (see F_GETFL in fcntl(2)). O_CLOEXEC
// doesn't affect the semantics on execve(), it's merely a note that the
// descriptor was originally opened with O_CLOEXEC as a flag. And it is sent
// over unix sockets just fine, so a receiver that would (incorrectly) look at
// O_CLOEXEC instead of FD_CLOEXEC may be tricked in thinking that the file
// descriptor will or won't be closed on execve().
constexpr int kCurrentProcessOpenFlagsMask = O_CLOEXEC;

enum BrokerCommand {
  COMMAND_INVALID = 0,
  COMMAND_ACCESS,
  COMMAND_MKDIR,
  COMMAND_OPEN,
  COMMAND_READLINK,
  COMMAND_RENAME,
  COMMAND_RMDIR,
  COMMAND_STAT,
  COMMAND_STAT64,
  COMMAND_UNLINK,
  COMMAND_INOTIFY_ADD_WATCH,

  // NOTE: update when adding new commands.
  COMMAND_MAX = COMMAND_INOTIFY_ADD_WATCH
};

using BrokerCommandSet = std::bitset<COMMAND_MAX + 1>;

// Helper function since std::bitset lacks an initializer list constructor.
inline BrokerCommandSet MakeBrokerCommandSet(
    const std::initializer_list<BrokerCommand>& args) {
  BrokerCommandSet result;
  for (const auto& arg : args)
    result.set(arg);
  return result;
}

// Helper functions to perform the same permissions test on either side
// (client or broker process) of a broker IPC command. The implementations
// must be safe when called from an async signal handler.
// They all return nullptr when permission checks fail.
[[nodiscard]] const char* CommandAccessIsSafe(
    const BrokerCommandSet& command_set,
    const BrokerPermissionList& policy,
    const char* requested_filename,
    int requested_mode  // e.g. F_OK, R_OK, W_OK.
);

[[nodiscard]] const char* CommandMkdirIsSafe(
    const BrokerCommandSet& command_set,
    const BrokerPermissionList& policy,
    const char* requested_filename);

[[nodiscard]] std::pair<const char*, bool> CommandOpenIsSafe(
    const BrokerCommandSet& command_set,
    const BrokerPermissionList& policy,
    const char* requested_filename,
    int requested_flags  // e.g. O_RDONLY, O_RDWR.
);

[[nodiscard]] const char* CommandReadlinkIsSafe(
    const BrokerCommandSet& command_set,
    const BrokerPermissionList& policy,
    const char* requested_filename);

[[nodiscard]] std::pair<const char*, const char*> CommandRenameIsSafe(
    const BrokerCommandSet& command_set,
    const BrokerPermissionList& policy,
    const char* old_filename,
    const char* new_filename);

[[nodiscard]] const char* CommandRmdirIsSafe(
    const BrokerCommandSet& command_set,
    const BrokerPermissionList& policy,
    const char* requested_filename);

[[nodiscard]] const char* CommandStatIsSafe(const BrokerCommandSet& command_set,
                                            const BrokerPermissionList& policy,
                                            const char* requested_filename);

[[nodiscard]] const char* CommandUnlinkIsSafe(
    const BrokerCommandSet& command_set,
    const BrokerPermissionList& policy,
    const char* requested_filename);

[[nodiscard]] const char* CommandInotifyAddWatchIsSafe(
    const BrokerCommandSet& command_set,
    const BrokerPermissionList& policy,
    const char* requested_filename,
    uint32_t mask);

}  // namespace syscall_broker
}  // namespace sandbox

#endif  // SANDBOX_LINUX_SYSCALL_BROKER_BROKER_COMMAND_H_
