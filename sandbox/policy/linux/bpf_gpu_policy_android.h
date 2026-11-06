// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_POLICY_LINUX_BPF_GPU_POLICY_ANDROID_H_
#define SANDBOX_POLICY_LINUX_BPF_GPU_POLICY_ANDROID_H_

#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/bpf_dsl/policy.h"
#include "sandbox/policy/export.h"

namespace sandbox::policy {

// Seccomp-BPF policy for the Android GPU process.
class SANDBOX_POLICY_EXPORT GpuProcessPolicyAndroid : public bpf_dsl::Policy {
 public:
  GpuProcessPolicyAndroid();

  GpuProcessPolicyAndroid(const GpuProcessPolicyAndroid&) = delete;
  GpuProcessPolicyAndroid& operator=(const GpuProcessPolicyAndroid&) = delete;

  ~GpuProcessPolicyAndroid() override;

  // bpf_dsl::Policy:
  bpf_dsl::ResultExpr EvaluateSyscall(int system_call_number) const override;
};

}  // namespace sandbox::policy

#endif  // SANDBOX_POLICY_LINUX_BPF_GPU_POLICY_ANDROID_H_
