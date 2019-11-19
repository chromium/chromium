// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/named_pipe_policy.h"

#include <string>

#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/policy_engine_opcodes.h"
#include "sandbox/win/src/policy_params.h"
#include "sandbox/win/src/sandbox_types.h"

namespace {

// Creates a named pipe and duplicates the handle to 'target_process'. The
// remaining parameters are the same as CreateNamedPipeW().
HANDLE CreateNamedPipeHelper(HANDLE target_process,
                             LPCWSTR pipe_name,
                             DWORD open_mode,
                             DWORD pipe_mode,
                             DWORD max_instances,
                             DWORD out_buffer_size,
                             DWORD in_buffer_size,
                             DWORD default_timeout,
                             LPSECURITY_ATTRIBUTES security_attributes) {
  HANDLE pipe = ::CreateNamedPipeW(
      pipe_name, open_mode, pipe_mode, max_instances, out_buffer_size,
      in_buffer_size, default_timeout, security_attributes);
  if (INVALID_HANDLE_VALUE == pipe)
    return pipe;

  HANDLE new_pipe;
  if (!::DuplicateHandle(::GetCurrentProcess(), pipe, target_process, &new_pipe,
                         0, false,
                         DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS)) {
    return INVALID_HANDLE_VALUE;
  }

  return new_pipe;
}

}  // namespace

namespace sandbox {

bool NamedPipePolicy::GenerateRules(const wchar_t* name,
                                    TargetPolicy::Semantics semantics,
                                    LowLevelPolicy* policy) {
  if (TargetPolicy::NAMEDPIPES_ALLOW_ANY != semantics) {
    return false;
  }
  PolicyRule pipe(ASK_BROKER);
  if (!pipe.AddStringMatch(IF, NameBased::NAME, name, CASE_INSENSITIVE)) {
    return false;
  }
  if (!policy->AddRule(IpcTag::CREATENAMEDPIPEW, &pipe)) {
    return false;
  }
  return true;
}

DWORD NamedPipePolicy::CreateNamedPipeAction(EvalResult eval_result,
                                             const ClientInfo& client_info,
                                             const std::wstring& name,
                                             DWORD open_mode,
                                             DWORD pipe_mode,
                                             DWORD max_instances,
                                             DWORD out_buffer_size,
                                             DWORD in_buffer_size,
                                             DWORD default_timeout,
                                             HANDLE* pipe) {
  *pipe = INVALID_HANDLE_VALUE;
  // The only action supported is ASK_BROKER which means create the pipe.
  if (ASK_BROKER != eval_result) {
    return ERROR_ACCESS_DENIED;
  }

  *pipe = CreateNamedPipeHelper(client_info.process, name.c_str(), open_mode,
                                pipe_mode, max_instances, out_buffer_size,
                                in_buffer_size, default_timeout, nullptr);

  if (INVALID_HANDLE_VALUE == *pipe)
    return ERROR_ACCESS_DENIED;

  return ERROR_SUCCESS;
}

}  // namespace sandbox
