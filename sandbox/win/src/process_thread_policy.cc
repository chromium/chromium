// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/process_thread_policy.h"

#include <stdint.h>

#include <memory>
#include <string>

#include "base/memory/free_deleter.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/policy_engine_opcodes.h"
#include "sandbox/win/src/policy_params.h"
#include "sandbox/win/src/sandbox_types.h"
#include "sandbox/win/src/win_utils.h"

namespace sandbox {

NTSTATUS ProcessPolicy::OpenThreadAction(const ClientInfo& client_info,
                                         uint32_t desired_access,
                                         uint32_t thread_id,
                                         HANDLE* handle) {
  *handle = nullptr;

  NtOpenThreadFunction NtOpenThread = nullptr;
  ResolveNTFunctionPtr("NtOpenThread", &NtOpenThread);

  OBJECT_ATTRIBUTES attributes = {0};
  attributes.Length = sizeof(attributes);
  CLIENT_ID client_id = {0};
  client_id.UniqueProcess =
      reinterpret_cast<PVOID>(static_cast<ULONG_PTR>(client_info.process_id));
  client_id.UniqueThread =
      reinterpret_cast<PVOID>(static_cast<ULONG_PTR>(thread_id));

  HANDLE local_handle = nullptr;
  NTSTATUS status =
      NtOpenThread(&local_handle, desired_access, &attributes, &client_id);
  if (NT_SUCCESS(status)) {
    if (!::DuplicateHandle(::GetCurrentProcess(), local_handle,
                           client_info.process, handle, 0, false,
                           DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS)) {
      return STATUS_ACCESS_DENIED;
    }
  }

  return status;
}

NTSTATUS ProcessPolicy::OpenProcessAction(const ClientInfo& client_info,
                                          uint32_t desired_access,
                                          uint32_t process_id,
                                          HANDLE* handle) {
  *handle = nullptr;

  NtOpenProcessFunction NtOpenProcess = nullptr;
  ResolveNTFunctionPtr("NtOpenProcess", &NtOpenProcess);

  if (client_info.process_id != process_id)
    return STATUS_ACCESS_DENIED;

  OBJECT_ATTRIBUTES attributes = {0};
  attributes.Length = sizeof(attributes);
  CLIENT_ID client_id = {0};
  client_id.UniqueProcess =
      reinterpret_cast<PVOID>(static_cast<ULONG_PTR>(client_info.process_id));
  HANDLE local_handle = nullptr;
  NTSTATUS status =
      NtOpenProcess(&local_handle, desired_access, &attributes, &client_id);
  if (NT_SUCCESS(status)) {
    if (!::DuplicateHandle(::GetCurrentProcess(), local_handle,
                           client_info.process, handle, 0, false,
                           DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS)) {
      return STATUS_ACCESS_DENIED;
    }
  }

  return status;
}

NTSTATUS ProcessPolicy::OpenProcessTokenAction(const ClientInfo& client_info,
                                               HANDLE process,
                                               uint32_t desired_access,
                                               HANDLE* handle) {
  *handle = nullptr;
  NtOpenProcessTokenFunction NtOpenProcessToken = nullptr;
  ResolveNTFunctionPtr("NtOpenProcessToken", &NtOpenProcessToken);

  if (CURRENT_PROCESS != process)
    return STATUS_ACCESS_DENIED;

  HANDLE local_handle = nullptr;
  NTSTATUS status =
      NtOpenProcessToken(client_info.process, desired_access, &local_handle);
  if (NT_SUCCESS(status)) {
    if (!::DuplicateHandle(::GetCurrentProcess(), local_handle,
                           client_info.process, handle, 0, false,
                           DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS)) {
      return STATUS_ACCESS_DENIED;
    }
  }
  return status;
}

NTSTATUS ProcessPolicy::OpenProcessTokenExAction(const ClientInfo& client_info,
                                                 HANDLE process,
                                                 uint32_t desired_access,
                                                 uint32_t attributes,
                                                 HANDLE* handle) {
  *handle = nullptr;
  NtOpenProcessTokenExFunction NtOpenProcessTokenEx = nullptr;
  ResolveNTFunctionPtr("NtOpenProcessTokenEx", &NtOpenProcessTokenEx);

  if (CURRENT_PROCESS != process)
    return STATUS_ACCESS_DENIED;

  HANDLE local_handle = nullptr;
  NTSTATUS status = NtOpenProcessTokenEx(client_info.process, desired_access,
                                         attributes, &local_handle);
  if (NT_SUCCESS(status)) {
    if (!::DuplicateHandle(::GetCurrentProcess(), local_handle,
                           client_info.process, handle, 0, false,
                           DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS)) {
      return STATUS_ACCESS_DENIED;
    }
  }
  return status;
}

DWORD ProcessPolicy::CreateThreadAction(
    const ClientInfo& client_info,
    const SIZE_T stack_size,
    const LPTHREAD_START_ROUTINE start_address,
    const LPVOID parameter,
    const DWORD creation_flags,
    LPDWORD thread_id,
    HANDLE* handle) {
  *handle = nullptr;
  HANDLE local_handle =
      ::CreateRemoteThread(client_info.process, nullptr, stack_size,
                           start_address, parameter, creation_flags, thread_id);
  if (!local_handle) {
    return ::GetLastError();
  }
  if (!::DuplicateHandle(::GetCurrentProcess(), local_handle,
                         client_info.process, handle, 0, false,
                         DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS)) {
    return ERROR_ACCESS_DENIED;
  }
  return ERROR_SUCCESS;
}

}  // namespace sandbox
