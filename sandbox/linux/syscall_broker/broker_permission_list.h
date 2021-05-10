// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SYSCALL_BROKER_BROKER_PERMISSION_LIST_H_
#define SANDBOX_LINUX_SYSCALL_BROKER_BROKER_PERMISSION_LIST_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/macros.h"

#include "sandbox/linux/syscall_broker/broker_file_permission.h"

namespace sandbox {
namespace syscall_broker {

// BrokerPermissionList defines the file access control list enforced by
// a BrokerHost. The BrokerHost will evaluate requests to manipulate files
// sent over its IPC channel according to the BrokerPermissionList.
// The methods of this class must be usable in an async-signal safe manner.
class BrokerPermissionList {
 public:
  // |denied_errno| is the error code returned when IPC requests for system
  // calls such as open() or access() are denied because a file is not in the
  // allowlist. EACCESS would be a typical value.
  // |permissions| is a list of BrokerPermission objects that define
  // what the broker will allow.
  BrokerPermissionList(int denied_errno,
                       const std::vector<BrokerFilePermission>& permissions);

  ~BrokerPermissionList();

  // Check if calling access() should be allowed on |requested_filename| with
  // mode |requested_mode|.
  // Note: access() being a system call to check permissions, this can get a bit
  // confusing. We're checking if calling access() should even be allowed with
  // If |file_to_open| is not NULL, a pointer to the path will be returned.
  // In the case of a recursive match, this will be the requested_filename,
  // otherwise it will return the matching pointer from the
  // allowlist. For paranoia a caller should then use |file_to_access|. See
  // GetFileNameIfAllowedToOpen() for more explanation.
  // return true if calling access() on this file should be allowed, false
  // otherwise.
  // Async signal safe if and only if |file_to_access| is NULL.
  bool GetFileNameIfAllowedToAccess(const char* requested_filename,
                                    int requested_mode,
                                    const char** file_to_access) const;

  // Check if |requested_filename| can be opened with flags |requested_flags|.
  // If |file_to_open| is not NULL, a pointer to the path will be returned.
  // In the case of a recursive match, this will be the requested_filename,
  // otherwise it will return the matching pointer from the
  // allowlist. For paranoia, a caller should then use |file_to_open| rather
  // than |requested_filename|, so that it never attempts to open an
  // attacker-controlled file name, even if an attacker managed to fool the
  // string comparison mechanism.
  // |unlink_after_open| if not NULL will be set to point to true if the
  // policy requests the caller unlink the path after opening.
  // Return true if opening should be allowed, false otherwise.
  // Async signal safe if and only if |file_to_open| is NULL.
  bool GetFileNameIfAllowedToOpen(const char* requested_filename,
                                  int requested_flags,
                                  const char** file_to_open,
                                  bool* unlink_after_open) const;

  // Check if calling stat() should be allowed on |requested_filename|.
  // Async signal safe if and only if |file_to_open| is NULL. This is
  // similar to GetFileNameIfAllowedToAccess(), except that if we have
  // create permission on file, we permit stat() on all its leading
  // components, otherwise checking for missing intermediate directories
  // can't happen proplery during a base::CreateDirectory() call.
  // Async signal safe if and only if |file_to_open| is NULL.
  bool GetFileNameIfAllowedToStat(const char* requested_filename,
                                  const char** file_to_access) const;

  int denied_errno() const { return denied_errno_; }

 private:
  const int denied_errno_;
  // The permissions_ vector is used as storage for the BrokerFilePermission
  // objects but is not referenced outside of the constructor as
  // vectors are unfriendly in async signal safe code.
  const std::vector<BrokerFilePermission> permissions_;
  // permissions_array_ is set up to point to the backing store of
  // permissions_ and is used in async signal safe methods.
  const BrokerFilePermission* permissions_array_;
  const size_t num_of_permissions_;

  DISALLOW_COPY_AND_ASSIGN(BrokerPermissionList);
};

}  // namespace syscall_broker
}  // namespace sandbox

#endif  // SANDBOX_LINUX_SYSCALL_BROKER_BROKER_PERMISSION_LIST_H_
