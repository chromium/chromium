// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_SANDBOX_POLICY_DIAGNOSTIC_H_
#define SANDBOX_WIN_SRC_SANDBOX_POLICY_DIAGNOSTIC_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include <optional>
#include "base/win/sid.h"
#include "sandbox/win/src/app_container.h"
#include "sandbox/win/src/handle_closer.h"
#include "sandbox/win/src/policy_low_level.h"
#include "sandbox/win/src/process_mitigations.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/security_level.h"

namespace sandbox {

class PolicyBase;

// Intended to rhyme with TargetPolicy, may eventually share a common base
// with a configuration holding class (i.e. this class will extend with dynamic
// members such as the |process_ids_| list.)
class PolicyDiagnostic final : public PolicyInfo {
 public:
  // This should quickly copy what it needs from PolicyBase.
  explicit PolicyDiagnostic(PolicyBase* policy);

  PolicyDiagnostic(const PolicyDiagnostic&) = delete;
  PolicyDiagnostic& operator=(const PolicyDiagnostic&) = delete;

  ~PolicyDiagnostic() override;
  const char* JsonString() override;

 private:
  // |json_string_| is lazily constructed.
  std::unique_ptr<std::string> json_string_;
  uint32_t process_id_;
  TokenLevel lockdown_level_ = USER_LAST;
  JobLevel job_level_ = JobLevel::kUnprotected;
  IntegrityLevel desired_integrity_level_ = INTEGRITY_LEVEL_LAST;
  MitigationFlags desired_mitigations_ = 0;
  std::optional<base::win::Sid> app_container_sid_;
  // Only populated if |app_container_sid_| is present.
  std::vector<base::win::Sid> capabilities_;
  // Only populated if |app_container_sid_| is present.
  std::vector<base::win::Sid> initial_capabilities_;
  AppContainerType app_container_type_ = AppContainerType::kNone;
  std::unique_ptr<PolicyGlobal> policy_rules_;
  // From policy's TopLevelDispatcher.
  std::vector<IpcTag> ipcs_;
  bool is_csrss_connected_ = false;
  bool zero_appshim_ = false;
  HandleCloserConfig handles_to_close_;
  std::string tag_;
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_SANDBOX_POLICY_DIAGNOSTIC_H_
