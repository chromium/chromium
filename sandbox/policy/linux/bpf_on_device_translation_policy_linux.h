// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_POLICY_LINUX_BPF_ON_DEVICE_TRANSLATION_POLICY_LINUX_H_
#define SANDBOX_POLICY_LINUX_BPF_ON_DEVICE_TRANSLATION_POLICY_LINUX_H_

#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/policy/linux/bpf_base_policy_linux.h"

namespace sandbox::policy {

// This policy is be used by OnDeviceTranslation processe.
class SANDBOX_POLICY_EXPORT OnDeviceTranslationProcessPolicy
    : public BPFBasePolicy {
 public:
  OnDeviceTranslationProcessPolicy() = default;
  ~OnDeviceTranslationProcessPolicy() override = default;

  OnDeviceTranslationProcessPolicy(const OnDeviceTranslationProcessPolicy&) =
      delete;
  OnDeviceTranslationProcessPolicy& operator=(
      const OnDeviceTranslationProcessPolicy&) = delete;

  bpf_dsl::ResultExpr EvaluateSyscall(int system_call_number) const override;
};

}  // namespace sandbox::policy

#endif  // SANDBOX_POLICY_LINUX_BPF_ON_DEVICE_TRANSLATION_POLICY_LINUX_H_
