// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/linux/bpf_hardware_video_decoding_policy_linux.h"

#include <linux/kcmp.h>

#include "media/gpu/buildflags.h"
#include "sandbox/linux/seccomp-bpf-helpers/sigsys_handlers.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_parameters_restrictions.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_sets.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"
#include "sandbox/policy/linux/sandbox_linux.h"

using sandbox::bpf_dsl::AllOf;
using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::Arg;
using sandbox::bpf_dsl::Error;
using sandbox::bpf_dsl::If;
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
    : policy_type_(policy_type) {}

ResultExpr HardwareVideoDecodingProcessPolicy::EvaluateSyscall(
    int system_call_number) const {
  switch (policy_type_) {
    case PolicyType::kVaapiOnIntel:
      return EvaluateSyscallForVaapiOnIntel(system_call_number);
    case PolicyType::kVaapiOnAMD:
      return EvaluateSyscallForVaapiOnAMD(system_call_number);
    case PolicyType::kV4L2:
      return EvaluateSyscallForV4L2(system_call_number);
  }
}

ResultExpr HardwareVideoDecodingProcessPolicy::EvaluateSyscallForVaapiOnIntel(
    int system_call_number) const {
  if (SyscallSets::IsTruncate(system_call_number)) {
    // Explicitly disallow ftruncate()/truncate() to eliminate the possibility
    // that a video decoder process can change the size of a file (including,
    // e.g., a dma-buf).
    return CrashSIGSYS();
  }

  if (system_call_number == __NR_ioctl)
    return Allow();

  auto* sandbox_linux = SandboxLinux::GetInstance();
  if (sandbox_linux->ShouldBrokerHandleSyscall(system_call_number))
    return sandbox_linux->HandleViaBroker(system_call_number);

  return BPFBasePolicy::EvaluateSyscall(system_call_number);
}

ResultExpr HardwareVideoDecodingProcessPolicy::EvaluateSyscallForVaapiOnAMD(
    int system_call_number) const {
  if (SyscallSets::IsTruncate(system_call_number)) {
    // Explicitly disallow ftruncate()/truncate() to eliminate the possibility
    // that a video decoder process can change the size of a file (including,
    // e.g., a dma-buf).
    return CrashSIGSYS();
  }

  switch (system_call_number) {
    case __NR_getdents64:
    case __NR_ioctl:
    case __NR_sysinfo:
    case __NR_sched_setscheduler:
      return Allow();
    case __NR_sched_setaffinity:
      return RestrictSchedTarget(GetPolicyPid(), system_call_number);
    case __NR_kcmp: {
      const Arg<pid_t> pid1(0);
      const Arg<pid_t> pid2(1);
      const Arg<int> type(2);
      const pid_t policy_pid = GetPolicyPid();
      // Only allowed when comparing file handles for the calling thread.
      return If(AllOf(pid1 == policy_pid, pid2 == policy_pid,
                      type == KCMP_FILE),
                Allow())
          .Else(Error(EPERM));
    }
  }

  auto* sandbox_linux = SandboxLinux::GetInstance();
  if (sandbox_linux->ShouldBrokerHandleSyscall(system_call_number))
    return sandbox_linux->HandleViaBroker(system_call_number);

  return BPFBasePolicy::EvaluateSyscall(system_call_number);
}

ResultExpr HardwareVideoDecodingProcessPolicy::EvaluateSyscallForV4L2(
    int system_call_number) const {
  if (SyscallSets::IsTruncate(system_call_number)) {
    // Explicitly disallow ftruncate()/truncate() to eliminate the possibility
    // that a video decoder process can change the size of a file (including,
    // e.g., a dma-buf).
    return CrashSIGSYS();
  }

  if (system_call_number == __NR_ioctl)
    return Allow();

  if (system_call_number == __NR_sched_setaffinity) {
    return RestrictSchedTarget(GetPolicyPid(), system_call_number);
  }

  auto* sandbox_linux = SandboxLinux::GetInstance();
  if (sandbox_linux->ShouldBrokerHandleSyscall(system_call_number))
    return sandbox_linux->HandleViaBroker(system_call_number);

  return BPFBasePolicy::EvaluateSyscall(system_call_number);
}

}  // namespace sandbox::policy
