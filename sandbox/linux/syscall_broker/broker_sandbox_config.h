// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SYSCALL_BROKER_BROKER_SANDBOX_CONFIG_H_
#define SANDBOX_LINUX_SYSCALL_BROKER_BROKER_SANDBOX_CONFIG_H_

#include <memory>
#include <vector>

#include "sandbox/linux/syscall_broker/broker_command.h"
#include "sandbox/linux/syscall_broker/broker_file_permission.h"
#include "sandbox/sandbox_export.h"

namespace sandbox {
namespace syscall_broker {

class BrokerPermissionList;

// A BrokerSandboxConfig encapsulates the policy this broker is meant to
// enforce. |allowed_command_set| is a bitwise-or of BrokerCommand flags from
// broker_command.h that further restrict the syscalls to execute.
// |file_permissions| describes the allowlisted set of files the broker is is
// allowed to access.
struct SANDBOX_EXPORT BrokerSandboxConfig {
  BrokerSandboxConfig(BrokerCommandSet allowed_command_set,
                      std::vector<BrokerFilePermission> perms,
                      int denied_errno);

  // Movable, not copyable
  BrokerSandboxConfig(BrokerSandboxConfig&&);
  BrokerSandboxConfig& operator=(BrokerSandboxConfig&&);
  BrokerSandboxConfig(const BrokerSandboxConfig&) = delete;
  BrokerSandboxConfig& operator=(const BrokerSandboxConfig&) = delete;

  ~BrokerSandboxConfig();

  BrokerCommandSet allowed_command_set;
  std::unique_ptr<BrokerPermissionList> file_permissions;
};

}  // namespace syscall_broker
}  // namespace sandbox

#endif  // SANDBOX_LINUX_SYSCALL_BROKER_BROKER_SANDBOX_CONFIG_H_
