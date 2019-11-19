// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/named_pipe_interception.h"

#include "sandbox/win/src/crosscall_client.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/policy_params.h"
#include "sandbox/win/src/policy_target.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/sandbox_nt_util.h"
#include "sandbox/win/src/sharedmem_ipc_client.h"
#include "sandbox/win/src/target_services.h"

namespace sandbox {

HANDLE WINAPI
TargetCreateNamedPipeW(CreateNamedPipeWFunction orig_CreateNamedPipeW,
                       LPCWSTR pipe_name,
                       DWORD open_mode,
                       DWORD pipe_mode,
                       DWORD max_instance,
                       DWORD out_buffer_size,
                       DWORD in_buffer_size,
                       DWORD default_timeout,
                       LPSECURITY_ATTRIBUTES security_attributes) {
  HANDLE pipe = orig_CreateNamedPipeW(
      pipe_name, open_mode, pipe_mode, max_instance, out_buffer_size,
      in_buffer_size, default_timeout, security_attributes);
  if (INVALID_HANDLE_VALUE != pipe)
    return pipe;

  // We don't trust that the IPC can work this early.
  if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled())
    return INVALID_HANDLE_VALUE;

  DWORD original_error = ::GetLastError();

  // We don't support specific Security Attributes.
  if (security_attributes)
    return INVALID_HANDLE_VALUE;

  do {
    void* memory = GetGlobalIPCMemory();
    if (!memory)
      break;

    CountedParameterSet<NameBased> params;
    params[NameBased::NAME] = ParamPickerMake(pipe_name);

    if (!QueryBroker(IpcTag::CREATENAMEDPIPEW, params.GetBase()))
      break;

    SharedMemIPCClient ipc(memory);
    CrossCallReturn answer = {0};
    ResultCode code =
        CrossCall(ipc, IpcTag::CREATENAMEDPIPEW, pipe_name, open_mode,
                  pipe_mode, max_instance, out_buffer_size, in_buffer_size,
                  default_timeout, &answer);
    if (SBOX_ALL_OK != code)
      break;

    ::SetLastError(answer.win32_result);

    if (ERROR_SUCCESS != answer.win32_result)
      return INVALID_HANDLE_VALUE;

    return answer.handle;
  } while (false);

  ::SetLastError(original_error);
  return INVALID_HANDLE_VALUE;
}

}  // namespace sandbox
