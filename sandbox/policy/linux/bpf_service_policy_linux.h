// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_POLICY_LINUX_BPF_SERVICE_POLICY_LINUX_H_
#define SANDBOX_POLICY_LINUX_BPF_SERVICE_POLICY_LINUX_H_

#include "sandbox/policy/linux/bpf_base_policy_linux.h"

namespace sandbox {
namespace policy {

// This policy can be used by isolated utilities such as the Sharing
// service to host WebRTC, and isolated javascript worklets to host
// jitless javascript. Resources should be provided via mojo.
// Consider UtilityProcessPolicy if this is too restrictive.
class ServiceProcessPolicy : public BPFBasePolicy {
 public:
  ServiceProcessPolicy() = default;
  ~ServiceProcessPolicy() override = default;

  bpf_dsl::ResultExpr EvaluateSyscall(int system_call_number) const override;

  ServiceProcessPolicy(const ServiceProcessPolicy&) = delete;
  ServiceProcessPolicy& operator=(const ServiceProcessPolicy&) = delete;
};

}  // namespace policy
}  // namespace sandbox

#endif  // SANDBOX_POLICY_LINUX_BPF_SERVICE_POLICY_LINUX_H_
