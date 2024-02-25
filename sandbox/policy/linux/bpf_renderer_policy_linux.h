// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_POLICY_LINUX_BPF_RENDERER_POLICY_LINUX_H_
#define SANDBOX_POLICY_LINUX_BPF_RENDERER_POLICY_LINUX_H_

#include "build/build_config.h"
#include "sandbox/policy/export.h"
#include "sandbox/policy/linux/bpf_base_policy_linux.h"

#if BUILDFLAG(IS_ANDROID)
#include "sandbox/linux/seccomp-bpf-helpers/baseline_policy_android.h"
#endif

namespace sandbox::policy {

// This policy can be used by both renderer and worker processes.
class SANDBOX_POLICY_EXPORT RendererProcessPolicy : public BPFBasePolicy {
 public:
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  RendererProcessPolicy();
#elif BUILDFLAG(IS_ANDROID)
  explicit RendererProcessPolicy(
      const BaselinePolicyAndroid::RuntimeOptions& options);
#endif

  RendererProcessPolicy(const RendererProcessPolicy&) = delete;
  RendererProcessPolicy& operator=(const RendererProcessPolicy&) = delete;

  ~RendererProcessPolicy() override;

  bpf_dsl::ResultExpr EvaluateSyscall(int system_call_number) const override;
};

}  // namespace sandbox::policy

#endif  // SANDBOX_POLICY_LINUX_BPF_RENDERER_POLICY_LINUX_H_
