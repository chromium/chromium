// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/signed_policy.h"

#include <ntstatus.h>
#include <stdint.h>

#include <string>

#include "base/win/security_descriptor.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/policy_engine_opcodes.h"
#include "sandbox/win/src/policy_params.h"
#include "sandbox/win/src/sandbox_nt_util.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "sandbox/win/src/win_utils.h"

namespace sandbox {

base::win::ScopedHandle SignedPolicy::GenerateRules(base::FilePath dll_path,
                                                    LowLevelPolicy* policy) {
  base::win::ScopedHandle file_handle(
      ::CreateFile(dll_path.value().c_str(), FILE_EXECUTE,
                   FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                   FILE_ATTRIBUTE_NORMAL, nullptr));
  if (!file_handle.is_valid()) {
    return {};
  }

  auto nt_filename = GetPathFromHandle(file_handle.get());
  if (!nt_filename) {
    return {};
  }

  auto sd = base::win::SecurityDescriptor::CreateWithEmptyDacl();
  SECURITY_DESCRIPTOR sd_abs = sd.ToAbsolute();
  OBJECT_ATTRIBUTES obj_attr = {};
  InitializeObjectAttributes(&obj_attr, nullptr, OBJ_INHERIT, nullptr, &sd_abs);
  HANDLE local_section_handle = nullptr;
  NTSTATUS status = GetNtExports()->CreateSection(
      &local_section_handle,
      SECTION_QUERY | SECTION_MAP_WRITE | SECTION_MAP_READ |
          SECTION_MAP_EXECUTE,
      &obj_attr, 0, PAGE_EXECUTE, SEC_IMAGE, file_handle.get());

  if (status != STATUS_SUCCESS) {
    return {};
  }

  base::win::ScopedHandle section_handle(local_section_handle);
  // Create a rule to RETURN_CONST the section handle if the name matches.
  PolicyRule signed_policy(RETURN_CONST,
                           reinterpret_cast<uintptr_t>(section_handle.get()));
  if (!signed_policy.AddStringMatch(IF, NameBased::NAME,
                                    nt_filename->c_str())) {
    return {};
  }
  if (!policy->AddRule(IpcTag::NTCREATESECTION, std::move(signed_policy))) {
    return {};
  }

  return section_handle;
}

}  // namespace sandbox
