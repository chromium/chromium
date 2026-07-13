// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_POLICY_LINUX_BPF_HARDWARE_VIDEO_DECODING_POLICY_LINUX_H_
#define SANDBOX_POLICY_LINUX_BPF_HARDWARE_VIDEO_DECODING_POLICY_LINUX_H_

#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/policy/export.h"
#include "sandbox/policy/linux/bpf_base_policy_linux.h"

namespace sandbox::policy {

// Policy used to sandbox utility processes that perform hardware video decoding
// on behalf of untrusted clients (Chrome renderer processes or ARC++/ARCVM).
//
// When making changes to this policy, ensure that you do not give access to
// privileged APIs (APIs that would allow these utility process to access data
// that's not explicitly shared with them through Mojo). For example, hardware
// video decoding processes should NEVER have access to /dev/dri/card* (the DRM
// master device).
class SANDBOX_POLICY_EXPORT HardwareVideoDecodingProcessPolicy
    : public BPFBasePolicy {
 public:
  enum class PolicyType {
    kVaapiOnIntel,
    kVaapiOnAMD,
    kV4L2,
    // Used when both VA-API and V4L2 are compiled in. The actual backend is
    // selected at runtime via the kPreferV4L2VideoAcceleration feature, so the
    // sandbox grants the union of permissions/seccomp filters required by
    // either. The VA-API half mirrors the single-backend split: kVaapiAndV4L2
    // pairs V4L2 with the Intel VA-API policy, kVaapiOnAMDAndV4L2 pairs it with
    // the AMD VA-API policy. Which one applies is decided from the GPU at
    // sandbox setup time (see ComputePolicyType()).
    kVaapiAndV4L2,
    kVaapiOnAMDAndV4L2,
  };
  static PolicyType ComputePolicyType(bool use_amd_specific_policies);

  explicit HardwareVideoDecodingProcessPolicy(PolicyType policy_type);
  HardwareVideoDecodingProcessPolicy(
      const HardwareVideoDecodingProcessPolicy&) = delete;
  HardwareVideoDecodingProcessPolicy& operator=(
      const HardwareVideoDecodingProcessPolicy&) = delete;
  ~HardwareVideoDecodingProcessPolicy() override = default;

  // sandbox::bpf_dsl::Policy implementation.
  bpf_dsl::ResultExpr EvaluateSyscall(int system_call_number) const override;

 private:
  bpf_dsl::ResultExpr EvaluateSyscallForVaapiOnIntel(
      int system_call_number) const;
  bpf_dsl::ResultExpr EvaluateSyscallForVaapiOnAMD(
      int system_call_number) const;
  bpf_dsl::ResultExpr EvaluateSyscallForV4L2(int system_call_number) const;
  bpf_dsl::ResultExpr EvaluateSyscallForVaapiAndV4L2(
      int system_call_number) const;

  const PolicyType policy_type_;
};

}  // namespace sandbox::policy

#endif  // SANDBOX_POLICY_LINUX_BPF_HARDWARE_VIDEO_DECODING_POLICY_LINUX_H_
