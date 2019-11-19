// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/sandbox/linux/bpf_base_policy_linux.h"

#include <errno.h>

#include "base/logging.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/seccomp-bpf-helpers/baseline_policy.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"

using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::ResultExpr;

namespace service_manager {

namespace {

// The errno used for denied file system access system calls, such as open(2).
static const int kFSDeniedErrno = EPERM;

}  // namespace.

BPFBasePolicy::BPFBasePolicy()
    : baseline_policy_(new sandbox::BaselinePolicy(kFSDeniedErrno)) {}
BPFBasePolicy::~BPFBasePolicy() {}

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

}  // namespace service_manager.
