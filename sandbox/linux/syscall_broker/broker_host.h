// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SYSCALL_BROKER_BROKER_HOST_H_
#define SANDBOX_LINUX_SYSCALL_BROKER_BROKER_HOST_H_

#include "base/memory/raw_ref.h"
#include "sandbox/linux/syscall_broker/broker_channel.h"
#include "sandbox/linux/syscall_broker/broker_command.h"
#include "sandbox/linux/syscall_broker/broker_sandbox_config.h"

namespace sandbox {

namespace syscall_broker {

// The BrokerHost class should be embedded in a (presumably not sandboxed)
// process. It will honor IPC requests from a BrokerClient sent over
// |ipc_channel| according to |broker_permission_list|.
class BrokerHost {
 public:
  BrokerHost(const BrokerSandboxConfig& policy,
             BrokerChannel::EndPoint ipc_channel);

  BrokerHost(const BrokerHost&) = delete;
  BrokerHost& operator=(const BrokerHost&) = delete;

  ~BrokerHost();

  // Receive system call requests and handle them forevermore.
  void LoopAndHandleRequests();

 private:
  const raw_ref<const BrokerSandboxConfig> policy_;
  const BrokerChannel::EndPoint ipc_channel_;
};

}  // namespace syscall_broker

}  // namespace sandbox

#endif  //  SANDBOX_LINUX_SYSCALL_BROKER_BROKER_HOST_H_
