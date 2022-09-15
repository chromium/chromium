// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_POLICY_LINUX_BPF_RENDERER_POLICY_LINUX_H_
#define SANDBOX_POLICY_LINUX_BPF_RENDERER_POLICY_LINUX_H_

#include "sandbox/policy/linux/bpf_base_policy_linux.h"

namespace sandbox {
namespace policy {

// This policy can be used by both renderer and worker processes.
class RendererProcessPolicy : public BPFBasePolicy {
 public:
  RendererProcessPolicy();

  RendererProcessPolicy(const RendererProcessPolicy&) = delete;
  RendererProcessPolicy& operator=(const RendererProcessPolicy&) = delete;

  ~RendererProcessPolicy() override;

  bpf_dsl::ResultExpr EvaluateSyscall(int system_call_number) const override;
};

}  // namespace policy
}  // namespace sandbox

#endif  // SANDBOX_POLICY_LINUX_BPF_RENDERER_POLICY_LINUX_H_
