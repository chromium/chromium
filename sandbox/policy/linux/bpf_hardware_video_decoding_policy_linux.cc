// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/linux/bpf_hardware_video_decoding_policy_linux.h"

#include "base/command_line.h"
#include "media/gpu/buildflags.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_sets.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"
#include "sandbox/policy/linux/bpf_cros_amd_gpu_policy_linux.h"
#include "sandbox/policy/linux/bpf_cros_arm_gpu_policy_linux.h"
#include "sandbox/policy/linux/bpf_gpu_policy_linux.h"
#include "sandbox/policy/linux/sandbox_linux.h"
#include "sandbox/policy/switches.h"

using sandbox::bpf_dsl::ResultExpr;

namespace sandbox::policy {

// static
HardwareVideoDecodingProcessPolicy::PolicyType
HardwareVideoDecodingProcessPolicy::ComputePolicyType(
    bool use_amd_specific_policies) {
  // TODO(b/210759684): the policy type computation is currently based on the
  // GPU. In reality, we should base this on the video decoding hardware. This
  // is good enough on ChromeOS but may be not good enough for a Linux system
  // with multiple GPUs.
#if BUILDFLAG(USE_VAAPI)
  return use_amd_specific_policies ? PolicyType::kVaapiOnAMD
                                   : PolicyType::kVaapiOnIntel;
#elif BUILDFLAG(USE_V4L2_CODEC)
  return PolicyType::kV4L2;
#else
  // TODO(b/195769334): the hardware video decoding sandbox is really only
  // useful when building with VA-API or V4L2 (otherwise, we're not really doing
  // hardware video decoding). Consider restricting the kHardwareVideoDecoding
  // sandbox type to exist only in those configurations so that the
  // HardwareVideoDecodingProcessPolicy is only compiled in those scenarios. As
  // it is now, kHardwareVideoDecoding exists for all ash-chrome builds because
  // chrome/browser/ash/arc/video/gpu_arc_video_service_host.cc depends on it
  // and that file is built for ash-chrome regardless of VA-API/V4L2. That means
  // that bots like linux-chromeos-rel end up compiling this policy.
  CHECK(false);
  return PolicyType::kVaapiOnIntel;
#endif
}

HardwareVideoDecodingProcessPolicy::HardwareVideoDecodingProcessPolicy(
    PolicyType policy_type)
    : policy_type_(policy_type) {
  switch (policy_type_) {
    case PolicyType::kVaapiOnIntel:
      gpu_process_policy_ = std::make_unique<GpuProcessPolicy>();
      break;
    case PolicyType::kVaapiOnAMD:
      gpu_process_policy_ = std::make_unique<CrosAmdGpuProcessPolicy>();
      break;
    case PolicyType::kV4L2:
      gpu_process_policy_ = std::make_unique<CrosArmGpuProcessPolicy>(
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kGpuSandboxAllowSysVShm));
      break;
  }
}

HardwareVideoDecodingProcessPolicy::~HardwareVideoDecodingProcessPolicy() =
    default;

ResultExpr HardwareVideoDecodingProcessPolicy::EvaluateSyscall(
    int system_call_number) const {
  switch (policy_type_) {
    case PolicyType::kVaapiOnIntel:
    case PolicyType::kVaapiOnAMD:
    case PolicyType::kV4L2:
      return gpu_process_policy_->EvaluateSyscall(system_call_number);
  }
}

}  // namespace sandbox::policy
