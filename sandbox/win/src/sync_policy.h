// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_SYNC_POLICY_H__
#define SANDBOX_SRC_SYNC_POLICY_H__

#include <stdint.h>

#include <string>

#include "sandbox/win/src/crosscall_server.h"
#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/policy_low_level.h"
#include "sandbox/win/src/sandbox_policy.h"

namespace sandbox {

// This class centralizes most of the knowledge related to sync policy
class SyncPolicy {
 public:
  // Creates the required low-level policy rules to evaluate a high-level
  // policy rule for sync calls, in particular open or create actions.
  // name is the sync object name, semantics is the desired semantics for the
  // open or create and policy is the policy generator to which the rules are
  // going to be added.
  static bool GenerateRules(const wchar_t* name,
                            TargetPolicy::Semantics semantics,
                            LowLevelPolicy* policy);

  // Performs the desired policy action on a request.
  // client_info is the target process that is making the request and
  // eval_result is the desired policy action to accomplish.
  static NTSTATUS CreateEventAction(EvalResult eval_result,
                                    const ClientInfo& client_info,
                                    const std::wstring& event_name,
                                    uint32_t event_type,
                                    uint32_t initial_state,
                                    HANDLE* handle);
  static NTSTATUS OpenEventAction(EvalResult eval_result,
                                  const ClientInfo& client_info,
                                  const std::wstring& event_name,
                                  uint32_t desired_access,
                                  HANDLE* handle);
};

}  // namespace sandbox

#endif  // SANDBOX_SRC_SYNC_POLICY_H__
