// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_NAMED_PIPE_POLICY_H__
#define SANDBOX_SRC_NAMED_PIPE_POLICY_H__

#include <string>

#include "sandbox/win/src/crosscall_server.h"
#include "sandbox/win/src/policy_low_level.h"
#include "sandbox/win/src/sandbox_policy.h"

namespace sandbox {

// This class centralizes most of the knowledge related to named pipe creation.
class NamedPipePolicy {
 public:
  // Creates the required low-level policy rules to evaluate a high-level.
  // policy rule for named pipe creation
  // 'name' is the named pipe to be created
  // 'semantics' is the desired semantics.
  // 'policy' is the policy generator to which the rules are going to be added.
  static bool GenerateRules(const wchar_t* name,
                            TargetPolicy::Semantics semantics,
                            LowLevelPolicy* policy);

  // Processes a 'CreateNamedPipeW()' request from the target.
  static DWORD CreateNamedPipeAction(EvalResult eval_result,
                                     const ClientInfo& client_info,
                                     const std::wstring& name,
                                     DWORD open_mode,
                                     DWORD pipe_mode,
                                     DWORD max_instances,
                                     DWORD out_buffer_size,
                                     DWORD in_buffer_size,
                                     DWORD default_timeout,
                                     HANDLE* pipe);
};

}  // namespace sandbox

#endif  // SANDBOX_SRC_NAMED_PIPE_POLICY_H__
