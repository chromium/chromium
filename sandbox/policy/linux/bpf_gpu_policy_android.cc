// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/linux/bpf_gpu_policy_android.h"

#include <sys/mman.h>

#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/seccomp-bpf-helpers/sigsys_handlers.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"

using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::Arg;
using sandbox::bpf_dsl::If;
using sandbox::bpf_dsl::ResultExpr;

namespace sandbox::policy {

GpuProcessPolicyAndroid::GpuProcessPolicyAndroid() = default;

GpuProcessPolicyAndroid::~GpuProcessPolicyAndroid() = default;

ResultExpr GpuProcessPolicyAndroid::EvaluateSyscall(int sysno) const {
  switch (sysno) {
#if defined(__NR_mmap)
    case __NR_mmap:
#endif
#if defined(__NR_mmap2)
    case __NR_mmap2:
#endif
    case __NR_mprotect:
#if defined(__NR_pkey_mprotect)
    case __NR_pkey_mprotect:
#endif
    {
      const Arg<int> prot(2);
      return If((prot & PROT_EXEC) == 0, Allow()).Else(CrashSIGSYS());
    }
    default:
      return Allow();
  }
}

}  // namespace sandbox::policy
