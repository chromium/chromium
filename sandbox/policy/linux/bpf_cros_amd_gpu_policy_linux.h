// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_POLICY_LINUX_BPF_CROS_AMD_GPU_POLICY_LINUX_H_
#define SANDBOX_POLICY_LINUX_BPF_CROS_AMD_GPU_POLICY_LINUX_H_

#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/policy/export.h"
#include "sandbox/policy/linux/bpf_gpu_policy_linux.h"

namespace sandbox {
namespace policy {

// This policy is for AMD GPUs running on Chrome OS.
class SANDBOX_POLICY_EXPORT CrosAmdGpuProcessPolicy : public GpuProcessPolicy {
 public:
  CrosAmdGpuProcessPolicy();

  CrosAmdGpuProcessPolicy(const CrosAmdGpuProcessPolicy&) = delete;
  CrosAmdGpuProcessPolicy& operator=(const CrosAmdGpuProcessPolicy&) = delete;

  ~CrosAmdGpuProcessPolicy() override;

  bpf_dsl::ResultExpr EvaluateSyscall(int system_call_number) const override;
};

}  // namespace policy
}  // namespace sandbox

#endif  // SANDBOX_POLICY_LINUX_BPF_CROS_AMD_GPU_POLICY_LINUX_H_
