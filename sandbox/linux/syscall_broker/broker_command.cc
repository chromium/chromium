// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unistd.h>

#include "sandbox/linux/syscall_broker/broker_command.h"
#include "sandbox/linux/syscall_broker/broker_permission_list.h"

namespace sandbox {
namespace syscall_broker {

const char* CommandAccessIsSafe(const BrokerCommandSet& command_set,
                                const BrokerPermissionList& policy,
                                const char* requested_filename,
                                int requested_mode) {
  if (!command_set.test(COMMAND_ACCESS)) {
    return nullptr;
  }

  return policy.GetFileNameIfAllowedToAccess(requested_filename,
                                             requested_mode);
}

const char* CommandMkdirIsSafe(const BrokerCommandSet& command_set,
                               const BrokerPermissionList& policy,
                               const char* requested_filename) {
  if (!command_set.test(COMMAND_MKDIR)) {
    return nullptr;
  }

  return policy
      .GetFileNameIfAllowedToOpen(requested_filename, O_RDWR | O_CREAT | O_EXCL)
      .first;
}

std::pair<const char*, bool> CommandOpenIsSafe(
    const BrokerCommandSet& command_set,
    const BrokerPermissionList& policy,
    const char* requested_filename,
    int requested_flags) {
  if (!command_set.test(COMMAND_OPEN)) {
    return {nullptr, false};
  }

  return policy.GetFileNameIfAllowedToOpen(
      requested_filename, requested_flags & ~kCurrentProcessOpenFlagsMask);
}

const char* CommandReadlinkIsSafe(const BrokerCommandSet& command_set,
                                  const BrokerPermissionList& policy,
                                  const char* requested_filename) {
  if (!command_set.test(COMMAND_READLINK)) {
    return nullptr;
  }

  return policy.GetFileNameIfAllowedToOpen(requested_filename, O_RDONLY).first;
}

std::pair<const char*, const char*> CommandRenameIsSafe(
    const BrokerCommandSet& command_set,
    const BrokerPermissionList& policy,
    const char* old_filename,
    const char* new_filename) {
  if (!command_set.test(COMMAND_RENAME)) {
    return {nullptr, nullptr};
  }

  auto old_allowed = policy.GetFileNameIfAllowedToOpen(
      old_filename, O_RDWR | O_CREAT | O_EXCL);
  auto new_allowed = policy.GetFileNameIfAllowedToOpen(
      new_filename, O_RDWR | O_CREAT | O_EXCL);
  if (!old_allowed.first || !new_allowed.first) {
    return {nullptr, nullptr};
  }

  return {old_allowed.first, new_allowed.first};
}

const char* CommandRmdirIsSafe(const BrokerCommandSet& command_set,
                               const BrokerPermissionList& policy,
                               const char* requested_filename) {
  if (!command_set.test(COMMAND_RMDIR)) {
    return nullptr;
  }

  return policy
      .GetFileNameIfAllowedToOpen(requested_filename, O_RDWR | O_CREAT | O_EXCL)
      .first;
}

const char* CommandStatIsSafe(const BrokerCommandSet& command_set,
                              const BrokerPermissionList& policy,
                              const char* requested_filename) {
  if (!command_set.test(COMMAND_STAT)) {
    return nullptr;
  }

  return policy.GetFileNameIfAllowedToStat(requested_filename);
}

const char* CommandUnlinkIsSafe(const BrokerCommandSet& command_set,
                                const BrokerPermissionList& policy,
                                const char* requested_filename) {
  if (!command_set.test(COMMAND_UNLINK)) {
    return nullptr;
  }

  return policy
      .GetFileNameIfAllowedToOpen(requested_filename, O_RDWR | O_CREAT | O_EXCL)
      .first;
}

const char* CommandInotifyAddWatchIsSafe(const BrokerCommandSet& command_set,
                                         const BrokerPermissionList& policy,
                                         const char* requested_filename,
                                         uint32_t mask) {
  if (!command_set.test(COMMAND_INOTIFY_ADD_WATCH)) {
    return nullptr;
  }

  return policy.GetFileNameIfAllowedToInotifyAddWatch(requested_filename, mask);
}

}  // namespace syscall_broker
}  // namespace sandbox
