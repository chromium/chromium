// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SECCOMP_BPF_HELPERS_SECCOMP_STARTER_ANDROID_H_
#define SANDBOX_LINUX_SECCOMP_BPF_HELPERS_SECCOMP_STARTER_ANDROID_H_

#include "sandbox/sandbox_buildflags.h"
#include "sandbox/sandbox_export.h"

#if BUILDFLAG(USE_SECCOMP_BPF)
#include <memory>

#include "sandbox/linux/bpf_dsl/policy.h"
#include "sandbox/linux/seccomp-bpf-helpers/baseline_policy_android.h"
#endif

namespace sandbox {

namespace bpf_dsl {
class Policy;
}

enum class SeccompSandboxStatus {
  NOT_SUPPORTED = 0,  // Seccomp is not supported.
  DETECTION_FAILED,   // Run-time detection of Seccomp+TSYNC failed.
  FEATURE_DISABLED,   // Sandbox was disabled by FeatureList. Obsolete/unused.
  FEATURE_ENABLED,    // Sandbox was enabled by FeatureList. Obsolete/unused.
  ENGAGED,            // Sandbox was enabled and successfully turned on.
  STATUS_MAX
  // This enum is used by an UMA histogram, so only append values.
};

// This helper class can be used to start a Seccomp-BPF sandbox on Android. It
// helps by doing compile- and run-time checks to see if Seccomp is supported
// on the given device.
class SANDBOX_EXPORT SeccompStarterAndroid {
 public:
  // Constructs a sandbox starter helper. The |build_sdk_int| is used for run-
  // time checks.
  explicit SeccompStarterAndroid(int build_sdk_int);

  SeccompStarterAndroid(const SeccompStarterAndroid&) = delete;
  SeccompStarterAndroid& operator=(const SeccompStarterAndroid&) = delete;

  ~SeccompStarterAndroid();

#if BUILDFLAG(USE_SECCOMP_BPF)
  // Returns the default runtime-configured options for the baseline Android
  // seccomp policy.
  BaselinePolicyAndroid::RuntimeOptions GetDefaultBaselineOptions() const;
#endif

  // Sets the BPF policy to apply. This must be called before StartSandbox()
  // if BUILDFLAG(USE_SECCOMP_BPF) is true.
  void set_policy(std::unique_ptr<bpf_dsl::Policy> policy) {
    policy_ = std::move(policy);
  }

  // Attempts to turn on the seccomp sandbox. Returns true iff the sandbox
  // was started successfully.
  bool StartSandbox();

  // Returns detailed status information about the sandbox. This will only
  // yield an interesting value after StartSandbox() is called.
  SeccompSandboxStatus status() const { return status_; }

 private:
  const int sdk_int_;
  SeccompSandboxStatus status_ = SeccompSandboxStatus::NOT_SUPPORTED;
  std::unique_ptr<bpf_dsl::Policy> policy_;
};

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SECCOMP_BPF_HELPERS_SECCOMP_STARTER_ANDROID_H_
