// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_SIGNED_POLICY_H_
#define SANDBOX_WIN_SRC_SIGNED_POLICY_H_

#include "base/files/file_path.h"
#include "base/win/scoped_handle.h"
#include "sandbox/win/src/policy_low_level.h"

namespace sandbox {

// This class centralizes most of the knowledge related to signed policy
class SignedPolicy {
 public:
  // Creates the required low-level policy rules to evaluate a high-level
  // policy rule. Note - dll_path should be an exact path. Returns a handle
  // to the created section object that will need to be shared with the new
  // process for the interception to function.
  static base::win::ScopedHandle GenerateRules(base::FilePath dll_path,
                                               LowLevelPolicy* policy);
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_SIGNED_POLICY_H_
