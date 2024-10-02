// Copyright 2012 The Chromium Authors
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
#include "base/feature_list.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/bpf_dsl/trap_registry.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"
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
#include "sandbox/policy/chromecast_sandbox_allowlist_buildflags.h"
#include "sandbox/policy/linux/bpf_audio_policy_linux.h"
#include "sandbox/policy/linux/bpf_base_policy_linux.h"
#include "sandbox/policy/linux/bpf_cdm_policy_linux.h"
#include "sandbox/policy/linux/bpf_cros_amd_gpu_policy_linux.h"
#include "sandbox/policy/linux/bpf_cros_arm_gpu_policy_linux.h"
#include "sandbox/policy/linux/bpf_cros_intel_gpu_policy_linux.h"
#include "sandbox/policy/linux/bpf_cros_nvidia_gpu_policy_linux.h"
#include "sandbox/policy/linux/bpf_cros_virtio_gpu_policy_linux.h"
#include "sandbox/policy/linux/bpf_gpu_policy_linux.h"
#include "sandbox/policy/linux/bpf_network_policy_linux.h"
#include "sandbox/policy/linux/bpf_ppapi_policy_linux.h"
#include "sandbox/policy/linux/bpf_print_backend_policy_linux.h"
#include "sandbox/policy/linux/bpf_print_compositor_policy_linux.h"
#include "sandbox/policy/linux/bpf_renderer_policy_linux.h"
#include "sandbox/policy/linux/bpf_service_policy_linux.h"
#include "sandbox/policy/linux/bpf_speech_recognition_policy_linux.h"
#include "sandbox/policy/linux/bpf_utility_policy_linux.h"

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
#include "sandbox/policy/linux/bpf_screen_ai_policy_linux.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/assistant/buildflags.h"
#include "sandbox/policy/features.h"
#include "sandbox/policy/linux/bpf_ime_policy_linux.h"
#include "sandbox/policy/linux/bpf_nearby_policy_linux.h"
#include "sandbox/policy/linux/bpf_tts_policy_linux.h"
#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#include "sandbox/policy/linux/bpf_libassistant_policy_linux.h"
#endif  // BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH)
#include "sandbox/policy/linux/bpf_hardware_video_decoding_policy_linux.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_LINUX)
#include "sandbox/policy/linux/bpf_on_device_translation_policy_linux.h"
#endif  // BUILDFLAG(IS_LINUX)

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

// nacl_helper needs to be tiny and includes only part of content/
// in its dependencies. Make sure to not link things that are not needed.
#if !defined(IN_NACL_HELPER)
inline bool IsChromeOS() {
#if BUILDFLAG(IS_CHROMEOS)
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
    const SandboxSeccompBPF::Options& options) {
  if (IsChromeOS() || UseChromecastSandboxAllowlist()) {
    if (IsArchitectureArm()) {
      return std::make_unique<CrosArmGpuProcessPolicy>(
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kGpuSandboxAllowSysVShm));
    }
    if (options.use_amd_specific_policies) {
      return std::make_unique<CrosAmdGpuProcessPolicy>();
    }
    if (options.use_intel_specific_policies) {
      return std::make_unique<CrosIntelGpuProcessPolicy>();
    }
    if (options.use_nvidia_specific_policies) {
      return std::make_unique<CrosNvidiaGpuProcessPolicy>();
    }
    if (options.use_virtio_specific_policies) {
      return std::make_unique<CrosVirtIoGpuProcessPolicy>();
    }
  }
  return std::make_unique<GpuProcessPolicy>();
}
#endif  // !defined(IN_NACL_HELPER)

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

bool SandboxSeccompBPF::SupportsSandboxWithTsync() {
#if BUILDFLAG(USE_SECCOMP_BPF)
  return SandboxBPF::SupportsSeccompSandbox(
      SandboxBPF::SeccompLevel::MULTI_THREADED);
#else
  return false;
#endif
}

std::unique_ptr<BPFBasePolicy> SandboxSeccompBPF::PolicyForSandboxType(
    sandbox::mojom::Sandbox sandbox_type,
    const SandboxSeccompBPF::Options& options) {
  switch (sandbox_type) {
    case sandbox::mojom::Sandbox::kGpu:
      return GetGpuProcessSandbox(options);
    case sandbox::mojom::Sandbox::kRenderer:
      return std::make_unique<RendererProcessPolicy>();
#if BUILDFLAG(ENABLE_PPAPI)
    case sandbox::mojom::Sandbox::kPpapi:
      return std::make_unique<PpapiProcessPolicy>();
#endif
    case sandbox::mojom::Sandbox::kOnDeviceModelExecution:
      return GetGpuProcessSandbox(options);
    case sandbox::mojom::Sandbox::kUtility:
      return std::make_unique<UtilityProcessPolicy>();
    case sandbox::mojom::Sandbox::kCdm:
      return std::make_unique<CdmProcessPolicy>();
    case sandbox::mojom::Sandbox::kPrintCompositor:
      return std::make_unique<PrintCompositorProcessPolicy>();
#if BUILDFLAG(ENABLE_OOP_PRINTING)
    case sandbox::mojom::Sandbox::kPrintBackend:
      return std::make_unique<PrintBackendProcessPolicy>();
#endif
    case sandbox::mojom::Sandbox::kNetwork:
      return std::make_unique<NetworkProcessPolicy>();
    case sandbox::mojom::Sandbox::kAudio:
      return std::make_unique<AudioProcessPolicy>();
    case sandbox::mojom::Sandbox::kService:
      return std::make_unique<ServiceProcessPolicy>();
    case sandbox::mojom::Sandbox::kServiceWithJit:
      return std::make_unique<ServiceProcessPolicy>();
    case sandbox::mojom::Sandbox::kSpeechRecognition:
      return std::make_unique<SpeechRecognitionProcessPolicy>();
#if BUILDFLAG(IS_LINUX)
    case sandbox::mojom::Sandbox::kOnDeviceTranslation:
      return std::make_unique<OnDeviceTranslationProcessPolicy>();
#endif  // BUILDFLAG(IS_LINUX)
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
    case sandbox::mojom::Sandbox::kScreenAI:
      return std::make_unique<ScreenAIProcessPolicy>();
#endif
#if BUILDFLAG(IS_LINUX)
    case sandbox::mojom::Sandbox::kVideoEffects:
      return std::make_unique<ServiceProcessPolicy>();
#endif  // BUILDFLAG(IS_LINUX)
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH)
    case sandbox::mojom::Sandbox::kHardwareVideoDecoding:
      return std::make_unique<HardwareVideoDecodingProcessPolicy>(
          HardwareVideoDecodingProcessPolicy::ComputePolicyType(
              options.use_amd_specific_policies));
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH)
    case sandbox::mojom::Sandbox::kHardwareVideoEncoding:
      // TODO(b/255554267): we're using the GPU process sandbox policy for now
      // as a transition step. However, we should create a policy that's tighter
      // just for hardware video encoding.
      return GetGpuProcessSandbox(options);
#if BUILDFLAG(IS_CHROMEOS_ASH)
    case sandbox::mojom::Sandbox::kIme:
      return std::make_unique<ImeProcessPolicy>();
    case sandbox::mojom::Sandbox::kTts:
      return std::make_unique<TtsProcessPolicy>();
    case sandbox::mojom::Sandbox::kNearby:
      return std::make_unique<NearbyProcessPolicy>();
#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
    case sandbox::mojom::Sandbox::kLibassistant:
      return std::make_unique<LibassistantProcessPolicy>();
#endif  // BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    case sandbox::mojom::Sandbox::kZygoteIntermediateSandbox:
    case sandbox::mojom::Sandbox::kNoSandbox:
      NOTREACHED();
  }
}

// If a BPF policy is engaged for |process_type|, run a few sanity checks.
void SandboxSeccompBPF::RunSandboxSanityChecks(
    sandbox::mojom::Sandbox sandbox_type,
    const SandboxSeccompBPF::Options& options) {
  switch (sandbox_type) {
    case sandbox::mojom::Sandbox::kRenderer:
    case sandbox::mojom::Sandbox::kGpu:
#if BUILDFLAG(ENABLE_PPAPI)
    case sandbox::mojom::Sandbox::kPpapi:
#endif
    case sandbox::mojom::Sandbox::kPrintCompositor:
    case sandbox::mojom::Sandbox::kCdm: {
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
      CHECK_EQ(sandbox_type == sandbox::mojom::Sandbox::kGpu
                   ? EACCES
                   : BPFBasePolicy::GetFSDeniedErrno(),
               errno);

      // We should never allow the creation of netlink sockets.
      syscall_ret = socket(AF_NETLINK, SOCK_DGRAM, 0);
      CHECK_EQ(-1, syscall_ret);
      CHECK_EQ(EPERM, errno);
#endif  // !defined(NDEBUG)
    } break;
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH)
    case sandbox::mojom::Sandbox::kHardwareVideoDecoding:
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH)
    case sandbox::mojom::Sandbox::kHardwareVideoEncoding:
#if BUILDFLAG(IS_CHROMEOS_ASH)
    case sandbox::mojom::Sandbox::kIme:
    case sandbox::mojom::Sandbox::kTts:
    case sandbox::mojom::Sandbox::kNearby:
#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
    case sandbox::mojom::Sandbox::kLibassistant:
#endif  // BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
    case sandbox::mojom::Sandbox::kScreenAI:
#endif
    case sandbox::mojom::Sandbox::kAudio:
#if BUILDFLAG(IS_LINUX)
    case sandbox::mojom::Sandbox::kVideoEffects:
#endif  // BUILDFLAG(IS_LINUX)
    case sandbox::mojom::Sandbox::kService:
    case sandbox::mojom::Sandbox::kServiceWithJit:
    case sandbox::mojom::Sandbox::kSpeechRecognition:
    case sandbox::mojom::Sandbox::kNetwork:
#if BUILDFLAG(ENABLE_OOP_PRINTING)
    case sandbox::mojom::Sandbox::kPrintBackend:
#endif
    case sandbox::mojom::Sandbox::kOnDeviceModelExecution:
#if BUILDFLAG(IS_LINUX)
    case sandbox::mojom::Sandbox::kOnDeviceTranslation:
#endif  // BUILDFLAG(IS_LINUX)
    case sandbox::mojom::Sandbox::kUtility:
    case sandbox::mojom::Sandbox::kNoSandbox:
    case sandbox::mojom::Sandbox::kZygoteIntermediateSandbox:
      // Otherwise, no checks required.
      break;
  }
}

bool SandboxSeccompBPF::StartSandboxWithExternalPolicy(
    std::unique_ptr<bpf_dsl::Policy> policy,
    base::ScopedFD proc_fd,
    SandboxBPF::SeccompLevel seccomp_level,
    bool force_disable_spectre_variant2_mitigation) {
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
    if (force_disable_spectre_variant2_mitigation) {
      enable_ibpb = false;
    } else {
      enable_ibpb =
          base::FeatureList::IsEnabled(features::kSpectreVariant2Mitigation);
    }
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
    // On Linux desktop and Lacros, the Spectre variant 2 mitigation is
    // on by default unless force disabled by the caller.
    enable_ibpb = !force_disable_spectre_variant2_mitigation;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    CHECK(sandbox.StartSandbox(seccomp_level, enable_ibpb));
    return true;
  }
#endif  // BUILDFLAG(USE_SECCOMP_BPF)
  return false;
}

std::unique_ptr<bpf_dsl::Policy> SandboxSeccompBPF::GetBaselinePolicy() {
#if BUILDFLAG(USE_SECCOMP_BPF)
  return std::make_unique<BaselinePolicy>();
#else
  return nullptr;
#endif  // BUILDFLAG(USE_SECCOMP_BPF)
}

}  // namespace policy
}  // namespace sandbox
