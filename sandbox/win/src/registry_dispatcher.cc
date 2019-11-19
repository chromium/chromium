// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/registry_dispatcher.h"

#include <stdint.h>

#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "sandbox/win/src/crosscall_client.h"
#include "sandbox/win/src/interception.h"
#include "sandbox/win/src/interceptors.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/policy_broker.h"
#include "sandbox/win/src/policy_params.h"
#include "sandbox/win/src/registry_interception.h"
#include "sandbox/win/src/registry_policy.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_nt_util.h"

namespace {

// Builds a path using the root directory and the name.
bool GetCompletePath(HANDLE root,
                     const std::wstring& name,
                     std::wstring* complete_name) {
  if (root) {
    if (!sandbox::GetPathFromHandle(root, complete_name))
      return false;

    *complete_name += L"\\";
    *complete_name += name;
  } else {
    *complete_name = name;
  }

  return true;
}

}  // namespace

namespace sandbox {

RegistryDispatcher::RegistryDispatcher(PolicyBase* policy_base)
    : policy_base_(policy_base) {
  static const IPCCall create_params = {
      {IpcTag::NTCREATEKEY,
       {WCHAR_TYPE, UINT32_TYPE, VOIDPTR_TYPE, UINT32_TYPE, UINT32_TYPE,
        UINT32_TYPE}},
      reinterpret_cast<CallbackGeneric>(&RegistryDispatcher::NtCreateKey)};

  static const IPCCall open_params = {
      {IpcTag::NTOPENKEY, {WCHAR_TYPE, UINT32_TYPE, VOIDPTR_TYPE, UINT32_TYPE}},
      reinterpret_cast<CallbackGeneric>(&RegistryDispatcher::NtOpenKey)};

  ipc_calls_.push_back(create_params);
  ipc_calls_.push_back(open_params);
}

bool RegistryDispatcher::SetupService(InterceptionManager* manager,
                                      IpcTag service) {
  if (IpcTag::NTCREATEKEY == service)
    return INTERCEPT_NT(manager, NtCreateKey, CREATE_KEY_ID, 32);

  if (IpcTag::NTOPENKEY == service) {
    bool result = INTERCEPT_NT(manager, NtOpenKey, OPEN_KEY_ID, 16);
    result &= INTERCEPT_NT(manager, NtOpenKeyEx, OPEN_KEY_EX_ID, 20);
    return result;
  }

  return false;
}

bool RegistryDispatcher::NtCreateKey(IPCInfo* ipc,
                                     std::wstring* name,
                                     uint32_t attributes,
                                     HANDLE root,
                                     uint32_t desired_access,
                                     uint32_t title_index,
                                     uint32_t create_options) {
  base::win::ScopedHandle root_handle;
  std::wstring real_path = *name;

  // If there is a root directory, we need to duplicate the handle to make
  // it valid in this process.
  if (root) {
    if (!::DuplicateHandle(ipc->client_info->process, root,
                           ::GetCurrentProcess(), &root, 0, false,
                           DUPLICATE_SAME_ACCESS))
      return false;

    root_handle.Set(root);
  }

  if (!GetCompletePath(root, *name, &real_path))
    return false;

  const wchar_t* regname = real_path.c_str();
  CountedParameterSet<OpenKey> params;
  params[OpenKey::NAME] = ParamPickerMake(regname);
  params[OpenKey::ACCESS] = ParamPickerMake(desired_access);

  EvalResult result =
      policy_base_->EvalPolicy(IpcTag::NTCREATEKEY, params.GetBase());

  HANDLE handle;
  NTSTATUS nt_status;
  ULONG disposition = 0;
  if (!RegistryPolicy::CreateKeyAction(
          result, *ipc->client_info, *name, attributes, root, desired_access,
          title_index, create_options, &handle, &nt_status, &disposition)) {
    ipc->return_info.nt_status = STATUS_ACCESS_DENIED;
    return true;
  }

  // Return operation status on the IPC.
  ipc->return_info.extended[0].unsigned_int = disposition;
  ipc->return_info.nt_status = nt_status;
  ipc->return_info.handle = handle;
  return true;
}

bool RegistryDispatcher::NtOpenKey(IPCInfo* ipc,
                                   std::wstring* name,
                                   uint32_t attributes,
                                   HANDLE root,
                                   uint32_t desired_access) {
  base::win::ScopedHandle root_handle;
  std::wstring real_path = *name;

  // If there is a root directory, we need to duplicate the handle to make
  // it valid in this process.
  if (root) {
    if (!::DuplicateHandle(ipc->client_info->process, root,
                           ::GetCurrentProcess(), &root, 0, false,
                           DUPLICATE_SAME_ACCESS))
      return false;
    root_handle.Set(root);
  }

  if (!GetCompletePath(root, *name, &real_path))
    return false;

  const wchar_t* regname = real_path.c_str();
  CountedParameterSet<OpenKey> params;
  params[OpenKey::NAME] = ParamPickerMake(regname);
  params[OpenKey::ACCESS] = ParamPickerMake(desired_access);

  EvalResult result =
      policy_base_->EvalPolicy(IpcTag::NTOPENKEY, params.GetBase());
  HANDLE handle;
  NTSTATUS nt_status;
  if (!RegistryPolicy::OpenKeyAction(result, *ipc->client_info, *name,
                                     attributes, root, desired_access, &handle,
                                     &nt_status)) {
    ipc->return_info.nt_status = STATUS_ACCESS_DENIED;
    return true;
  }

  // Return operation status on the IPC.
  ipc->return_info.nt_status = nt_status;
  ipc->return_info.handle = handle;
  return true;
}

}  // namespace sandbox
