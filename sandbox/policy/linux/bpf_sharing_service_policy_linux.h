// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_POLICY_LINUX_BPF_SHARING_SERVICE_POLICY_LINUX_H_
#define SANDBOX_POLICY_LINUX_BPF_SHARING_SERVICE_POLICY_LINUX_H_

#include "base/macros.h"
#include "sandbox/policy/linux/bpf_base_policy_linux.h"

namespace sandbox {
namespace policy {

// This policy can be used by the Sharing service to host WebRTC.
class SharingServiceProcessPolicy : public BPFBasePolicy {
 public:
  SharingServiceProcessPolicy() = default;
  ~SharingServiceProcessPolicy() override = default;

  bpf_dsl::ResultExpr EvaluateSyscall(int system_call_number) const override;

  SharingServiceProcessPolicy(const SharingServiceProcessPolicy&) = delete;
  SharingServiceProcessPolicy& operator=(const SharingServiceProcessPolicy&) =
      delete;
};

}  // namespace policy
}  // namespace sandbox

#endif  // SANDBOX_POLICY_LINUX_BPF_SHARING_SERVICE_POLICY_LINUX_H_
