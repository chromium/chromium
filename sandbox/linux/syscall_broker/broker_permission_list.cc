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

namespace sandbox {
namespace syscall_broker {

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

const char* BrokerPermissionList::GetFileNameIfAllowedToAccess(
    const char* requested_filename,
    int requested_mode) const {
  for (size_t i = 0; i < num_of_permissions_; i++) {
    const char* ret =
        permissions_array_[i].CheckAccess(requested_filename, requested_mode);
    if (ret) {
      return ret;
    }
  }
  return nullptr;
}

std::pair<const char*, bool> BrokerPermissionList::GetFileNameIfAllowedToOpen(
    const char* requested_filename,
    int requested_flags) const {
  for (size_t i = 0; i < num_of_permissions_; i++) {
    std::pair<const char*, bool> ret =
        permissions_array_[i].CheckOpen(requested_filename, requested_flags);
    if (ret.first) {
      return ret;
    }
  }
  return {nullptr, false};
}

const char* BrokerPermissionList::GetFileNameIfAllowedToStat(
    const char* requested_filename) const {
  for (size_t i = 0; i < num_of_permissions_; i++) {
    const char* ret =
        permissions_array_[i].CheckStatWithIntermediates(requested_filename);
    if (ret) {
      return ret;
    }
  }
  return nullptr;
}

const char* BrokerPermissionList::GetFileNameIfAllowedToInotifyAddWatch(
    const char* requested_filename,
    uint32_t mask) const {
  for (size_t i = 0; i < num_of_permissions_; i++) {
    const char* ret =
        permissions_array_[i].CheckInotifyAddWatchWithIntermediates(
            requested_filename, mask);
    if (ret) {
      return ret;
    }
  }
  return nullptr;
}

}  // namespace syscall_broker
}  // namespace sandbox
