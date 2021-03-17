// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_SIGNED_DISPATCHER_H_
#define SANDBOX_WIN_SRC_SIGNED_DISPATCHER_H_

#include <stdint.h>

#include "base/macros.h"
#include "sandbox/win/src/crosscall_server.h"
#include "sandbox/win/src/interception.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/sandbox_policy_base.h"

namespace sandbox {

// This class handles signed-binary related IPC calls.
class SignedDispatcher : public Dispatcher {
 public:
  explicit SignedDispatcher(PolicyBase* policy_base);
  ~SignedDispatcher() override {}

  // Dispatcher interface.
  bool SetupService(InterceptionManager* manager, IpcTag service) override;

 private:
  // Processes IPC requests coming from calls to CreateSection in the target.
  bool CreateSection(IPCInfo* ipc, HANDLE file_handle);

  PolicyBase* policy_base_;
  DISALLOW_COPY_AND_ASSIGN(SignedDispatcher);
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_SIGNED_DISPATCHER_H_
