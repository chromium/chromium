// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SYSCALL_BROKER_BROKER_PERMISSION_LIST_H_
#define SANDBOX_LINUX_SYSCALL_BROKER_BROKER_PERMISSION_LIST_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/files/scoped_file.h"
#include "base/memory/raw_ptr.h"
#include "base/types/pass_key.h"
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
                       std::vector<BrokerFilePermission> permissions);

  BrokerPermissionList(BrokerPermissionList&&) = delete;
  BrokerPermissionList& operator=(BrokerPermissionList&&) = delete;
  BrokerPermissionList(const BrokerPermissionList&) = delete;
  BrokerPermissionList& operator=(const BrokerPermissionList&) = delete;

  ~BrokerPermissionList();

  // Check if calling access() should be allowed on |requested_filename| with
  // mode |requested_mode|.
  //
  // Note: access() being a system call to check permissions, this can get a bit
  // confusing. We're checking if calling access() should even be allowed.
  //
  // See comments on BrokerFilePermission::CheckAccess() for a description of
  // the return value. The caller should ALWAYS use the return value rather than
  // reusing |requested_filename| for subsequent syscalls, so that it never
  // attempts to open an attacker-controlled file name, even if an attacker
  // managed to fool the string comparison mechanism.
  //
  // Async signal safe.
  [[nodiscard]] const char* GetFileNameIfAllowedToAccess(
      const char* requested_filename,
      int requested_mode) const;

  // Check if |requested_filename| can be opened with flags |requested_flags|.
  //
  // See comments on BrokerFilePermission::CheckOpen() for a description of
  // the return value. The caller should ALWAYS use the return value rather than
  // reusing |requested_filename| for subsequent syscalls, so that it never
  // attempts to open an attacker-controlled file name, even if an attacker
  // managed to fool the string comparison mechanism.
  //
  // Async signal safe.
  [[nodiscard]] std::pair<const char*, bool> GetFileNameIfAllowedToOpen(
      const char* requested_filename,
      int requested_flags) const;

  // Check if calling stat() should be allowed on |requested_filename|.
  // This is similar to GetFileNameIfAllowedToAccess(), except that if we have
  // create permission on file, we permit stat() on all its leading
  // components, otherwise checking for missing intermediate directories
  // can't happen properly during a base::CreateDirectory() call.
  //
  // See comments on BrokerFilePermission::CheckStatWithIntermediates() for a
  // description of the return value. The caller should ALWAYS use the return
  // value rather than reusing |requested_filename| for subsequent syscalls, so
  // that it never attempts to open an attacker-controlled file name, even if an
  // attacker managed to fool the string comparison mechanism.
  //
  // Async signal safe.
  [[nodiscard]] const char* GetFileNameIfAllowedToStat(
      const char* requested_filename) const;

  // Check if |requested_filename| can be watched with mask |mask|.
  //
  // See comments on
  // BrokerFilePermission::CheckInotifyAddWatchWithIntermediates() for a
  // description of the return value. The caller should ALWAYS use the return
  // value rather than reusing |requested_filename| for subsequent syscalls, so
  // that it never attempts to open an attacker-controlled file name, even if an
  // attacker managed to fool the string comparison mechanism.
  //
  // Async signal safe.
  [[nodiscard]] const char* GetFileNameIfAllowedToInotifyAddWatch(
      const char* requested_filename,
      uint32_t mask) const;

  int denied_errno() const { return denied_errno_; }

 private:
  friend class BrokerSandboxConfigSerializer;

  const int denied_errno_;

  // The permissions_ vector is used as storage for the BrokerFilePermission
  // objects but is not referenced outside of the constructor as
  // vectors are unfriendly in async signal safe code.
  const std::vector<BrokerFilePermission> permissions_;
  // permissions_array_ is set up to point to the backing store of
  // permissions_ and is used in async signal safe methods.
  raw_ptr<const BrokerFilePermission, AllowPtrArithmetic> permissions_array_;
  const size_t num_of_permissions_;
};

}  // namespace syscall_broker
}  // namespace sandbox

#endif  // SANDBOX_LINUX_SYSCALL_BROKER_BROKER_PERMISSION_LIST_H_
