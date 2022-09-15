// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_IPC_ARGS_H_
#define SANDBOX_WIN_SRC_IPC_ARGS_H_

#include "sandbox/win/src/crosscall_params.h"
#include "sandbox/win/src/crosscall_server.h"

namespace sandbox {

// Releases memory allocated for IPC arguments.
void ReleaseArgs(const IPCParams* ipc_params, void* args[kMaxIpcParams]);

// Fills up the list of arguments (args and ipc_params) for an IPC call.
// Call ReleaseArgs on |ipc_params| and |args| after calling this.
bool GetArgs(CrossCallParamsEx* params,
             IPCParams* ipc_params,
             void* args[kMaxIpcParams]);

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_IPC_ARGS_H_
