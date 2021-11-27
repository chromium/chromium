// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_SOCKET_DISPATCHER_H_
#define SANDBOX_WIN_SRC_SOCKET_DISPATCHER_H_

#include <stdint.h>
#include <winsock2.h>

#include "base/memory/raw_ptr.h"
#include "sandbox/win/src/crosscall_client.h"
#include "sandbox/win/src/crosscall_server.h"
#include "sandbox/win/src/interception.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/sandbox_policy_base.h"

namespace sandbox {

// This class handles socket related IPC calls.
class SocketDispatcher : public Dispatcher {
 public:
  explicit SocketDispatcher(PolicyBase* policy_base);
  ~SocketDispatcher() override = default;

  SocketDispatcher(const SocketDispatcher&) = delete;
  SocketDispatcher& operator=(const SocketDispatcher&) = delete;

  // Dispatcher interface.
  bool SetupService(InterceptionManager* manager, IpcTag service) override;

 private:
  // Processes IPC requests coming from calls to WS2_32!WSA_Socket in the
  // target.
  bool WS2Socket(IPCInfo* ipc,
                 uint32_t af,
                 uint32_t type,
                 uint32_t protocol,
                 InOutCountedBuffer* buffer);
  raw_ptr<PolicyBase> policy_base_;
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_SOCKET_DISPATCHER_H_
