// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_POLICY_LINUX_BPF_CROS_ARM_GPU_POLICY_LINUX_H_
#define SANDBOX_POLICY_LINUX_BPF_CROS_ARM_GPU_POLICY_LINUX_H_

#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/policy/export.h"
#include "sandbox/policy/linux/bpf_gpu_policy_linux.h"

namespace sandbox {
namespace policy {

// This policy is for Chrome OS ARM.
class SANDBOX_POLICY_EXPORT CrosArmGpuProcessPolicy : public GpuProcessPolicy {
 public:
  explicit CrosArmGpuProcessPolicy(bool allow_shmat);

  CrosArmGpuProcessPolicy(const CrosArmGpuProcessPolicy&) = delete;
  CrosArmGpuProcessPolicy& operator=(const CrosArmGpuProcessPolicy&) = delete;

  ~CrosArmGpuProcessPolicy() override;

  bpf_dsl::ResultExpr EvaluateSyscall(int system_call_number) const override;

 private:
#if defined(__arm__) || defined(__aarch64__)
  const bool allow_shmat_;  // Allow shmat(2).
#endif
};

}  // namespace policy
}  // namespace sandbox

#endif  // SANDBOX_POLICY_LINUX_BPF_CROS_ARM_GPU_POLICY_LINUX_H_
