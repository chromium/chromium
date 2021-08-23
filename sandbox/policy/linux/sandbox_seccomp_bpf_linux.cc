// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/linux/sandbox_seccomp_bpf_linux.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/bpf_dsl/trap_registry.h"
#include "sandbox/policy/sandbox_type.h"
#include "sandbox/policy/switches.h"
#include "sandbox/sandbox_buildflags.h"

#if BUILDFLAG(USE_SECCOMP_BPF)

#include "base/files/scoped_file.h"
#include "base/posix/eintr_wrapper.h"
#include "sandbox/linux/seccomp-bpf-helpers/baseline_policy.h"
#include "sandbox/linux/seccomp-bpf-helpers/sigsys_handlers.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_parameters_restrictions.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_sets.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"
#include "sandbox/policy/linux/bpf_audio_policy_linux.h"
#include "sandbox/policy/linux/bpf_base_policy_linux.h"
#include "sandbox/policy/linux/bpf_cdm_policy_linux.h"
#include "sandbox/policy/linux/bpf_cros_amd_gpu_policy_linux.h"
#include "sandbox/policy/linux/bpf_cros_arm_gpu_policy_linux.h"
#include "sandbox/policy/linux/bpf_gpu_policy_linux.h"
#include "sandbox/policy/linux/bpf_network_policy_linux.h"
#include "sandbox/policy/linux/bpf_ppapi_policy_linux.h"
#include "sandbox/policy/linux/bpf_print_backend_policy_linux.h"
#include "sandbox/policy/linux/bpf_print_compositor_policy_linux.h"
#include "sandbox/policy/linux/bpf_renderer_policy_linux.h"
#include "sandbox/policy/linux/bpf_service_policy_linux.h"
#include "sandbox/policy/linux/bpf_speech_recognition_policy_linux.h"
#include "sandbox/policy/linux/bpf_utility_policy_linux.h"

#if !defined(OS_NACL_NONSFI)
#include "sandbox/policy/chromecast_sandbox_allowlist_buildflags.h"
#endif  // !defined(OS_NACL_NONSFI)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "sandbox/policy/features.h"
#include "sandbox/policy/linux/bpf_ime_policy_linux.h"
#include "sandbox/policy/linux/bpf_tts_policy_linux.h"

#include "chromeos/assistant/buildflags.h"
#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#include "sandbox/policy/linux/bpf_libassistant_policy_linux.h"
#endif  // BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::ResultExpr;

#else  // BUILDFLAG(USE_SECCOMP_BPF)

// Make sure that seccomp-bpf does not get disabled by mistake. Also make sure
// that we think twice about this when adding a new architecture.
#if !defined(ARCH_CPU_ARM64) && !defined(ARCH_CPU_MIPS64EL)
#error "Seccomp-bpf disabled on supported architecture!"
#endif  // !defined(ARCH_CPU_ARM64) && !defined(ARCH_CPU_MIPS64EL)

#endif  // BUILDFLAG(USE_SECCOMP_BPF)

namespace sandbox {
namespace policy {

#if BUILDFLAG(USE_SECCOMP_BPF)
namespace {

#if !defined(OS_NACL_NONSFI)

// nacl_helper needs to be tiny and includes only part of content/
// in its dependencies. Make sure to not link things that are not needed.
#if !defined(IN_NACL_HELPER)
inline bool IsChromeOS() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return true;
#else
  return false;
#endif
}

inline bool UseChromecastSandboxAllowlist() {
#if BUILDFLAG(ENABLE_CHROMECAST_GPU_SANDBOX_ALLOWLIST)
  return true;
#else
  return false;
#endif
}

inline bool IsArchitectureArm() {
#if defined(ARCH_CPU_ARM_FAMILY)
  return true;
#else
  return false;
#endif
}

std::unique_ptr<BPFBasePolicy> GetGpuProcessSandbox(
    bool use_amd_specific_policies) {
  if (IsChromeOS() || UseChromecastSandboxAllowlist()) {
    if (IsArchitectureArm()) {
      return std::make_unique<CrosArmGpuProcessPolicy>(
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kGpuSandboxAllowSysVShm));
    }
    if (use_amd_specific_policies)
      return std::make_unique<CrosAmdGpuProcessPolicy>();
  }
  return std::make_unique<GpuProcessPolicy>();
}
#endif  // !defined(IN_NACL_HELPER)
#endif  // !defined(OS_NACL_NONSFI)

}  // namespace

#endif  // USE_SECCOMP_BPF

// Is seccomp BPF globally enabled?
bool SandboxSeccompBPF::IsSeccompBPFDesired() {
#if BUILDFLAG(USE_SECCOMP_BPF)
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  return !command_line.HasSwitch(switches::kNoSandbox) &&
         !command_line.HasSwitch(switches::kDisableSeccompFilterSandbox);
#else
  return false;
#endif  // USE_SECCOMP_BPF
}

bool SandboxSeccompBPF::SupportsSandbox() {
#if BUILDFLAG(USE_SECCOMP_BPF)
  return SandboxBPF::SupportsSeccompSandbox(
      SandboxBPF::SeccompLevel::SINGLE_THREADED);
#else
  return false;
#endif
}

#if !defined(OS_NACL_NONSFI)

bool SandboxSeccompBPF::SupportsSandboxWithTsync() {
#if BUILDFLAG(USE_SECCOMP_BPF)
  return SandboxBPF::SupportsSeccompSandbox(
      SandboxBPF::SeccompLevel::MULTI_THREADED);
#else
  return false;
#endif
}

std::unique_ptr<BPFBasePolicy> SandboxSeccompBPF::PolicyForSandboxType(
    SandboxType sandbox_type,
    const SandboxSeccompBPF::Options& options) {
  switch (sandbox_type) {
    case SandboxType::kGpu:
      return GetGpuProcessSandbox(options.use_amd_specific_policies);
    case SandboxType::kRenderer:
      return std::make_unique<RendererProcessPolicy>();
    case SandboxType::kPpapi:
      return std::make_unique<PpapiProcessPolicy>();
    case SandboxType::kUtility:
      return std::make_unique<UtilityProcessPolicy>();
    case SandboxType::kCdm:
      return std::make_unique<CdmProcessPolicy>();
    case SandboxType::kPrintCompositor:
      return std::make_unique<PrintCompositorProcessPolicy>();
#if BUILDFLAG(ENABLE_PRINTING)
    case SandboxType::kPrintBackend:
      return std::make_unique<PrintBackendProcessPolicy>();
#endif
    case SandboxType::kNetwork:
      return std::make_unique<NetworkProcessPolicy>();
    case SandboxType::kAudio:
      return std::make_unique<AudioProcessPolicy>();
    case SandboxType::kService:
      return std::make_unique<ServiceProcessPolicy>();
    case SandboxType::kSpeechRecognition:
      return std::make_unique<SpeechRecognitionProcessPolicy>();
#if BUILDFLAG(IS_CHROMEOS_ASH)
    case SandboxType::kIme:
      return std::make_unique<ImeProcessPolicy>();
    case SandboxType::kTts:
      return std::make_unique<TtsProcessPolicy>();
#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
    case SandboxType::kLibassistant:
      return std::make_unique<LibassistantProcessPolicy>();
#endif  // BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    case SandboxType::kZygoteIntermediateSandbox:
    case SandboxType::kNoSandbox:
      NOTREACHED();
      return nullptr;
  }
}

// If a BPF policy is engaged for |process_type|, run a few sanity checks.
void SandboxSeccompBPF::RunSandboxSanityChecks(
    SandboxType sandbox_type,
    const SandboxSeccompBPF::Options& options) {
  switch (sandbox_type) {
    case SandboxType::kRenderer:
    case SandboxType::kGpu:
    case SandboxType::kPpapi:
    case SandboxType::kPrintCompositor:
    case SandboxType::kCdm: {
      int syscall_ret;
      errno = 0;

      // Without the sandbox, this would EBADF.
      syscall_ret = fchmod(-1, 07777);
      CHECK_EQ(-1, syscall_ret);
      CHECK_EQ(EPERM, errno);

// Run most of the sanity checks only in DEBUG mode to avoid a perf.
// impact.
#if !defined(NDEBUG)
      // open() must be restricted.
      syscall_ret = open("/etc/passwd", O_RDONLY);
      CHECK_EQ(-1, syscall_ret);
      // The broker used with the GPU process sandbox uses EACCES for
      // invalid filesystem access. See crbug.com/1233028 for more info.
      CHECK_EQ(sandbox_type == SandboxType::kGpu
                   ? EACCES
                   : BPFBasePolicy::GetFSDeniedErrno(),
               errno);

      // We should never allow the creation of netlink sockets.
      syscall_ret = socket(AF_NETLINK, SOCK_DGRAM, 0);
      CHECK_EQ(-1, syscall_ret);
      CHECK_EQ(EPERM, errno);
#endif  // !defined(NDEBUG)
    } break;
#if BUILDFLAG(IS_CHROMEOS_ASH)
    case SandboxType::kIme:
    case SandboxType::kTts:
#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
    case SandboxType::kLibassistant:
#endif  // BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    case SandboxType::kAudio:
    case SandboxType::kService:
    case SandboxType::kSpeechRecognition:
    case SandboxType::kNetwork:
#if BUILDFLAG(ENABLE_PRINTING)
    case SandboxType::kPrintBackend:
#endif
    case SandboxType::kUtility:
    case SandboxType::kNoSandbox:
    case SandboxType::kZygoteIntermediateSandbox:
      // Otherwise, no checks required.
      break;
  }
}
#endif  // !defined(OS_NACL_NONSFI)

bool SandboxSeccompBPF::StartSandboxWithExternalPolicy(
    std::unique_ptr<bpf_dsl::Policy> policy,
    base::ScopedFD proc_fd,
    SandboxBPF::SeccompLevel seccomp_level) {
#if BUILDFLAG(USE_SECCOMP_BPF)
  if (IsSeccompBPFDesired() && SupportsSandbox()) {
    CHECK(policy);
    // Starting the sandbox is a one-way operation. The kernel doesn't allow
    // us to unload a sandbox policy after it has been started. Nonetheless,
    // in order to make the use of the "Sandbox" object easier, we allow for
    // the object to be destroyed after the sandbox has been started. Note that
    // doing so does not stop the sandbox.
    SandboxBPF sandbox(std::move(policy));
    sandbox.SetProcFd(std::move(proc_fd));
    bool enable_ibpb = true;
#if BUILDFLAG(IS_CHROMEOS_ASH)
    enable_ibpb =
        base::FeatureList::IsEnabled(
            features::kForceSpectreVariant2Mitigation) ||
        base::FeatureList::IsEnabled(features::kSpectreVariant2Mitigation);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    CHECK(sandbox.StartSandbox(seccomp_level, enable_ibpb));
    return true;
  }
#endif  // BUILDFLAG(USE_SECCOMP_BPF)
  return false;
}

#if !defined(OS_NACL_NONSFI)
std::unique_ptr<bpf_dsl::Policy> SandboxSeccompBPF::GetBaselinePolicy() {
#if BUILDFLAG(USE_SECCOMP_BPF)
  return std::make_unique<BaselinePolicy>();
#else
  return nullptr;
#endif  // BUILDFLAG(USE_SECCOMP_BPF)
}
#endif  // !defined(OS_NACL_NONSFI)

}  // namespace policy
}  // namespace sandbox
