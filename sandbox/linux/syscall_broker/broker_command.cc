// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unistd.h>

#include "sandbox/linux/syscall_broker/broker_command.h"
#include "sandbox/linux/syscall_broker/broker_permission_list.h"

namespace sandbox {
namespace syscall_broker {

bool CommandAccessIsSafe(const BrokerCommandSet& command_set,
                         const BrokerPermissionList& policy,
                         const char* requested_filename,
                         int requested_mode,
                         const char** filename_to_use) {
  return command_set.test(COMMAND_ACCESS) &&
         policy.GetFileNameIfAllowedToAccess(requested_filename, requested_mode,
                                             filename_to_use);
}

bool CommandMkdirIsSafe(const BrokerCommandSet& command_set,
                        const BrokerPermissionList& policy,
                        const char* requested_filename,
                        const char** filename_to_use) {
  return command_set.test(COMMAND_MKDIR) &&
         policy.GetFileNameIfAllowedToOpen(requested_filename,
                                           O_RDWR | O_CREAT | O_EXCL,
                                           filename_to_use, nullptr);
}

bool CommandOpenIsSafe(const BrokerCommandSet& command_set,
                       const BrokerPermissionList& policy,
                       const char* requested_filename,
                       int requested_flags,
                       const char** filename_to_use,
                       bool* unlink_after_open) {
  return command_set.test(COMMAND_OPEN) &&
         policy.GetFileNameIfAllowedToOpen(
             requested_filename,
             requested_flags & ~kCurrentProcessOpenFlagsMask, filename_to_use,
             unlink_after_open);
}

bool CommandReadlinkIsSafe(const BrokerCommandSet& command_set,
                           const BrokerPermissionList& policy,
                           const char* requested_filename,
                           const char** filename_to_use) {
  return command_set.test(COMMAND_READLINK) &&
         policy.GetFileNameIfAllowedToOpen(requested_filename, O_RDONLY,
                                           filename_to_use, nullptr);
}

bool CommandRenameIsSafe(const BrokerCommandSet& command_set,
                         const BrokerPermissionList& policy,
                         const char* old_filename,
                         const char* new_filename,
                         const char** old_filename_to_use,
                         const char** new_filename_to_use) {
  return command_set.test(COMMAND_RENAME) &&
         policy.GetFileNameIfAllowedToOpen(old_filename,
                                           O_RDWR | O_CREAT | O_EXCL,
                                           old_filename_to_use, nullptr) &&
         policy.GetFileNameIfAllowedToOpen(new_filename,
                                           O_RDWR | O_CREAT | O_EXCL,
                                           new_filename_to_use, nullptr);
}

bool CommandRmdirIsSafe(const BrokerCommandSet& command_set,
                        const BrokerPermissionList& policy,
                        const char* requested_filename,
                        const char** filename_to_use) {
  return command_set.test(COMMAND_RMDIR) &&
         policy.GetFileNameIfAllowedToOpen(requested_filename,
                                           O_RDWR | O_CREAT | O_EXCL,
                                           filename_to_use, nullptr);
}

bool CommandStatIsSafe(const BrokerCommandSet& command_set,
                       const BrokerPermissionList& policy,
                       const char* requested_filename,
                       const char** filename_to_use) {
  return command_set.test(COMMAND_STAT) &&
         policy.GetFileNameIfAllowedToStat(requested_filename, filename_to_use);
}

bool CommandUnlinkIsSafe(const BrokerCommandSet& command_set,
                         const BrokerPermissionList& policy,
                         const char* requested_filename,
                         const char** filename_to_use) {
  return command_set.test(COMMAND_UNLINK) &&
         policy.GetFileNameIfAllowedToOpen(requested_filename,
                                           O_RDWR | O_CREAT | O_EXCL,
                                           filename_to_use, nullptr);
}

bool CommandInotifyAddWatchIsSafe(const BrokerCommandSet& command_set,
                                  const BrokerPermissionList& policy,
                                  const char* requested_filename,
                                  uint32_t mask,
                                  const char** filename_to_use) {
  return command_set.test(COMMAND_INOTIFY_ADD_WATCH) &&
         policy.GetFileNameIfAllowedToInotifyAddWatch(requested_filename, mask,
                                                      filename_to_use);
}

}  // namespace syscall_broker
}  // namespace sandbox
