// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/seccomp-bpf-helpers/seccomp_starter_android.h"

#include <signal.h>

#include "base/logging.h"

#if BUILDFLAG(USE_SECCOMP_BPF)
#include "base/android/build_info.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#endif

namespace sandbox {

SeccompStarterAndroid::SeccompStarterAndroid(int build_sdk)
    : sdk_int_(build_sdk) {}

SeccompStarterAndroid::~SeccompStarterAndroid() = default;

#if BUILDFLAG(USE_SECCOMP_BPF)
BaselinePolicyAndroid::RuntimeOptions
SeccompStarterAndroid::GetDefaultBaselineOptions() const {
  BaselinePolicyAndroid::RuntimeOptions options;
  // On Android S+, there are CTS-enforced requirements that the kernel carries
  // patches to userfaultfd that enforce usermode pages only (i.e.
  // UFFD_USER_MODE_ONLY). Userfaultfd is used for a new ART garbage collector.
  // See https://crbug.com/1300653 for details.
  options.allow_userfaultfd_ioctls = sdk_int_ >= base::android::SDK_VERSION_S;
  return options;
}
#endif

bool SeccompStarterAndroid::StartSandbox() {
#if BUILDFLAG(USE_SECCOMP_BPF)
  DCHECK(policy_);

  // Do run-time detection to ensure that support is present.
  if (!SandboxBPF::SupportsSeccompSandbox(
          SandboxBPF::SeccompLevel::MULTI_THREADED)) {
    status_ = SeccompSandboxStatus::DETECTION_FAILED;
    LOG(WARNING) << "Seccomp support should be present, but detection "
                 << "failed. Continuing without Seccomp-BPF.";
    return false;
  }

  sig_t old_handler = signal(SIGSYS, SIG_DFL);
  if (old_handler != SIG_DFL && sdk_int_ < base::android::SDK_VERSION_OREO) {
    // On Android O and later, the zygote applies a seccomp filter to all
    // apps. It has its own SIGSYS handler that must be un-hooked so that
    // the Chromium one can be used instead. If pre-O devices have a SIGSYS
    // handler, then warn about that.
    DLOG(WARNING) << "Un-hooking existing SIGSYS handler before starting "
                  << "Seccomp sandbox";
  }

  SandboxBPF sandbox(std::move(policy_));
  CHECK(sandbox.StartSandbox(SandboxBPF::SeccompLevel::MULTI_THREADED));
  status_ = SeccompSandboxStatus::ENGAGED;
  return true;
#else
  return false;
#endif
}

}  // namespace sandbox
