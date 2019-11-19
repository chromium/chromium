// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_REGISTRY_POLICY_H__
#define SANDBOX_SRC_REGISTRY_POLICY_H__

#include <stdint.h>

#include <string>

#include "sandbox/win/src/crosscall_server.h"
#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/policy_low_level.h"
#include "sandbox/win/src/sandbox_policy.h"

namespace sandbox {

// This class centralizes most of the knowledge related to registry policy
class RegistryPolicy {
 public:
  // Creates the required low-level policy rules to evaluate a high-level
  // policy rule for registry IO, in particular open or create actions.
  static bool GenerateRules(const wchar_t* name,
                            TargetPolicy::Semantics semantics,
                            LowLevelPolicy* policy);

  // Performs the desired policy action on a create request with an
  // API that is compatible with the IPC-received parameters.
  static bool CreateKeyAction(EvalResult eval_result,
                              const ClientInfo& client_info,
                              const std::wstring& key,
                              uint32_t attributes,
                              HANDLE root_directory,
                              uint32_t desired_access,
                              uint32_t title_index,
                              uint32_t create_options,
                              HANDLE* handle,
                              NTSTATUS* nt_status,
                              ULONG* disposition);

  // Performs the desired policy action on an open request with an
  // API that is compatible with the IPC-received parameters.
  static bool OpenKeyAction(EvalResult eval_result,
                            const ClientInfo& client_info,
                            const std::wstring& key,
                            uint32_t attributes,
                            HANDLE root_directory,
                            uint32_t desired_access,
                            HANDLE* handle,
                            NTSTATUS* nt_status);
};

}  // namespace sandbox

#endif  // SANDBOX_SRC_REGISTRY_POLICY_H__
