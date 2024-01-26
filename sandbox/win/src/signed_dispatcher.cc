// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/signed_dispatcher.h"

#include <stdint.h>

#include <string>

#include "base/strings/string_util.h"
#include "base/win/scoped_handle.h"
#include "sandbox/win/src/crosscall_client.h"
#include "sandbox/win/src/interception.h"
#include "sandbox/win/src/interceptors.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/policy_params.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/signed_interception.h"
#include "sandbox/win/src/signed_policy.h"
#include "sandbox/win/src/win_utils.h"

namespace sandbox {

SignedDispatcher::SignedDispatcher(PolicyBase* policy_base)
    : policy_base_(policy_base) {
  static const IPCCall create_params = {
      {IpcTag::NTCREATESECTION, {VOIDPTR_TYPE}},
      reinterpret_cast<CallbackGeneric>(&SignedDispatcher::CreateSection)};

  ipc_calls_.push_back(create_params);
}

bool SignedDispatcher::SetupService(InterceptionManager* manager,
                                    IpcTag service) {
  if (service == IpcTag::NTCREATESECTION)
    return INTERCEPT_NT(manager, NtCreateSection, CREATE_SECTION_ID, 32);
  return false;
}

bool SignedDispatcher::CreateSection(IPCInfo* ipc, HANDLE file_handle) {
  // Duplicate input handle from target to broker.
  HANDLE local_file_handle = nullptr;
  if (!::DuplicateHandle((*ipc->client_info).process, file_handle,
                         ::GetCurrentProcess(), &local_file_handle,
                         FILE_MAP_EXECUTE, false, 0)) {
    return false;
  }

  base::win::ScopedHandle local_handle(local_file_handle);
  auto path = GetPathFromHandle(local_handle.get());
  if (!path)
    return false;
  const wchar_t* module_name = path->c_str();
  CountedParameterSet<NameBased> params;
  params[NameBased::NAME] = ParamPickerMake(module_name);

  EvalResult result =
      policy_base_->EvalPolicy(IpcTag::NTCREATESECTION, params.GetBase());

  // Return operation status on the IPC.
  HANDLE section_handle = nullptr;
  ipc->return_info.nt_status = SignedPolicy::CreateSectionAction(
      result, *ipc->client_info, local_handle, &section_handle);
  ipc->return_info.handle = section_handle;
  return true;
}

}  // namespace sandbox
