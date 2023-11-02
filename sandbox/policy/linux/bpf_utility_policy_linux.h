// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_POLICY_LINUX_BPF_UTILITY_POLICY_LINUX_H_
#define SANDBOX_POLICY_LINUX_BPF_UTILITY_POLICY_LINUX_H_

#include "sandbox/policy/linux/bpf_base_policy_linux.h"

namespace sandbox {
namespace policy {

// This policy can be used by utility processes.
class UtilityProcessPolicy : public BPFBasePolicy {
 public:
  UtilityProcessPolicy();

  UtilityProcessPolicy(const UtilityProcessPolicy&) = delete;
  UtilityProcessPolicy& operator=(const UtilityProcessPolicy&) = delete;

  ~UtilityProcessPolicy() override;

  bpf_dsl::ResultExpr EvaluateSyscall(int system_call_number) const override;
};

}  // namespace policy
}  // namespace sandbox

#endif  // SANDBOX_POLICY_LINUX_BPF_UTILITY_POLICY_LINUX_H_
