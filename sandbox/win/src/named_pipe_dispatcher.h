// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_NAMED_PIPE_DISPATCHER_H__
#define SANDBOX_SRC_NAMED_PIPE_DISPATCHER_H__

#include <stdint.h>

#include <string>

#include "base/macros.h"
#include "sandbox/win/src/crosscall_server.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/sandbox_policy_base.h"

namespace sandbox {

// This class handles named pipe related IPC calls.
class NamedPipeDispatcher : public Dispatcher {
 public:
  explicit NamedPipeDispatcher(PolicyBase* policy_base);
  ~NamedPipeDispatcher() override {}

  // Dispatcher interface.
  bool SetupService(InterceptionManager* manager, IpcTag service) override;

 private:
  // Processes IPC requests coming from calls to CreateNamedPipeW() in the
  // target.
  bool CreateNamedPipe(IPCInfo* ipc,
                       std::wstring* name,
                       uint32_t open_mode,
                       uint32_t pipe_mode,
                       uint32_t max_instances,
                       uint32_t out_buffer_size,
                       uint32_t in_buffer_size,
                       uint32_t default_timeout);

  PolicyBase* policy_base_;
  DISALLOW_COPY_AND_ASSIGN(NamedPipeDispatcher);
};

}  // namespace sandbox

#endif  // SANDBOX_SRC_NAMED_PIPE_DISPATCHER_H__
