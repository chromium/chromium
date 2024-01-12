// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/signed_policy.h"

#include <ntstatus.h>
#include <stdint.h>

#include <string>

#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/policy_engine_opcodes.h"
#include "sandbox/win/src/policy_params.h"
#include "sandbox/win/src/sandbox_nt_util.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "sandbox/win/src/win_utils.h"

namespace sandbox {

bool SignedPolicy::GenerateRules(const wchar_t* name,
                                 LowLevelPolicy* policy) {
  base::FilePath file_path(name);
  auto nt_path_name = GetNtPathFromWin32Path(file_path.DirName().value());
  if (!nt_path_name)
    return false;

  base::FilePath nt_path(nt_path_name.value());
  std::wstring nt_filename = nt_path.Append(file_path.BaseName()).value();
  // Create a rule to ASK_BROKER if name matches.
  PolicyRule signed_policy(ASK_BROKER);
  if (!signed_policy.AddStringMatch(IF, NameBased::NAME, nt_filename.c_str())) {
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
  // The only action supported is ASK_BROKER which means create the requested
  // section as specified.
  if (ASK_BROKER != eval_result)
    return false;

  HANDLE local_section_handle = nullptr;
  NTSTATUS status = GetNtExports()->CreateSection(
      &local_section_handle,
      SECTION_QUERY | SECTION_MAP_WRITE | SECTION_MAP_READ |
          SECTION_MAP_EXECUTE,
      nullptr, 0, PAGE_EXECUTE, SEC_IMAGE, local_file_handle.get());
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
