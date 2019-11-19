// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/registry_interception.h"

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

NTSTATUS WINAPI TargetNtCreateKey(NtCreateKeyFunction orig_CreateKey,
                                  PHANDLE key,
                                  ACCESS_MASK desired_access,
                                  POBJECT_ATTRIBUTES object_attributes,
                                  ULONG title_index,
                                  PUNICODE_STRING class_name,
                                  ULONG create_options,
                                  PULONG disposition) {
  // Check if the process can create it first.
  NTSTATUS status =
      orig_CreateKey(key, desired_access, object_attributes, title_index,
                     class_name, create_options, disposition);
  if (NT_SUCCESS(status))
    return status;

  // We don't trust that the IPC can work this early.
  if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled())
    return status;

  do {
    if (!ValidParameter(key, sizeof(HANDLE), WRITE))
      break;

    if (disposition && !ValidParameter(disposition, sizeof(ULONG), WRITE))
      break;

    // At this point we don't support class_name.
    if (class_name && class_name->Buffer && class_name->Length)
      break;

    // We don't support creating link keys, volatile keys and backup/restore.
    if (create_options)
      break;

    void* memory = GetGlobalIPCMemory();
    if (!memory)
      break;

    std::unique_ptr<wchar_t, NtAllocDeleter> name;
    uint32_t attributes = 0;
    HANDLE root_directory = 0;
    NTSTATUS ret = AllocAndCopyName(object_attributes, &name, &attributes,
                                    &root_directory);
    if (!NT_SUCCESS(ret) || !name)
      break;

    uint32_t desired_access_uint32 = desired_access;
    CountedParameterSet<OpenKey> params;
    params[OpenKey::ACCESS] = ParamPickerMake(desired_access_uint32);

    bool query_broker = false;
    {
      std::unique_ptr<wchar_t, NtAllocDeleter> full_name;
      const wchar_t* name_ptr = name.get();
      const wchar_t* full_name_ptr = nullptr;

      if (root_directory) {
        ret = sandbox::AllocAndGetFullPath(root_directory, name.get(),
                                           &full_name);
        if (!NT_SUCCESS(ret) || !full_name)
          break;
        full_name_ptr = full_name.get();
        params[OpenKey::NAME] = ParamPickerMake(full_name_ptr);
      } else {
        params[OpenKey::NAME] = ParamPickerMake(name_ptr);
      }

      query_broker = QueryBroker(IpcTag::NTCREATEKEY, params.GetBase());
    }

    if (!query_broker)
      break;

    SharedMemIPCClient ipc(memory);
    CrossCallReturn answer = {0};

    ResultCode code = CrossCall(ipc, IpcTag::NTCREATEKEY, name.get(),
                                attributes, root_directory, desired_access,
                                title_index, create_options, &answer);

    if (SBOX_ALL_OK != code)
      break;

    if (!NT_SUCCESS(answer.nt_status))
      // TODO(nsylvain): We should return answer.nt_status here instead
      // of status. We can do this only after we checked the policy.
      // otherwise we will returns ACCESS_DENIED for all paths
      // that are not specified by a policy, even though your token allows
      // access to that path, and the original call had a more meaningful
      // error. Bug 4369
      break;

    __try {
      *key = answer.handle;

      if (disposition)
        *disposition = answer.extended[0].unsigned_int;

      status = answer.nt_status;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
      break;
    }
  } while (false);

  return status;
}

NTSTATUS WINAPI CommonNtOpenKey(NTSTATUS status,
                                PHANDLE key,
                                ACCESS_MASK desired_access,
                                POBJECT_ATTRIBUTES object_attributes) {
  // We don't trust that the IPC can work this early.
  if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled())
    return status;

  do {
    if (!ValidParameter(key, sizeof(HANDLE), WRITE))
      break;

    void* memory = GetGlobalIPCMemory();
    if (!memory)
      break;

    std::unique_ptr<wchar_t, NtAllocDeleter> name;
    uint32_t attributes;
    HANDLE root_directory;
    NTSTATUS ret = AllocAndCopyName(object_attributes, &name, &attributes,
                                    &root_directory);
    if (!NT_SUCCESS(ret) || !name)
      break;

    uint32_t desired_access_uint32 = desired_access;
    CountedParameterSet<OpenKey> params;
    params[OpenKey::ACCESS] = ParamPickerMake(desired_access_uint32);

    bool query_broker = false;
    {
      std::unique_ptr<wchar_t, NtAllocDeleter> full_name;
      const wchar_t* name_ptr = name.get();
      const wchar_t* full_name_ptr = nullptr;

      if (root_directory) {
        ret = sandbox::AllocAndGetFullPath(root_directory, name.get(),
                                           &full_name);
        if (!NT_SUCCESS(ret) || !full_name)
          break;
        full_name_ptr = full_name.get();
        params[OpenKey::NAME] = ParamPickerMake(full_name_ptr);
      } else {
        params[OpenKey::NAME] = ParamPickerMake(name_ptr);
      }

      query_broker = QueryBroker(IpcTag::NTOPENKEY, params.GetBase());
    }

    if (!query_broker)
      break;

    SharedMemIPCClient ipc(memory);
    CrossCallReturn answer = {0};
    ResultCode code = CrossCall(ipc, IpcTag::NTOPENKEY, name.get(), attributes,
                                root_directory, desired_access, &answer);

    if (SBOX_ALL_OK != code)
      break;

    if (!NT_SUCCESS(answer.nt_status))
      // TODO(nsylvain): We should return answer.nt_status here instead
      // of status. We can do this only after we checked the policy.
      // otherwise we will returns ACCESS_DENIED for all paths
      // that are not specified by a policy, even though your token allows
      // access to that path, and the original call had a more meaningful
      // error. Bug 4369
      break;

    __try {
      *key = answer.handle;
      status = answer.nt_status;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
      break;
    }
  } while (false);

  return status;
}

NTSTATUS WINAPI TargetNtOpenKey(NtOpenKeyFunction orig_OpenKey,
                                PHANDLE key,
                                ACCESS_MASK desired_access,
                                POBJECT_ATTRIBUTES object_attributes) {
  // Check if the process can open it first.
  NTSTATUS status = orig_OpenKey(key, desired_access, object_attributes);
  if (NT_SUCCESS(status))
    return status;

  return CommonNtOpenKey(status, key, desired_access, object_attributes);
}

NTSTATUS WINAPI TargetNtOpenKeyEx(NtOpenKeyExFunction orig_OpenKeyEx,
                                  PHANDLE key,
                                  ACCESS_MASK desired_access,
                                  POBJECT_ATTRIBUTES object_attributes,
                                  ULONG open_options) {
  // Check if the process can open it first.
  NTSTATUS status =
      orig_OpenKeyEx(key, desired_access, object_attributes, open_options);

  // We do not support open_options at this time. The 2 current known values
  // are REG_OPTION_CREATE_LINK, to open a symbolic link, and
  // REG_OPTION_BACKUP_RESTORE to open the key with special privileges.
  if (NT_SUCCESS(status) || open_options != 0)
    return status;

  return CommonNtOpenKey(status, key, desired_access, object_attributes);
}

}  // namespace sandbox
