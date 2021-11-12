// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_REGISTRY_DISPATCHER_H_
#define SANDBOX_WIN_SRC_REGISTRY_DISPATCHER_H_

#include <stdint.h>

#include <string>

#include "sandbox/win/src/crosscall_server.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/sandbox_policy_base.h"

namespace sandbox {

// This class handles registry-related IPC calls.
class RegistryDispatcher : public Dispatcher {
 public:
  explicit RegistryDispatcher(PolicyBase* policy_base);

  RegistryDispatcher(const RegistryDispatcher&) = delete;
  RegistryDispatcher& operator=(const RegistryDispatcher&) = delete;

  ~RegistryDispatcher() override {}

  // Dispatcher interface.
  bool SetupService(InterceptionManager* manager, IpcTag service) override;

 private:
  // Processes IPC requests coming from calls to NtCreateKey in the target.
  bool NtCreateKey(IPCInfo* ipc,
                   std::wstring* name,
                   uint32_t attributes,
                   HANDLE root,
                   uint32_t desired_access,
                   uint32_t title_index,
                   uint32_t create_options);

  // Processes IPC requests coming from calls to NtOpenKey in the target.
  bool NtOpenKey(IPCInfo* ipc,
                 std::wstring* name,
                 uint32_t attributes,
                 HANDLE root,
                 uint32_t desired_access);

  PolicyBase* policy_base_;
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_REGISTRY_DISPATCHER_H_
