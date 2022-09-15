// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_NETWORK_SANDBOX_HOOK_LINUX_H_
#define SERVICES_NETWORK_NETWORK_SANDBOX_HOOK_LINUX_H_

#include <vector>

#include "base/component_export.h"
#include "sandbox/linux/syscall_broker/broker_command.h"
#include "sandbox/linux/syscall_broker/broker_file_permission.h"
#include "sandbox/policy/linux/sandbox_linux.h"

namespace network {

COMPONENT_EXPORT(NETWORK_SERVICE)
sandbox::syscall_broker::BrokerCommandSet GetNetworkBrokerCommandSet();

COMPONENT_EXPORT(NETWORK_SERVICE)
std::vector<sandbox::syscall_broker::BrokerFilePermission>
GetNetworkFilePermissions();

COMPONENT_EXPORT(NETWORK_SERVICE)
bool NetworkPreSandboxHook(sandbox::policy::SandboxLinux::Options options);

}  // namespace network

#endif  // SERVICES_NETWORK_NETWORK_SANDBOX_HOOK_LINUX_H_
