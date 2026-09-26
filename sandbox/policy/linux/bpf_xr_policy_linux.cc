// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/linux/bpf_xr_policy_linux.h"

#include <errno.h>
#include <sys/socket.h>

#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"
#include "sandbox/policy/linux/sandbox_linux.h"

using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::Arg;
using sandbox::bpf_dsl::Error;
using sandbox::bpf_dsl::If;
using sandbox::bpf_dsl::ResultExpr;

namespace sandbox {
namespace policy {

XrProcessPolicy::XrProcessPolicy() : GpuProcessPolicy(MremapPolicy::kBlock) {}

XrProcessPolicy::~XrProcessPolicy() = default;

ResultExpr XrProcessPolicy::EvaluateSyscall(int system_call_number) const {
  switch (system_call_number) {
    // The runtime reaches its compositor over an AF_UNIX socket and passes fds
    // with SCM_RIGHTS, neither of which the GPU policy allows. get/setsockopt
    // stay disallowed; add a narrow level/optname restriction if ever needed.
#if defined(__NR_getpeername)
    case __NR_getpeername:
#endif
#if defined(__NR_getsockname)
    case __NR_getsockname:
#endif
      // The OpenXR runtime's IPC client arms a watchdog timer around its
      // connection handshake.
#if defined(__NR_alarm)
    case __NR_alarm:
#endif
      // SteamVR's runtime additionally locks files and reads robust-futex
      // lists.
#if defined(__NR_flock)
    case __NR_flock:
#endif
#if defined(__NR_get_robust_list)
    case __NR_get_robust_list:
#endif
      return Allow();
#if defined(__NR_kill)
    case __NR_kill: {
      // SteamVR probes its sibling processes for liveness with kill(pid, 0).
      // There is no PID namespace here, so delivering a real signal to an
      // arbitrary pid stays refused.
      const Arg<int> sig(1);
      return If(sig == 0, Allow()).Else(Error(EPERM));
    }
#endif
    // connect() and bind() are brokered so the browser process can restrict
    // them to the active runtime's socket names (seccomp cannot inspect the
    // sockaddr): connect() to the runtime's IPC sockets, and bind() to the
    // endpoints SteamVR creates to receive the compositor's texture fds.
#if defined(__NR_bind)
    case __NR_bind:
#endif
#if defined(__NR_connect)
    case __NR_connect: {
      auto* sandbox_linux = SandboxLinux::GetInstance();
      if (sandbox_linux->ShouldBrokerHandleSyscall(system_call_number)) {
        return sandbox_linux->HandleViaBroker(system_call_number);
      }
      return Error(EPERM);
    }
#endif
#if defined(__NR_socket)
    case __NR_socket: {
      // Restrict socket creation to AF_UNIX; the runtime never needs network
      // sockets, and netlink must never be allowed.
      const Arg<int> domain(0);
      return If(domain == AF_UNIX, Allow()).Else(Error(EPERM));
    }
#endif
#if defined(__NR_sched_setscheduler)
    case __NR_sched_setscheduler: {
      // Mesa's shader-cache workers set a sibling thread's policy, which the
      // baseline crashes on (it only rewrites the caller's own TID). Setting
      // priority is best-effort everywhere, so refuse with EPERM instead.
      const Arg<pid_t> pid(0);
      return If(pid == 0, Allow()).Else(Error(EPERM));
    }
#endif
    default:
      // Everything else (GPU/DRM/Vulkan syscalls, directory enumeration, the
      // file broker, etc.) is handled by the GPU process policy.
      return GpuProcessPolicy::EvaluateSyscall(system_call_number);
  }
}

}  // namespace policy
}  // namespace sandbox
