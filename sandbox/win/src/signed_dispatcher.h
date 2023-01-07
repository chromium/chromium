// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_SIGNED_DISPATCHER_H_
#define SANDBOX_WIN_SRC_SIGNED_DISPATCHER_H_

#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "sandbox/win/src/crosscall_server.h"
#include "sandbox/win/src/interception.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/sandbox_policy_base.h"

namespace sandbox {

// This class handles signed-binary related IPC calls.
class SignedDispatcher : public Dispatcher {
 public:
  explicit SignedDispatcher(PolicyBase* policy_base);

  SignedDispatcher(const SignedDispatcher&) = delete;
  SignedDispatcher& operator=(const SignedDispatcher&) = delete;

  ~SignedDispatcher() override {}

  // Dispatcher interface.
  bool SetupService(InterceptionManager* manager, IpcTag service) override;

 private:
  // Processes IPC requests coming from calls to CreateSection in the target.
  bool CreateSection(IPCInfo* ipc, HANDLE file_handle);

  raw_ptr<PolicyBase> policy_base_;
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_SIGNED_DISPATCHER_H_
