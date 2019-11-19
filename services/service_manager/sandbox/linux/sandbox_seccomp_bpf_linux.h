// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_SANDBOX_LINUX_SANDBOX_SECCOMP_BPF_LINUX_H_
#define SERVICES_SERVICE_MANAGER_SANDBOX_LINUX_SANDBOX_SECCOMP_BPF_LINUX_H_

#include <memory>

#include "base/callback.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "sandbox/linux/bpf_dsl/policy.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "services/service_manager/sandbox/export.h"
#include "services/service_manager/sandbox/linux/bpf_base_policy_linux.h"
#include "services/service_manager/sandbox/sandbox_type.h"

namespace service_manager {

// This class has two main sets of APIs. One can be used to start the sandbox
// for internal content process types, the other is indirectly exposed as
// a public content/ API and uses a supplied policy.
class SERVICE_MANAGER_SANDBOX_EXPORT SandboxSeccompBPF {
 public:
  struct Options {
    bool use_amd_specific_policies = false;  // For ChromiumOS.

    // Options for GPU's PreSandboxHook.
    bool accelerated_video_decode_enabled = false;
    bool accelerated_video_encode_enabled = false;
  };

  // This is the API to enable a seccomp-bpf sandbox for content/
  // process-types:
  // Is the sandbox globally enabled, can anything use it at all ?
  // This looks at global command line flags to see if the sandbox
  // should be enabled at all.
  static bool IsSeccompBPFDesired();

  // Check if the kernel supports seccomp-bpf.
  static bool SupportsSandbox();

#if !defined(OS_NACL_NONSFI)
  // Check if the kernel supports TSYNC (thread synchronization) with seccomp.
  static bool SupportsSandboxWithTsync();

  // Return a policy suitable for use with StartSandboxWithExternalPolicy.
  static std::unique_ptr<BPFBasePolicy> PolicyForSandboxType(
      SandboxType sandbox_type,
      const SandboxSeccompBPF::Options& options);

  // Prove that the sandbox was engaged by the StartSandbox() call. Crashes
  // the process if the sandbox failed to engage.
  static void RunSandboxSanityChecks(SandboxType sandbox_type,
                                     const SandboxSeccompBPF::Options& options);
#endif  // !defined(OS_NACL_NONSFI)

  // This is the API to enable a seccomp-bpf sandbox by using an
  // external policy.
  static bool StartSandboxWithExternalPolicy(
      std::unique_ptr<sandbox::bpf_dsl::Policy> policy,
      base::ScopedFD proc_fd,
      sandbox::SandboxBPF::SeccompLevel seccomp_level =
          sandbox::SandboxBPF::SeccompLevel::SINGLE_THREADED);

  // The "baseline" policy can be a useful base to build a sandbox policy.
  static std::unique_ptr<sandbox::bpf_dsl::Policy> GetBaselinePolicy();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(SandboxSeccompBPF);
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_SANDBOX_LINUX_SANDBOX_SECCOMP_BPF_LINUX_H_
