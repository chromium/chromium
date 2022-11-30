// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/linux/bpf_gpu_policy_linux.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/compiler_specific.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_parameters_restrictions.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_sets.h"
#include "sandbox/linux/syscall_broker/broker_process.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"
#include "sandbox/policy/linux/bpf_base_policy_linux.h"
#include "sandbox/policy/linux/sandbox_linux.h"
#include "sandbox/policy/linux/sandbox_seccomp_bpf_linux.h"

using sandbox::bpf_dsl::AllOf;
using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::Arg;
using sandbox::bpf_dsl::BoolExpr;
using sandbox::bpf_dsl::Error;
using sandbox::bpf_dsl::If;
using sandbox::bpf_dsl::ResultExpr;
using sandbox::bpf_dsl::Trap;
using sandbox::syscall_broker::BrokerProcess;

namespace sandbox {
namespace policy {

GpuProcessPolicy::GpuProcessPolicy() {}

GpuProcessPolicy::~GpuProcessPolicy() {}

// Main policy for x86_64/i386. Extended by CrosArmGpuProcessPolicy.
ResultExpr GpuProcessPolicy::EvaluateSyscall(int sysno) const {
  // Many GPU drivers need to dlopen additional libraries (see the file
  // permissions in gpu_sandbox_hook_linux.cc that end in .so).
  if (SyscallSets::IsDlopen(sysno)) {
    return Allow();
  }

  switch (sysno) {
    case __NR_kcmp:
      return Error(ENOSYS);
#if !BUILDFLAG(IS_CHROMEOS)
    case __NR_fallocate:
      return Allow();
#endif  // BUILDFLAG(IS_CHROMEOS)
    case __NR_fcntl: {
      // The Nvidia driver uses flags not in the baseline policy
      // fcntl(fd, F_ADD_SEALS, F_SEAL_SEAL | F_SEAL_SHRINK | F_SEAL_GROW)
      // https://crbug.com/1128175
      const Arg<unsigned int> cmd(1);
      const Arg<unsigned long> arg(2);

      const unsigned long kAllowedMask =
          F_SEAL_SEAL | F_SEAL_SHRINK | F_SEAL_GROW;
      BoolExpr add_seals =
          AllOf(cmd == F_ADD_SEALS, (arg & ~kAllowedMask) == 0);

      return If(add_seals, Allow()).Else(BPFBasePolicy::EvaluateSyscall(sysno));
    }
    case __NR_ftruncate:
#if defined(__i386__) || defined(__arm__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
    case __NR_ftruncate64:
#endif
#if !defined(__aarch64__)
    case __NR_getdents:
#endif
    case __NR_getdents64:
    case __NR_ioctl:
      return Allow();
#if defined(__i386__) || defined(__x86_64__) || defined(__mips__)
    // The Nvidia driver uses flags not in the baseline policy
    // (MAP_LOCKED | MAP_EXECUTABLE | MAP_32BIT)
    case __NR_mmap:
      return Allow();
#endif
    // We also hit this on the linux_chromeos bot but don't yet know what
    // weird flags were involved.
    case __NR_mprotect:
    // TODO(jln): restrict prctl.
    case __NR_prctl:
    case __NR_sysinfo:
    case __NR_uname:  // https://crbug.com/1075934
      return Allow();
    case __NR_sched_setaffinity:
      return RestrictSchedTarget(GetPolicyPid(), sysno);
    case __NR_prlimit64:
      return RestrictPrlimit64(GetPolicyPid());
    default:
      break;
  }

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  if (SyscallSets::IsSystemVSharedMemory(sysno))
    return Allow();
#endif

  auto* sandbox_linux = SandboxLinux::GetInstance();
  if (sandbox_linux->ShouldBrokerHandleSyscall(sysno))
    return sandbox_linux->HandleViaBroker(sysno);

  // Default on the baseline policy.
  return BPFBasePolicy::EvaluateSyscall(sysno);
}

}  // namespace policy
}  // namespace sandbox
