// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/sync_interception.h"

#include <stdint.h>

#include "sandbox/win/src/crosscall_client.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/policy_params.h"
#include "sandbox/win/src/policy_target.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/sandbox_nt_util.h"
#include "sandbox/win/src/sharedmem_ipc_client.h"
#include "sandbox/win/src/target_services.h"

namespace sandbox {

ResultCode ProxyCreateEvent(LPCWSTR name,
                            uint32_t initial_state,
                            EVENT_TYPE event_type,
                            void* ipc_memory,
                            CrossCallReturn* answer) {
  CountedParameterSet<NameBased> params;
  params[NameBased::NAME] = ParamPickerMake(name);

  if (!QueryBroker(IpcTag::CREATEEVENT, params.GetBase()))
    return SBOX_ERROR_GENERIC;

  SharedMemIPCClient ipc(ipc_memory);
  ResultCode code = CrossCall(ipc, IpcTag::CREATEEVENT, name, event_type,
                              initial_state, answer);
  return code;
}

ResultCode ProxyOpenEvent(LPCWSTR name,
                          uint32_t desired_access,
                          void* ipc_memory,
                          CrossCallReturn* answer) {
  CountedParameterSet<OpenEventParams> params;
  params[OpenEventParams::NAME] = ParamPickerMake(name);
  params[OpenEventParams::ACCESS] = ParamPickerMake(desired_access);

  if (!QueryBroker(IpcTag::OPENEVENT, params.GetBase()))
    return SBOX_ERROR_GENERIC;

  SharedMemIPCClient ipc(ipc_memory);
  ResultCode code =
      CrossCall(ipc, IpcTag::OPENEVENT, name, desired_access, answer);

  return code;
}

NTSTATUS WINAPI TargetNtCreateEvent(NtCreateEventFunction orig_CreateEvent,
                                    PHANDLE event_handle,
                                    ACCESS_MASK desired_access,
                                    POBJECT_ATTRIBUTES object_attributes,
                                    EVENT_TYPE event_type,
                                    BOOLEAN initial_state) {
  NTSTATUS status =
      orig_CreateEvent(event_handle, desired_access, object_attributes,
                       event_type, initial_state);
  if (status != STATUS_ACCESS_DENIED || !object_attributes)
    return status;

  // We don't trust that the IPC can work this early.
  if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled())
    return status;

  do {
    if (!ValidParameter(event_handle, sizeof(HANDLE), WRITE))
      break;

    void* memory = GetGlobalIPCMemory();
    if (!memory)
      break;

    OBJECT_ATTRIBUTES object_attribs_copy = *object_attributes;
    // The RootDirectory points to BaseNamedObjects. We can ignore it.
    object_attribs_copy.RootDirectory = nullptr;

    std::unique_ptr<wchar_t, NtAllocDeleter> name;
    uint32_t attributes = 0;
    NTSTATUS ret =
        AllocAndCopyName(&object_attribs_copy, &name, &attributes, nullptr);
    if (!NT_SUCCESS(ret) || !name)
      break;

    CrossCallReturn answer = {0};
    answer.nt_status = status;
    ResultCode code = ProxyCreateEvent(name.get(), initial_state, event_type,
                                       memory, &answer);

    if (code != SBOX_ALL_OK) {
      status = answer.nt_status;
      break;
    }
    __try {
      *event_handle = answer.handle;
      status = STATUS_SUCCESS;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
      break;
    }
  } while (false);

  return status;
}

NTSTATUS WINAPI TargetNtOpenEvent(NtOpenEventFunction orig_OpenEvent,
                                  PHANDLE event_handle,
                                  ACCESS_MASK desired_access,
                                  POBJECT_ATTRIBUTES object_attributes) {
  NTSTATUS status =
      orig_OpenEvent(event_handle, desired_access, object_attributes);
  if (status != STATUS_ACCESS_DENIED || !object_attributes)
    return status;

  // We don't trust that the IPC can work this early.
  if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled())
    return status;

  do {
    if (!ValidParameter(event_handle, sizeof(HANDLE), WRITE))
      break;

    void* memory = GetGlobalIPCMemory();
    if (!memory)
      break;

    OBJECT_ATTRIBUTES object_attribs_copy = *object_attributes;
    // The RootDirectory points to BaseNamedObjects. We can ignore it.
    object_attribs_copy.RootDirectory = nullptr;

    std::unique_ptr<wchar_t, NtAllocDeleter> name;
    uint32_t attributes = 0;
    NTSTATUS ret =
        AllocAndCopyName(&object_attribs_copy, &name, &attributes, nullptr);
    if (!NT_SUCCESS(ret) || !name)
      break;

    CrossCallReturn answer = {0};
    answer.nt_status = status;
    ResultCode code =
        ProxyOpenEvent(name.get(), desired_access, memory, &answer);

    if (code != SBOX_ALL_OK) {
      status = answer.nt_status;
      break;
    }
    __try {
      *event_handle = answer.handle;
      status = STATUS_SUCCESS;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
      break;
    }
  } while (false);

  return status;
}

}  // namespace sandbox
