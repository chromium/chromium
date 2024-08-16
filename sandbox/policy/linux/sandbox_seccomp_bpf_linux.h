// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_POLICY_LINUX_SANDBOX_SECCOMP_BPF_LINUX_H_
#define SANDBOX_POLICY_LINUX_SANDBOX_SECCOMP_BPF_LINUX_H_

#include <memory>

#include "base/files/scoped_file.h"
#include "base/functional/callback.h"
#include "build/build_config.h"
#include "sandbox/linux/bpf_dsl/policy.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/policy/export.h"
#include "sandbox/policy/linux/bpf_base_policy_linux.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"

namespace sandbox {
namespace policy {

// This class has two main sets of APIs. One can be used to start the sandbox
// for internal content process types, the other is indirectly exposed as
// a public content/ API and uses a supplied policy.
class SANDBOX_POLICY_EXPORT SandboxSeccompBPF {
 public:
  struct Options {
    bool use_amd_specific_policies = false;     // For ChromiumOS.
    bool use_intel_specific_policies = false;   // For ChromiumOS.
    bool use_virtio_specific_policies = false;  // For ChromiumOS VM.
    bool use_nvidia_specific_policies = false;  // For Linux.

    // Options for GPU's PreSandboxHook.
    bool accelerated_video_decode_enabled = false;
    bool accelerated_video_encode_enabled = false;
  };

  SandboxSeccompBPF() = delete;
  SandboxSeccompBPF(const SandboxSeccompBPF&) = delete;
  SandboxSeccompBPF& operator=(const SandboxSeccompBPF&) = delete;

  // This is the API to enable a seccomp-bpf sandbox for content/
  // process-types:
  // Is the sandbox globally enabled, can anything use it at all ?
  // This looks at global command line flags to see if the sandbox
  // should be enabled at all.
  static bool IsSeccompBPFDesired();

  // Check if the kernel supports seccomp-bpf.
  static bool SupportsSandbox();

  // Check if the kernel supports TSYNC (thread synchronization) with seccomp.
  static bool SupportsSandboxWithTsync();

  // Return a policy suitable for use with StartSandboxWithExternalPolicy.
  static std::unique_ptr<BPFBasePolicy> PolicyForSandboxType(
      sandbox::mojom::Sandbox sandbox_type,
      const SandboxSeccompBPF::Options& options);

  // Prove that the sandbox was engaged by the StartSandbox() call. Crashes
  // the process if the sandbox failed to engage.
  static void RunSandboxSanityChecks(sandbox::mojom::Sandbox sandbox_type,
                                     const SandboxSeccompBPF::Options& options);

  // This is the API to enable a seccomp-bpf sandbox by using an
  // external policy.
  // If `force_disable_spectre_variant2_mitigation` is true, the Spectre variant
  // 2 mitigation will be disabled.
  static bool StartSandboxWithExternalPolicy(
      std::unique_ptr<bpf_dsl::Policy> policy,
      base::ScopedFD proc_fd,
      SandboxBPF::SeccompLevel seccomp_level =
          SandboxBPF::SeccompLevel::SINGLE_THREADED,
      bool force_disable_spectre_variant2_mitigation = false);

  // The "baseline" policy can be a useful base to build a sandbox policy.
  static std::unique_ptr<bpf_dsl::Policy> GetBaselinePolicy();
};

}  // namespace policy
}  // namespace sandbox

#endif  // SANDBOX_POLICY_LINUX_SANDBOX_SECCOMP_BPF_LINUX_H_
