// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/signed_policy.h"

#include <stdint.h>

#include <string>

#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/policy_engine_opcodes.h"
#include "sandbox/win/src/policy_params.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "sandbox/win/src/win_utils.h"

namespace sandbox {

bool SignedPolicy::GenerateRules(const wchar_t* name,
                                 TargetPolicy::Semantics semantics,
                                 LowLevelPolicy* policy) {
  // Only support one semantic.
  if (TargetPolicy::SIGNED_ALLOW_LOAD != semantics) {
    return false;
  }

  base::FilePath file_path(name);
  std::wstring nt_path_name;
  if (!GetNtPathFromWin32Path(file_path.DirName().value().c_str(),
                              &nt_path_name))
    return false;
  base::FilePath nt_path(nt_path_name);
  std::wstring nt_filename = nt_path.Append(file_path.BaseName()).value();
  // Create a rule to ASK_BROKER if name matches.
  PolicyRule signed_policy(ASK_BROKER);
  if (!signed_policy.AddStringMatch(IF, NameBased::NAME, nt_filename.c_str(),
                                    CASE_INSENSITIVE)) {
    return false;
  }
  if (!policy->AddRule(IpcTag::NTCREATESECTION, &signed_policy)) {
    return false;
  }

  return true;
}

NTSTATUS SignedPolicy::CreateSectionAction(
    EvalResult eval_result,
    const ClientInfo& client_info,
    const base::win::ScopedHandle& local_file_handle,
    HANDLE* target_section_handle) {
  NtCreateSectionFunction NtCreateSection = nullptr;
  ResolveNTFunctionPtr("NtCreateSection", &NtCreateSection);

  // The only action supported is ASK_BROKER which means create the requested
  // section as specified.
  if (ASK_BROKER != eval_result)
    return false;

  HANDLE local_section_handle = nullptr;
  NTSTATUS status = NtCreateSection(&local_section_handle,
                                    SECTION_QUERY | SECTION_MAP_WRITE |
                                        SECTION_MAP_READ | SECTION_MAP_EXECUTE,
                                    nullptr, 0, PAGE_EXECUTE, SEC_IMAGE,
                                    local_file_handle.Get());
  if (!local_section_handle)
    return status;

  // Duplicate section handle back to the target.
  if (!::DuplicateHandle(::GetCurrentProcess(), local_section_handle,
                         client_info.process, target_section_handle, 0, false,
                         DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS)) {
    return STATUS_ACCESS_DENIED;
  }
  return status;
}

}  // namespace sandbox
