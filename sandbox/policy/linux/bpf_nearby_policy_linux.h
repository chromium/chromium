// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_POLICY_LINUX_BPF_NEARBY_POLICY_LINUX_H_
#define SANDBOX_POLICY_LINUX_BPF_NEARBY_POLICY_LINUX_H_

#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/policy/export.h"
#include "sandbox/policy/linux/bpf_base_policy_linux.h"

namespace sandbox::policy {

// This policy is be used by Nearby utility processes.
// It is a minimal policy designed to allow the necessary socket operations for
// Nearby transfer mediums to establish connections between devices.
class SANDBOX_POLICY_EXPORT NearbyProcessPolicy : public BPFBasePolicy {
 public:
  NearbyProcessPolicy();

  NearbyProcessPolicy(const NearbyProcessPolicy&) = delete;
  NearbyProcessPolicy& operator=(const NearbyProcessPolicy&) = delete;

  ~NearbyProcessPolicy() override;

  bpf_dsl::ResultExpr EvaluateSyscall(int system_call_number) const override;
};

}  // namespace sandbox::policy

#endif  // SANDBOX_POLICY_LINUX_BPF_NEARBY_POLICY_LINUX_H_
