// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/linux/bpf_cros_nvidia_gpu_policy_linux.h"

// Define these so that unistd.h pulls in needed symbols.
#if !defined(__ARCH_WANT_SYSCALL_NO_AT) || \
    !defined(__ARCH_WANT_SYSCALL_DEPRECATED)
#define __ARCH_WANT_SYSCALL_NO_AT
#define __ARCH_WANT_SYSCALL_DEPRECATED
#endif
#include <unistd.h>

#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"

using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::ResultExpr;

namespace sandbox::policy {

CrosNvidiaGpuProcessPolicy::CrosNvidiaGpuProcessPolicy() = default;

CrosNvidiaGpuProcessPolicy::~CrosNvidiaGpuProcessPolicy() = default;

ResultExpr CrosNvidiaGpuProcessPolicy::EvaluateSyscall(int sysno) const {
  switch (sysno) {
    case __NR_sched_setscheduler:
      return Allow();
    default:
      return GpuProcessPolicy::EvaluateSyscall(sysno);
  }
}

}  // namespace sandbox::policy
