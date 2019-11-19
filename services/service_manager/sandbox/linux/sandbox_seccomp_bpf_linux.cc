// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/sandbox/linux/sandbox_seccomp_bpf_linux.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/bpf_dsl/trap_registry.h"
#include "sandbox/sandbox_buildflags.h"
#include "services/service_manager/sandbox/sandbox_type.h"
#include "services/service_manager/sandbox/switches.h"

#if BUILDFLAG(USE_SECCOMP_BPF)

#include "base/files/scoped_file.h"
#include "base/posix/eintr_wrapper.h"
#include "sandbox/linux/seccomp-bpf-helpers/baseline_policy.h"
#include "sandbox/linux/seccomp-bpf-helpers/sigsys_handlers.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_parameters_restrictions.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_sets.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"
#include "services/service_manager/sandbox/linux/bpf_audio_policy_linux.h"
#include "services/service_manager/sandbox/linux/bpf_base_policy_linux.h"
#include "services/service_manager/sandbox/linux/bpf_cdm_policy_linux.h"
#include "services/service_manager/sandbox/linux/bpf_cros_amd_gpu_policy_linux.h"
#include "services/service_manager/sandbox/linux/bpf_cros_arm_gpu_policy_linux.h"
#include "services/service_manager/sandbox/linux/bpf_gpu_policy_linux.h"
#include "services/service_manager/sandbox/linux/bpf_network_policy_linux.h"
#include "services/service_manager/sandbox/linux/bpf_pdf_compositor_policy_linux.h"
#include "services/service_manager/sandbox/linux/bpf_ppapi_policy_linux.h"
#include "services/service_manager/sandbox/linux/bpf_renderer_policy_linux.h"
#include "services/service_manager/sandbox/linux/bpf_utility_policy_linux.h"

#if !defined(OS_NACL_NONSFI)
#include "services/service_manager/sandbox/chromecast_sandbox_whitelist_buildflags.h"
#endif  // !defined(OS_NACL_NONSFI)

#if defined(OS_CHROMEOS)
#include "services/service_manager/sandbox/linux/bpf_ime_policy_linux.h"
#endif  // defined(OS_CHROMEOS)

using sandbox::BaselinePolicy;
using sandbox::SandboxBPF;
using sandbox::SyscallSets;
using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::ResultExpr;

#else  // BUILDFLAG(USE_SECCOMP_BPF)

// Make sure that seccomp-bpf does not get disabled by mistake. Also make sure
// that we think twice about this when adding a new architecture.
#if !defined(ARCH_CPU_ARM64) && !defined(ARCH_CPU_MIPS64EL)
#error "Seccomp-bpf disabled on supported architecture!"
#endif  // !defined(ARCH_CPU_ARM64) && !defined(ARCH_CPU_MIPS64EL)

#endif  // BUILDFLAG(USE_SECCOMP_BPF)

namespace service_manager {

#if BUILDFLAG(USE_SECCOMP_BPF)
namespace {

#if !defined(OS_NACL_NONSFI)

// nacl_helper needs to be tiny and includes only part of content/
// in its dependencies. Make sure to not link things that are not needed.
#if !defined(IN_NACL_HELPER)
inline bool IsChromeOS() {
#if defined(OS_CHROMEOS)
  return true;
#else
  return false;
#endif
}

inline bool UseChromecastSandboxWhitelist() {
#if BUILDFLAG(ENABLE_CHROMECAST_GPU_SANDBOX_WHITELIST)
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
  if (IsChromeOS() || UseChromecastSandboxWhitelist()) {
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
#endif  // USE_SECCOMP_BPF
  return false;
}

bool SandboxSeccompBPF::SupportsSandbox() {
#if BUILDFLAG(USE_SECCOMP_BPF)
  return SandboxBPF::SupportsSeccompSandbox(
      SandboxBPF::SeccompLevel::SINGLE_THREADED);
#endif
  return false;
}

#if !defined(OS_NACL_NONSFI)

bool SandboxSeccompBPF::SupportsSandboxWithTsync() {
#if BUILDFLAG(USE_SECCOMP_BPF)
  return SandboxBPF::SupportsSeccompSandbox(
      SandboxBPF::SeccompLevel::MULTI_THREADED);
#endif
  return false;
}

std::unique_ptr<BPFBasePolicy> SandboxSeccompBPF::PolicyForSandboxType(
    SandboxType sandbox_type,
    const SandboxSeccompBPF::Options& options) {
  switch (sandbox_type) {
    case SANDBOX_TYPE_GPU:
      return GetGpuProcessSandbox(options.use_amd_specific_policies);
    case SANDBOX_TYPE_RENDERER:
      return std::make_unique<RendererProcessPolicy>();
    case SANDBOX_TYPE_PPAPI:
      return std::make_unique<PpapiProcessPolicy>();
    case SANDBOX_TYPE_UTILITY:
    case SANDBOX_TYPE_PROFILING:
      return std::make_unique<UtilityProcessPolicy>();
    case SANDBOX_TYPE_CDM:
      return std::make_unique<CdmProcessPolicy>();
    case SANDBOX_TYPE_PDF_COMPOSITOR:
      return std::make_unique<PdfCompositorProcessPolicy>();
    case SANDBOX_TYPE_NETWORK:
      return std::make_unique<NetworkProcessPolicy>();
    case SANDBOX_TYPE_AUDIO:
      return std::make_unique<AudioProcessPolicy>();
#if defined(OS_CHROMEOS)
    case SANDBOX_TYPE_IME:
      return std::make_unique<ImeProcessPolicy>();
#endif  // defined(OS_CHROMEOS)
    case SANDBOX_TYPE_NO_SANDBOX:
    default:
      NOTREACHED();
      return nullptr;
  }
}

// If a BPF policy is engaged for |process_type|, run a few sanity checks.
void SandboxSeccompBPF::RunSandboxSanityChecks(
    SandboxType sandbox_type,
    const SandboxSeccompBPF::Options& options) {
  switch (sandbox_type) {
    case SANDBOX_TYPE_RENDERER:
    case SANDBOX_TYPE_GPU:
    case SANDBOX_TYPE_PPAPI:
    case SANDBOX_TYPE_PDF_COMPOSITOR:
    case SANDBOX_TYPE_CDM: {
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
      CHECK_EQ(BPFBasePolicy::GetFSDeniedErrno(), errno);

      // We should never allow the creation of netlink sockets.
      syscall_ret = socket(AF_NETLINK, SOCK_DGRAM, 0);
      CHECK_EQ(-1, syscall_ret);
      CHECK_EQ(EPERM, errno);
#endif  // !defined(NDEBUG)
    } break;
    default:
      // Otherwise, no checks required.
      break;
  }
}
#endif  // !defined(OS_NACL_NONSFI)

bool SandboxSeccompBPF::StartSandboxWithExternalPolicy(
    std::unique_ptr<sandbox::bpf_dsl::Policy> policy,
    base::ScopedFD proc_fd,
    sandbox::SandboxBPF::SeccompLevel seccomp_level) {
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
    CHECK(sandbox.StartSandbox(seccomp_level));
    return true;
  }
#endif  // BUILDFLAG(USE_SECCOMP_BPF)
  return false;
}

#if !defined(OS_NACL_NONSFI)
std::unique_ptr<sandbox::bpf_dsl::Policy>
SandboxSeccompBPF::GetBaselinePolicy() {
#if BUILDFLAG(USE_SECCOMP_BPF)
  return std::make_unique<BaselinePolicy>();
#else
  return nullptr;
#endif  // BUILDFLAG(USE_SECCOMP_BPF)
}
#endif  // !defined(OS_NACL_NONSFI)

}  // namespace service_manager
