// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_POLICY_LINUX_BPF_CROS_NVIDIA_GPU_POLICY_LINUX_H_
#define SANDBOX_POLICY_LINUX_BPF_CROS_NVIDIA_GPU_POLICY_LINUX_H_

#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/policy/export.h"
#include "sandbox/policy/linux/bpf_gpu_policy_linux.h"

namespace sandbox::policy {

// This policy is for ChromeOS running on Nvidia GPUs.
class SANDBOX_POLICY_EXPORT CrosNvidiaGpuProcessPolicy
    : public GpuProcessPolicy {
 public:
  CrosNvidiaGpuProcessPolicy();

  CrosNvidiaGpuProcessPolicy(const CrosNvidiaGpuProcessPolicy&) = delete;
  CrosNvidiaGpuProcessPolicy& operator=(const CrosNvidiaGpuProcessPolicy&) =
      delete;

  ~CrosNvidiaGpuProcessPolicy() override;

  bpf_dsl::ResultExpr EvaluateSyscall(int system_call_number) const override;
};

}  // namespace sandbox::policy

#endif  // SANDBOX_POLICY_LINUX_BPF_CROS_NVIDIA_GPU_POLICY_LINUX_H_
