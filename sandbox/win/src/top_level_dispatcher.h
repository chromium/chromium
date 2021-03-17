// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_TOP_LEVEL_DISPATCHER_H__
#define SANDBOX_SRC_TOP_LEVEL_DISPATCHER_H__

#include <memory>

#include "base/macros.h"
#include "sandbox/win/src/crosscall_server.h"
#include "sandbox/win/src/interception.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/sandbox_policy_base.h"

namespace sandbox {

// Top level dispatcher which hands requests to the appropriate service
// dispatchers.
class TopLevelDispatcher : public Dispatcher {
 public:
  // |policy| must outlive this class.
  explicit TopLevelDispatcher(PolicyBase* policy);
  ~TopLevelDispatcher() override;

  Dispatcher* OnMessageReady(IPCParams* ipc,
                             CallbackGeneric* callback) override;
  bool SetupService(InterceptionManager* manager, IpcTag service) override;

 private:
  // Test IPC provider.
  bool Ping(IPCInfo* ipc, void* cookie);

  // Returns a dispatcher from ipc_targets_.
  Dispatcher* GetDispatcher(IpcTag ipc_tag);

  PolicyBase* policy_;
  std::unique_ptr<Dispatcher> filesystem_dispatcher_;
  std::unique_ptr<Dispatcher> named_pipe_dispatcher_;
  std::unique_ptr<Dispatcher> thread_process_dispatcher_;
  std::unique_ptr<Dispatcher> sync_dispatcher_;
  std::unique_ptr<Dispatcher> registry_dispatcher_;
  std::unique_ptr<Dispatcher> handle_dispatcher_;
  std::unique_ptr<Dispatcher> process_mitigations_win32k_dispatcher_;
  std::unique_ptr<Dispatcher> signed_dispatcher_;
  Dispatcher* ipc_targets_[kMaxIpcTag];

  DISALLOW_COPY_AND_ASSIGN(TopLevelDispatcher);
};

}  // namespace sandbox

#endif  // SANDBOX_SRC_TOP_LEVEL_DISPATCHER_H__
