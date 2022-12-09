// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/syscall_broker/broker_permission_list.h"

#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <string>
#include <vector>

#include "base/check_op.h"
#include "base/logging.h"
#include "sandbox/linux/syscall_broker/broker_command.h"

namespace sandbox {
namespace syscall_broker {

namespace {

bool CheckCallerArgs(const char** file_to_access) {
  if (file_to_access && *file_to_access) {
    // Make sure that callers never pass a non-empty string. In case callers
    // wrongly forget to check the return value and look at the string
    // instead, this could catch bugs.
    RAW_LOG(FATAL, "*file_to_access should be nullptr");
    return false;
  }
  return true;
}

}  // namespace

BrokerPermissionList::BrokerPermissionList(
    int denied_errno,
    std::vector<BrokerFilePermission> permissions)
    : denied_errno_(denied_errno),
      permissions_(std::move(permissions)),
      num_of_permissions_(permissions_.size()) {
  // The spec guarantees vectors store their elements contiguously
  // so set up a pointer to array of element so it can be used
  // in async signal safe code instead of vector operations.
  if (num_of_permissions_ > 0) {
    permissions_array_ = &permissions_[0];
  } else {
    permissions_array_ = nullptr;
  }
}

BrokerPermissionList::~BrokerPermissionList() = default;

// Check if calling access() should be allowed on |requested_filename| with
// mode |requested_mode|.
// Note: access() being a system call to check permissions, this can get a bit
// confusing. We're checking if calling access() should even be allowed with
// the same policy we would use for open().
// If |file_to_access| is not nullptr, we will return the matching pointer from
// the allowlist. For paranoia a caller should then use |file_to_access|. See
// GetFileNameIfAllowedToOpen() for more explanation.
// return true if calling access() on this file should be allowed, false
// otherwise.
// Async signal safe if and only if |file_to_access| is nullptr.
bool BrokerPermissionList::GetFileNameIfAllowedToAccess(
    const char* requested_filename,
    int requested_mode,
    const char** file_to_access) const {
  if (!CheckCallerArgs(file_to_access))
    return false;

  for (size_t i = 0; i < num_of_permissions_; i++) {
    if (permissions_array_[i].CheckAccess(requested_filename, requested_mode,
                                          file_to_access)) {
      return true;
    }
  }
  return false;
}

// Check if |requested_filename| can be opened with flags |requested_flags|.
// If |file_to_open| is not nullptr, we will return the matching pointer from
// the allowlist. For paranoia, a caller should then use |file_to_open| rather
// than |requested_filename|, so that it never attempts to open an
// attacker-controlled file name, even if an attacker managed to fool the
// string comparison mechanism.
// Return true if opening should be allowed, false otherwise.
// Async signal safe if and only if |file_to_open| is nullptr.
bool BrokerPermissionList::GetFileNameIfAllowedToOpen(
    const char* requested_filename,
    int requested_flags,
    const char** file_to_open,
    bool* unlink_after_open) const {
  if (!CheckCallerArgs(file_to_open))
    return false;

  for (size_t i = 0; i < num_of_permissions_; i++) {
    if (permissions_array_[i].CheckOpen(requested_filename, requested_flags,
                                        file_to_open, unlink_after_open)) {
      return true;
    }
  }
  return false;
}

bool BrokerPermissionList::GetFileNameIfAllowedToStat(
    const char* requested_filename,
    const char** file_to_stat) const {
  if (!CheckCallerArgs(file_to_stat))
    return false;

  for (size_t i = 0; i < num_of_permissions_; i++) {
    if (permissions_array_[i].CheckStatWithIntermediates(requested_filename,
                                                         file_to_stat))
      return true;
  }
  return false;
}

bool BrokerPermissionList::GetFileNameIfAllowedToInotifyAddWatch(
    const char* requested_filename,
    uint32_t mask,
    const char** file_to_inotify_add_watch) const {
  if (!CheckCallerArgs(file_to_inotify_add_watch))
    return false;

  for (size_t i = 0; i < num_of_permissions_; i++) {
    if (permissions_array_[i].CheckInotifyAddWatchWithIntermediates(
            requested_filename, mask, file_to_inotify_add_watch))
      return true;
  }
  return false;
}

}  // namespace syscall_broker
}  // namespace sandbox
