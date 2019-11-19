// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/sandbox/linux/bpf_audio_policy_linux.h"

#include <sys/socket.h>

#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/seccomp-bpf-helpers/sigsys_handlers.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_parameters_restrictions.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_sets.h"
#include "sandbox/linux/services/syscall_wrappers.h"
#include "sandbox/linux/syscall_broker/broker_process.h"
#include "sandbox/linux/system_headers/linux_futex.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"
#include "services/service_manager/sandbox/linux/sandbox_linux.h"

using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::Arg;
using sandbox::bpf_dsl::Error;
using sandbox::bpf_dsl::If;
using sandbox::bpf_dsl::ResultExpr;
using sandbox::bpf_dsl::Trap;
using sandbox::syscall_broker::BrokerProcess;

namespace service_manager {

AudioProcessPolicy::AudioProcessPolicy() = default;

AudioProcessPolicy::~AudioProcessPolicy() = default;

ResultExpr AudioProcessPolicy::EvaluateSyscall(int system_call_number) const {
  switch (system_call_number) {
#if defined(__NR_connect)
    case __NR_connect:
#endif
#if defined(__NR_ftruncate)
    case __NR_ftruncate:
#endif
#if defined(__NR_ftruncate64)
    case __NR_ftruncate64:
#endif
#if defined(__NR_fallocate)
    case __NR_fallocate:
#endif
#if defined(__NR_getdents)
    case __NR_getdents:
#endif
#if defined(__NR_getpeername)
    case __NR_getpeername:
#endif
#if defined(__NR_getsockopt)
    case __NR_getsockopt:
#endif
#if defined(__NR_getsockname)
    case __NR_getsockname:
#endif
#if defined(__NR_ioctl)
    case __NR_ioctl:
#endif
#if defined(__NR_memfd_create)
    case __NR_memfd_create:
#endif
#if defined(__NR_pwrite)
    case __NR_pwrite:
#endif
#if defined(__NR_pwrite64)
    case __NR_pwrite64:
#endif
#if defined(__NR_sched_setscheduler)
    case __NR_sched_setscheduler:
#endif
#if defined(__NR_setsockopt)
    case __NR_setsockopt:
#endif
#if defined(__NR_uname)
    case __NR_uname:
#endif
      return Allow();
#if defined(__NR_futex)
    case __NR_futex: {
      const Arg<int> op(1);
#if defined(USE_PULSEAUDIO)
      return Switch(op & ~FUTEX_PRIVATE_FLAG)
          .SANDBOX_BPF_DSL_CASES(
              (FUTEX_CMP_REQUEUE, FUTEX_LOCK_PI, FUTEX_UNLOCK_PI, FUTEX_WAIT,
               FUTEX_WAIT_BITSET, FUTEX_WAKE),
              Allow())
          .Default(Error(EPERM));
#else
      return sandbox::RestrictFutex();
#endif
    }
#endif
#if defined(__NR_kill)
    case __NR_kill: {
      // man kill says:
      // "If sig is 0, then no signal is sent, but existence and permission
      //  checks are still performed; this can be used to check for the
      //  existence of a process ID or process group ID that the caller is
      //  permitted to signal."
      //
      // This seems to be tripping up at least ESET's NOD32 anti-virus, causing
      // an unnecessary crash in the audio process. See: http://crbug.com/904787
      const Arg<pid_t> pid(0);
      const Arg<int> sig(1);
      return If(pid == sandbox::sys_getpid(), Allow())
          .ElseIf(sig == 0, Error(EPERM))
          .Else(sandbox::CrashSIGSYSKill());
    }
#endif
#if defined(__NR_socket)
    case __NR_socket: {
      const Arg<int> domain(0);
      return If(domain == AF_UNIX, Allow()).Else(Error(EPERM));
    }
#endif
    default:
#if defined(__x86_64__)
      if (sandbox::SyscallSets::IsSystemVSemaphores(system_call_number) ||
          sandbox::SyscallSets::IsSystemVSharedMemory(system_call_number)) {
        return Allow();
      }
#elif defined(__i386__)
      if (sandbox::SyscallSets::IsSystemVIpc(system_call_number))
        return Allow();
#endif

      auto* broker_process = SandboxLinux::GetInstance()->broker_process();
      if (broker_process->IsSyscallAllowed(system_call_number))
        return Trap(BrokerProcess::SIGSYS_Handler, broker_process);

      return BPFBasePolicy::EvaluateSyscall(system_call_number);
  }
}

}  // namespace service_manager
