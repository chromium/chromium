// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/linux/bpf_renderer_policy_linux.h"

#include <errno.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>

#include "build/build_config.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/seccomp-bpf-helpers/sigsys_handlers.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_parameters_restrictions.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_sets.h"
#include "sandbox/linux/system_headers/linux_prctl.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"
#include "sandbox/policy/linux/sandbox_linux.h"

// TODO(vignatti): replace the local definitions below with #include
// <linux/dma-buf.h> once kernel version 4.6 becomes widely used.
#include <linux/types.h>

struct local_dma_buf_sync {
  __u64 flags;
};
#define LOCAL_DMA_BUF_BASE 'b'
#define LOCAL_DMA_BUF_IOCTL_SYNC \
  _IOW(LOCAL_DMA_BUF_BASE, 0, struct local_dma_buf_sync)

using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::Arg;
using sandbox::bpf_dsl::Error;
using sandbox::bpf_dsl::If;
using sandbox::bpf_dsl::ResultExpr;

namespace sandbox {
namespace policy {

namespace {

#if !BUILDFLAG(IS_ANDROID)
ResultExpr RestrictIoctl() {
  const Arg<unsigned long> request(1);
  return Switch(request)
      .Cases({static_cast<unsigned long>(TCGETS), FIONREAD,
              static_cast<unsigned long>(LOCAL_DMA_BUF_IOCTL_SYNC)},
             Allow())
      .Default(CrashSIGSYSIoctl());
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace
#if !BUILDFLAG(IS_ANDROID)
RendererProcessPolicy::RendererProcessPolicy() = default;
#else
RendererProcessPolicy::RendererProcessPolicy(
    const BaselinePolicyAndroid::RuntimeOptions& options)
    : BPFBasePolicy(options) {}
#endif  // !BUILDFLAG(IS_ANDROID)
RendererProcessPolicy::~RendererProcessPolicy() = default;

ResultExpr RendererProcessPolicy::EvaluateSyscall(int sysno) const {
  switch (sysno) {
    // The baseline policy allows __NR_clock_gettime. Allow
    // clock_getres() for V8. crbug.com/329053.
    case __NR_clock_getres:
#if defined(__i386__) || defined(__arm__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
    case __NR_clock_getres_time64:
#endif
      return RestrictClockID();
// Android requires a larger set of allowed ioctls, so this case is handled
// through BPFBasePolicy calling through to BaselinePolicyAndroid on Android.
#if !BUILDFLAG(IS_ANDROID)
    case __NR_ioctl:
      return RestrictIoctl();
#endif  // !BUILDFLAG(IS_ANDROID)
    // Allow the system calls below.
    case __NR_fdatasync:
    case __NR_fsync:
    case __NR_ftruncate:
#if defined(__i386__) || defined(__arm__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
    case __NR_ftruncate64:
#endif
#if defined(__i386__) || defined(__x86_64__) || defined(__mips__) || \
    defined(__aarch64__)
    case __NR_getrlimit:
    case __NR_setrlimit:
// We allow setrlimit to dynamically adjust the address space limit as
// needed for WebAssembly memory objects (https://crbug.com/750378). Even
// with setrlimit being allowed, we cannot raise rlim_max once it's
// lowered. Thus we generally have the same protection because we normally
// set rlim_max and rlim_cur together.
//
// See SandboxLinux::LimitAddressSpace() in
// sandbox/policy/linux/sandbox_linux.cc and
// ArrayBufferContents::ReserveMemory,
// ArrayBufferContents::ReleaseReservedMemory in
// third_party/WebKit/Source/platform/wtf/typed_arrays/ArrayBufferContents.cpp.
#endif
#if defined(__i386__) || defined(__arm__)
    case __NR_ugetrlimit:
#endif
    case __NR_mremap:  // See crbug.com/149834.
    case __NR_pwrite64:
    case __NR_sched_get_priority_max:
    case __NR_sched_get_priority_min:
    case __NR_sysinfo:
    case __NR_times:
    case __NR_uname:
      // getcpu() is allowed on ARM chips because it is used in
      // //third_party/cpuinfo/ on those chips.
#if defined(__arm__) || defined(__aarch64__)
    case __NR_getcpu:
#endif
      return Allow();
#if defined(__arm__) || defined(__aarch64__)
    case __NR_prctl: {
      const Arg<int> option(0);
      return If(option == PR_SVE_GET_VL, Allow())
          .Else(BPFBasePolicy::EvaluateSyscall(sysno));
    }
#endif
    case __NR_prlimit64:
      // See crbug.com/662450 and setrlimit comment above.
      return RestrictPrlimit(GetPolicyPid());
    default:
      // Default on the content baseline policy.
      return BPFBasePolicy::EvaluateSyscall(sysno);
  }
}

}  // namespace policy
}  // namespace sandbox
