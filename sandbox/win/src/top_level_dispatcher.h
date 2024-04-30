// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_TOP_LEVEL_DISPATCHER_H_
#define SANDBOX_WIN_SRC_TOP_LEVEL_DISPATCHER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "sandbox/win/src/crosscall_server.h"
#include "sandbox/win/src/interception.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/sandbox_policy_base.h"

namespace sandbox {

// Top level dispatcher which hands requests to the appropriate service
// dispatchers.
class TopLevelDispatcher : public Dispatcher {
 public:
  // `policy` must outlive this class, and be fully Configured.
  explicit TopLevelDispatcher(PolicyBase* policy);

  TopLevelDispatcher(const TopLevelDispatcher&) = delete;
  TopLevelDispatcher& operator=(const TopLevelDispatcher&) = delete;

  ~TopLevelDispatcher() override;

  Dispatcher* OnMessageReady(IPCParams* ipc,
                             CallbackGeneric* callback) override;
  bool SetupService(InterceptionManager* manager, IpcTag service) override;

 private:
  friend class PolicyDiagnostic;

  // Test IPC provider.
  bool Ping(IPCInfo* ipc, void* cookie);

  // Returns a dispatcher from ipc_targets_.
  Dispatcher* GetDispatcher(IpcTag ipc_tag);
  // Helper that reports the set of IPCs this top level dispatcher can service.
  std::vector<IpcTag> ipc_targets();

  raw_ptr<PolicyBase> policy_;
  // Dispatchers below are only created if they are needed.
  std::unique_ptr<Dispatcher> filesystem_dispatcher_;
  std::unique_ptr<Dispatcher> thread_process_dispatcher_;
  std::unique_ptr<Dispatcher> handle_dispatcher_;
  std::unique_ptr<Dispatcher> process_mitigations_win32k_dispatcher_;
  std::unique_ptr<Dispatcher> signed_dispatcher_;
  Dispatcher* ipc_targets_[kSandboxIpcCount];
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_TOP_LEVEL_DISPATCHER_H_
