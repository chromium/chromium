// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/process_thread_interception.h"

#include <stdint.h>

#include <optional>
#include "sandbox/win/src/crosscall_client.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/policy_params.h"
#include "sandbox/win/src/policy_target.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/sandbox_nt_util.h"
#include "sandbox/win/src/sharedmem_ipc_client.h"
#include "sandbox/win/src/target_services.h"

namespace sandbox {

namespace {

NTSTATUS DuplicateObject(HANDLE handle,
                         ACCESS_MASK desired_access,
                         PHANDLE out_handle) {
  if (desired_access & MAXIMUM_ALLOWED) {
    desired_access |= GENERIC_ALL;
  }

  return GetNtExports()->DuplicateObject(CURRENT_PROCESS, handle,
                                         CURRENT_PROCESS, out_handle,
                                         desired_access, 0, 0);
}

template <typename T>
std::optional<T> CaptureParameter(const T* parameter) {
  if (parameter) {
    __try {
      return *parameter;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
  }
  return std::nullopt;
}

bool ValidObjectAttributes(const OBJECT_ATTRIBUTES* object_attributes) {
  if (!object_attributes) {
    return true;
  }
  std::optional<OBJECT_ATTRIBUTES> valid_obj_attr =
      CaptureParameter(object_attributes);
  return valid_obj_attr.has_value() && !valid_obj_attr->Attributes &&
         !valid_obj_attr->ObjectName && !valid_obj_attr->RootDirectory &&
         !valid_obj_attr->SecurityDescriptor &&
         !valid_obj_attr->SecurityQualityOfService;
}

NTSTATUS CallNtOpenProcessTokenEx(NTSTATUS status,
                                  HANDLE process,
                                  ACCESS_MASK desired_access,
                                  ULONG handle_attributes,
                                  PHANDLE token) {
  if (NT_SUCCESS(status)) {
    return status;
  }

  if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled()) {
    return status;
  }

  if (CURRENT_PROCESS != process ||
      !ValidParameter(token, sizeof(HANDLE), WRITE)) {
    return status;
  }

  void* memory = GetGlobalIPCMemory();
  if (!memory) {
    return status;
  }

  SharedMemIPCClient ipc(memory);
  CrossCallReturn answer = {0};
  ResultCode code = CrossCall(ipc, IpcTag::NTOPENPROCESSTOKENEX, process,
                              desired_access, handle_attributes, &answer);
  if (SBOX_ALL_OK != code) {
    return status;
  }

  if (!NT_SUCCESS(answer.nt_status)) {
    return answer.nt_status;
  }

  __try {
    // Write the output parameters.
    *token = answer.handle;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return status;
  }

  return answer.nt_status;
}

}  // namespace

// Hooks NtOpenThread and proxy the call to the broker if it's trying to
// open a thread in the same process.
NTSTATUS WINAPI TargetNtOpenThread(NtOpenThreadFunction orig_OpenThread,
                                   PHANDLE thread,
                                   ACCESS_MASK desired_access,
                                   POBJECT_ATTRIBUTES object_attributes,
                                   PCLIENT_ID client_id) {
  NTSTATUS status =
      orig_OpenThread(thread, desired_access, object_attributes, client_id);
  if (NT_SUCCESS(status)) {
    return status;
  }

  if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled()) {
    return status;
  }

  if (!ValidParameter(thread, sizeof(HANDLE), WRITE) ||
      !ValidObjectAttributes(object_attributes)) {
    return status;
  }

  std::optional<CLIENT_ID> valid_client_id = CaptureParameter(client_id);
  if (!valid_client_id.has_value() || valid_client_id->UniqueProcess) {
    return status;
  }

  if (valid_client_id->UniqueThread == GetCurrentClientId().UniqueThread) {
    return DuplicateObject(CURRENT_THREAD, desired_access, thread);
  }

  void* memory = GetGlobalIPCMemory();
  if (!memory) {
    return status;
  }

  SharedMemIPCClient ipc(memory);
  CrossCallReturn answer = {0};
  ResultCode code = CrossCall(ipc, IpcTag::NTOPENTHREAD, desired_access,
                              static_cast<uint32_t>(reinterpret_cast<ULONG_PTR>(
                                  valid_client_id->UniqueThread)),
                              &answer);
  if (SBOX_ALL_OK != code) {
    return status;
  }

  if (!NT_SUCCESS(answer.nt_status)) {
    // The nt_status here is most likely STATUS_INVALID_CID because
    // in the broker we set the process id in the CID (client ID) param
    // to be the current process. If you try to open a thread from another
    // process you will get this INVALID_CID error. On the other hand, if you
    // try to open a thread in your own process, it should return success.
    // We don't want to return STATUS_INVALID_CID here, so we return the
    // return of the original open thread status, which is most likely
    // STATUS_ACCESS_DENIED.
    return status;
  }

  __try {
    // Write the output parameters.
    *thread = answer.handle;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return status;
  }

  return answer.nt_status;
}

// Hooks NtOpenProcess and duplicates the current process handle if opening the
// current process.
NTSTATUS WINAPI TargetNtOpenProcess(NtOpenProcessFunction orig_OpenProcess,
                                    PHANDLE process,
                                    ACCESS_MASK desired_access,
                                    POBJECT_ATTRIBUTES object_attributes,
                                    PCLIENT_ID client_id) {
  NTSTATUS status =
      orig_OpenProcess(process, desired_access, object_attributes, client_id);
  if (NT_SUCCESS(status)) {
    return status;
  }

  if (!ValidObjectAttributes(object_attributes)) {
    return status;
  }

  std::optional<CLIENT_ID> valid_client_id = CaptureParameter(client_id);
  if (!valid_client_id.has_value() ||
      valid_client_id->UniqueProcess != GetCurrentClientId().UniqueProcess) {
    return status;
  }

  return DuplicateObject(CURRENT_PROCESS, desired_access, process);
}

NTSTATUS WINAPI
TargetNtOpenProcessToken(NtOpenProcessTokenFunction orig_OpenProcessToken,
                         HANDLE process,
                         ACCESS_MASK desired_access,
                         PHANDLE token) {
  // NtOpenProcessToken is just NtOpenProcessTokenEx with handle_attributes set
  // to 0.
  return CallNtOpenProcessTokenEx(
      orig_OpenProcessToken(process, desired_access, token), process,
      desired_access, 0, token);
}

NTSTATUS WINAPI
TargetNtOpenProcessTokenEx(NtOpenProcessTokenExFunction orig_OpenProcessTokenEx,
                           HANDLE process,
                           ACCESS_MASK desired_access,
                           ULONG handle_attributes,
                           PHANDLE token) {
  return CallNtOpenProcessTokenEx(
      orig_OpenProcessTokenEx(process, desired_access, handle_attributes,
                              token),
      process, desired_access, handle_attributes, token);
}

HANDLE WINAPI TargetCreateThread(CreateThreadFunction orig_CreateThread,
                                 LPSECURITY_ATTRIBUTES thread_attributes,
                                 SIZE_T stack_size,
                                 LPTHREAD_START_ROUTINE start_address,
                                 LPVOID parameter,
                                 DWORD creation_flags,
                                 LPDWORD thread_id) {
  HANDLE hThread = nullptr;

  TargetServices* target_services = SandboxFactory::GetTargetServices();
  if (!target_services || target_services->GetState()->IsCsrssConnected()) {
    hThread = orig_CreateThread(thread_attributes, stack_size, start_address,
                                parameter, creation_flags, thread_id);
    if (hThread)
      return hThread;
  }

  DWORD original_error = ::GetLastError();
  do {
    if (!target_services)
      break;

    // We don't trust that the IPC can work this early.
    if (!target_services->GetState()->InitCalled())
      break;

    __try {
      if (thread_id && !ValidParameter(thread_id, sizeof(*thread_id), WRITE))
        break;

      if (!start_address)
        break;
      // We don't support thread_attributes not being null.
      if (thread_attributes)
        break;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
      break;
    }

    void* memory = GetGlobalIPCMemory();
    if (!memory)
      break;

    SharedMemIPCClient ipc(memory);
    CrossCallReturn answer = {0};

    // NOTE: we don't pass the thread_attributes through. This matches the
    // approach in CreateProcess and in CreateThreadInternal().
    ResultCode code = CrossCall(ipc, IpcTag::CREATETHREAD,
                                reinterpret_cast<LPVOID>(stack_size),
                                reinterpret_cast<LPVOID>(start_address),
                                parameter, creation_flags, &answer);
    if (SBOX_ALL_OK != code)
      break;

    ::SetLastError(answer.win32_result);
    if (ERROR_SUCCESS != answer.win32_result)
      return nullptr;

    __try {
      if (thread_id)
        *thread_id = ::GetThreadId(answer.handle);
      return answer.handle;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
      break;
    }
  } while (false);

  ::SetLastError(original_error);
  return nullptr;
}

}  // namespace sandbox
