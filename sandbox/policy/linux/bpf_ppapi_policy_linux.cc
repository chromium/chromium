// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/linux/bpf_ppapi_policy_linux.h"

#include <errno.h>

#include "build/build_config.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_parameters_restrictions.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_sets.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"
#include "sandbox/policy/linux/sandbox_linux.h"

using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::Error;
using sandbox::bpf_dsl::ResultExpr;

namespace sandbox {
namespace policy {

PpapiProcessPolicy::PpapiProcessPolicy() {}
PpapiProcessPolicy::~PpapiProcessPolicy() {}

ResultExpr PpapiProcessPolicy::EvaluateSyscall(int sysno) const {
  switch (sysno) {
    // TODO(jln): restrict prctl.
    case __NR_prctl:
    case __NR_pwrite64:
    case __NR_sched_get_priority_max:
    case __NR_sched_get_priority_min:
    case __NR_sysinfo:
    case __NR_times:
      return Allow();
    case __NR_ioctl:
      return Error(ENOTTY);  // Flash Access.
    default:
      // Default on the baseline policy.
      return BPFBasePolicy::EvaluateSyscall(sysno);
  }
}

}  // namespace policy
}  // namespace sandbox
