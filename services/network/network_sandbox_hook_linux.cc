// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/network_sandbox_hook_linux.h"
#include "sandbox/linux/syscall_broker/broker_command.h"

#include "base/rand_util.h"
#include "base/sys_info.h"

using sandbox::syscall_broker::BrokerFilePermission;
using sandbox::syscall_broker::MakeBrokerCommandSet;

namespace network {

bool NetworkPreSandboxHook(service_manager::SandboxLinux::Options options) {
  auto* instance = service_manager::SandboxLinux::GetInstance();

  // TODO(tsepez): remove universal permission under filesytem root.
  instance->StartBrokerProcess(
      MakeBrokerCommandSet({
          sandbox::syscall_broker::COMMAND_ACCESS,
          sandbox::syscall_broker::COMMAND_MKDIR,
          sandbox::syscall_broker::COMMAND_OPEN,
          sandbox::syscall_broker::COMMAND_READLINK,
          sandbox::syscall_broker::COMMAND_RENAME,
          sandbox::syscall_broker::COMMAND_RMDIR,
          sandbox::syscall_broker::COMMAND_STAT,
          sandbox::syscall_broker::COMMAND_UNLINK,
      }),
      {BrokerFilePermission::ReadWriteCreateRecursive("/")},
      service_manager::SandboxLinux::PreSandboxHook(), options);

  instance->EngageNamespaceSandboxIfPossible();
  return true;
}

}  // namespace network
