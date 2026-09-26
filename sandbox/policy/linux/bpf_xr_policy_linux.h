// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_POLICY_LINUX_BPF_XR_POLICY_LINUX_H_
#define SANDBOX_POLICY_LINUX_BPF_XR_POLICY_LINUX_H_

#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/policy/export.h"
#include "sandbox/policy/linux/bpf_gpu_policy_linux.h"

namespace sandbox {
namespace policy {

// Seccomp-bpf policy for the XR Device Service (kXrCompositing) on Linux: the
// GPU policy plus the AF_UNIX socket syscalls the runtime needs to reach its
// compositor (socket() restricted to AF_UNIX, as AudioProcessPolicy does).
class SANDBOX_POLICY_EXPORT XrProcessPolicy : public GpuProcessPolicy {
 public:
  XrProcessPolicy();

  XrProcessPolicy(const XrProcessPolicy&) = delete;
  XrProcessPolicy& operator=(const XrProcessPolicy&) = delete;

  ~XrProcessPolicy() override;

  bpf_dsl::ResultExpr EvaluateSyscall(int system_call_number) const override;
};

}  // namespace policy
}  // namespace sandbox

#endif  // SANDBOX_POLICY_LINUX_BPF_XR_POLICY_LINUX_H_
