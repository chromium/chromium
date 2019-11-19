// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_SANDBOX_POLICY_DIAGNOSTIC_H_
#define SANDBOX_WIN_SRC_SANDBOX_POLICY_DIAGNOSTIC_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/values.h"
#include "sandbox/win/src/policy_low_level.h"
#include "sandbox/win/src/process_mitigations.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/security_level.h"
#include "sandbox/win/src/sid.h"

namespace sandbox {

class PolicyBase;

// Intended to rhyme with TargetPolicy, may eventually share a common base
// with a configuration holding class (i.e. this class will extend with dynamic
// members such as the |process_ids_| list.)
class PolicyDiagnostic final : public PolicyInfo {
 public:
  // This should quickly copy what it needs from PolicyBase.
  PolicyDiagnostic(PolicyBase* policy);
  ~PolicyDiagnostic() override;
  const char* JsonString() override;

 private:
  // |json_string_| is lazily constructed.
  std::unique_ptr<std::string> json_string_;
  std::vector<uint32_t> process_ids_;
  TokenLevel lockdown_level_ = USER_LAST;
  JobLevel job_level_ = JOB_NONE;
  IntegrityLevel desired_integrity_level_ = INTEGRITY_LEVEL_LAST;
  MitigationFlags desired_mitigations_ = 0;
  std::unique_ptr<Sid> app_container_sid_ = nullptr;
  std::unique_ptr<Sid> lowbox_sid_ = nullptr;
  std::unique_ptr<PolicyGlobal> policy_rules_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(PolicyDiagnostic);
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_SANDBOX_POLICY_DIAGNOSTIC_H_
