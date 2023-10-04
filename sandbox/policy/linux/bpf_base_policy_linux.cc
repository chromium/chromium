// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/linux/bpf_base_policy_linux.h"

#include <errno.h>

#include "base/check.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/seccomp-bpf-helpers/baseline_policy.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"

#if BUILDFLAG(IS_ANDROID)
#include "sandbox/linux/seccomp-bpf-helpers/baseline_policy_android.h"
#endif

using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::ResultExpr;

namespace sandbox {
namespace policy {

namespace {

// The errno used for denied file system access system calls, such as open(2).
static const int kFSDeniedErrno = EPERM;

}  // namespace.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
BPFBasePolicy::BPFBasePolicy()
    : baseline_policy_(std::make_unique<BaselinePolicy>(kFSDeniedErrno)) {}
#elif BUILDFLAG(IS_ANDROID)
BPFBasePolicy::BPFBasePolicy(
    const BaselinePolicyAndroid::RuntimeOptions& options)
    : baseline_policy_(std::make_unique<BaselinePolicyAndroid>(options)) {}
#endif
BPFBasePolicy::~BPFBasePolicy() = default;

ResultExpr BPFBasePolicy::EvaluateSyscall(int system_call_number) const {
  DCHECK(baseline_policy_);

  // set_robust_list(2) is part of the futex(2) infrastructure.
  // Chrome on Linux/Chrome OS will call set_robust_list(2) frequently.
  // The baseline policy will EPERM set_robust_list(2), but on systems with
  // SECCOMP logs enabled in auditd this will cause a ton of logspam.
  // If we're not blocking the entire futex(2) infrastructure, we should allow
  // set_robust_list(2) and quiet the logspam.
  if (system_call_number == __NR_set_robust_list) {
    return Allow();
  }

  return baseline_policy_->EvaluateSyscall(system_call_number);
}

ResultExpr BPFBasePolicy::InvalidSyscall() const {
  DCHECK(baseline_policy_);
  return baseline_policy_->InvalidSyscall();
}

int BPFBasePolicy::GetFSDeniedErrno() {
  return kFSDeniedErrno;
}

}  // namespace policy
}  // namespace sandbox.
