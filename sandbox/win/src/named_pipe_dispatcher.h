// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_NAMED_PIPE_DISPATCHER_H_
#define SANDBOX_WIN_SRC_NAMED_PIPE_DISPATCHER_H_

#include <stdint.h>

#include <string>

#include "base/memory/raw_ptr.h"
#include "sandbox/win/src/crosscall_server.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/sandbox_policy_base.h"

namespace sandbox {

// This class handles named pipe related IPC calls.
class NamedPipeDispatcher : public Dispatcher {
 public:
  explicit NamedPipeDispatcher(PolicyBase* policy_base);

  NamedPipeDispatcher(const NamedPipeDispatcher&) = delete;
  NamedPipeDispatcher& operator=(const NamedPipeDispatcher&) = delete;

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

  raw_ptr<PolicyBase> policy_base_;
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_NAMED_PIPE_DISPATCHER_H_
