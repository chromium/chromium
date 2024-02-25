// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_SIGNED_POLICY_H_
#define SANDBOX_WIN_SRC_SIGNED_POLICY_H_

#include <stdint.h>

#include "base/win/scoped_handle.h"
#include "sandbox/win/src/crosscall_server.h"
#include "sandbox/win/src/policy_engine_opcodes.h"
#include "sandbox/win/src/policy_low_level.h"

namespace sandbox {

// This class centralizes most of the knowledge related to signed policy
class SignedPolicy {
 public:
  // Creates the required low-level policy rules to evaluate a high-level
  // policy rule.
  static bool GenerateRules(const wchar_t* name,
                            LowLevelPolicy* policy);

  // Performs the desired policy action on a request.
  // client_info is the target process that is making the request and
  // eval_result is the desired policy action to accomplish.
  static NTSTATUS CreateSectionAction(
      EvalResult eval_result,
      const ClientInfo& client_info,
      const base::win::ScopedHandle& local_file_handle,
      HANDLE* section_handle);
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_SIGNED_POLICY_H_
