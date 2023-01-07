// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_POLICY_LINUX_BPF_SCREEN_AI_POLICY_LINUX_H_
#define SANDBOX_POLICY_LINUX_BPF_SCREEN_AI_POLICY_LINUX_H_

#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/policy/linux/bpf_base_policy_linux.h"

namespace sandbox::policy {

// The process policy for the sandboxed utility process that loads the Screen AI
// on-device library.
class SANDBOX_POLICY_EXPORT ScreenAIProcessPolicy : public BPFBasePolicy {
 public:
  ScreenAIProcessPolicy();

  ScreenAIProcessPolicy(const ScreenAIProcessPolicy&) = delete;
  ScreenAIProcessPolicy& operator=(const ScreenAIProcessPolicy&) = delete;

  ~ScreenAIProcessPolicy() override;

  bpf_dsl::ResultExpr EvaluateSyscall(int system_call_number) const override;
};

}  // namespace sandbox::policy

#endif  // SANDBOX_POLICY_LINUX_BPF_SCREEN_AI_POLICY_LINUX_H_
