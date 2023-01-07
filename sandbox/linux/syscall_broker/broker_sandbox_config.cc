// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/syscall_broker/broker_sandbox_config.h"

#include "sandbox/linux/syscall_broker/broker_file_permission.h"
#include "sandbox/linux/syscall_broker/broker_permission_list.h"

namespace sandbox {
namespace syscall_broker {

BrokerSandboxConfig::BrokerSandboxConfig(
    BrokerCommandSet allowed_command_set,
    std::vector<BrokerFilePermission> perms,
    int denied_errno)
    : allowed_command_set(allowed_command_set),
      file_permissions(
          std::make_unique<BrokerPermissionList>(denied_errno,
                                                 std::move(perms))) {}

BrokerSandboxConfig::BrokerSandboxConfig(BrokerSandboxConfig&&) = default;
BrokerSandboxConfig& BrokerSandboxConfig::operator=(BrokerSandboxConfig&&) =
    default;

BrokerSandboxConfig::~BrokerSandboxConfig() = default;

}  // namespace syscall_broker
}  // namespace sandbox
