// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/named_pipe_dispatcher.h"

#include <stdint.h>

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

#include "sandbox/win/src/crosscall_client.h"
#include "sandbox/win/src/interception.h"
#include "sandbox/win/src/interceptors.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/named_pipe_interception.h"
#include "sandbox/win/src/named_pipe_policy.h"
#include "sandbox/win/src/policy_broker.h"
#include "sandbox/win/src/policy_params.h"
#include "sandbox/win/src/sandbox.h"

namespace sandbox {

NamedPipeDispatcher::NamedPipeDispatcher(PolicyBase* policy_base)
    : policy_base_(policy_base) {
  static const IPCCall create_params = {
      {IpcTag::CREATENAMEDPIPEW,
       {WCHAR_TYPE, UINT32_TYPE, UINT32_TYPE, UINT32_TYPE, UINT32_TYPE,
        UINT32_TYPE, UINT32_TYPE}},
      reinterpret_cast<CallbackGeneric>(&NamedPipeDispatcher::CreateNamedPipe)};

  ipc_calls_.push_back(create_params);
}

bool NamedPipeDispatcher::SetupService(InterceptionManager* manager,
                                       IpcTag service) {
  if (IpcTag::CREATENAMEDPIPEW == service)
    return INTERCEPT_EAT(manager, kKerneldllName, CreateNamedPipeW,
                         CREATE_NAMED_PIPE_ID, 36);

  return false;
}

bool NamedPipeDispatcher::CreateNamedPipe(IPCInfo* ipc,
                                          std::wstring* name,
                                          uint32_t open_mode,
                                          uint32_t pipe_mode,
                                          uint32_t max_instances,
                                          uint32_t out_buffer_size,
                                          uint32_t in_buffer_size,
                                          uint32_t default_timeout) {
  ipc->return_info.win32_result = ERROR_ACCESS_DENIED;
  ipc->return_info.handle = INVALID_HANDLE_VALUE;

  base::StringPiece16 dotdot(STRING16_LITERAL(".."));

  for (const base::StringPiece16& path : base::SplitStringPiece(
           base::AsStringPiece16(*name), STRING16_LITERAL("/"),
           base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    for (const base::StringPiece16& inner :
         base::SplitStringPiece(path, STRING16_LITERAL("\\"),
                                base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
      if (inner == dotdot)
        return true;
    }
  }

  const wchar_t* pipe_name = name->c_str();
  CountedParameterSet<NameBased> params;
  params[NameBased::NAME] = ParamPickerMake(pipe_name);

  EvalResult eval =
      policy_base_->EvalPolicy(IpcTag::CREATENAMEDPIPEW, params.GetBase());

  // "For file I/O, the "\\?\" prefix to a path string tells the Windows APIs to
  // disable all string parsing and to send the string that follows it straight
  // to the file system."
  // http://msdn.microsoft.com/en-us/library/aa365247(VS.85).aspx
  // This ensures even if there is a path traversal in the pipe name, and it is
  // able to get past the checks above, it will still not be allowed to escape
  // our allowed namespace.
  if (name->compare(0, 4, L"\\\\.\\") == 0)
    name->replace(0, 4, L"\\\\\?\\");

  HANDLE pipe;
  DWORD ret = NamedPipePolicy::CreateNamedPipeAction(
      eval, *ipc->client_info, *name, open_mode, pipe_mode, max_instances,
      out_buffer_size, in_buffer_size, default_timeout, &pipe);

  ipc->return_info.win32_result = ret;
  ipc->return_info.handle = pipe;
  return true;
}

}  // namespace sandbox
