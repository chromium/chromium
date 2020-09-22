// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SYSCALL_BROKER_BROKER_HOST_H_
#define SANDBOX_LINUX_SYSCALL_BROKER_BROKER_HOST_H_

#include "base/macros.h"
#include "sandbox/linux/syscall_broker/broker_channel.h"
#include "sandbox/linux/syscall_broker/broker_command.h"

namespace sandbox {

namespace syscall_broker {

class BrokerPermissionList;

// The BrokerHost class should be embedded in a (presumably not sandboxed)
// process. It will honor IPC requests from a BrokerClient sent over
// |ipc_channel| according to |broker_permission_list|.
class BrokerHost {
 public:
  BrokerHost(const BrokerPermissionList& broker_permission_list,
             const BrokerCommandSet& allowed_command_set,
             BrokerChannel::EndPoint ipc_channel);
  ~BrokerHost();

  // Receive system call requests and handle them forevermore.
  void LoopAndHandleRequests();

 private:
  const BrokerPermissionList& broker_permission_list_;
  const BrokerCommandSet allowed_command_set_;
  const BrokerChannel::EndPoint ipc_channel_;

  DISALLOW_COPY_AND_ASSIGN(BrokerHost);
};

}  // namespace syscall_broker

}  // namespace sandbox

#endif  //  SANDBOX_LINUX_SYSCALL_BROKER_BROKER_HOST_H_
