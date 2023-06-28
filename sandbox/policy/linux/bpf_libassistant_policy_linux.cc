// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/linux/bpf_libassistant_policy_linux.h"

#include <netinet/tcp.h>
#include <sys/socket.h>

#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_parameters_restrictions.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_sets.h"
#include "sandbox/linux/syscall_broker/broker_process.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"
#include "sandbox/policy/linux/sandbox_linux.h"

using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::Arg;
using sandbox::bpf_dsl::Error;
using sandbox::bpf_dsl::If;
using sandbox::bpf_dsl::ResultExpr;
using sandbox::bpf_dsl::Trap;
using sandbox::syscall_broker::BrokerProcess;

namespace sandbox {
namespace policy {

LibassistantProcessPolicy::LibassistantProcessPolicy() = default;
LibassistantProcessPolicy::~LibassistantProcessPolicy() = default;

ResultExpr LibassistantProcessPolicy::EvaluateSyscall(int sysno) const {
  switch (sysno) {
    case __NR_socket: {
      const Arg<int> domain(0);
      const Arg<int> type(1);
      const Arg<int> protocol(2);
      const int kSockFlags = SOCK_CLOEXEC | SOCK_NONBLOCK;
      return If(AllOf(domain == AF_UNIX, (type & ~kSockFlags) == SOCK_STREAM,
                      protocol == 0),
                Allow())
          .Else(Error(EPERM));
    }
    case __NR_getsockopt: {
      const Arg<int> level(1);
      const Arg<int> optname(2);
      return If(AllOf(level == SOL_SOCKET, optname == SO_REUSEADDR), Allow())
          .Else(BPFBasePolicy::EvaluateSyscall(sysno));
    }
    case __NR_setsockopt: {
      const Arg<int> level(1);
      const Arg<int> optname(2);
      return If(AllOf(AnyOf(level == SOL_SOCKET, level == SOL_TCP),
                      AnyOf(optname == SO_REUSEADDR, optname == SO_MARK,
                            optname == SO_ZEROCOPY)),
                Allow())
          .Else(BPFBasePolicy::EvaluateSyscall(sysno));
    }
    case __NR_accept4:
    case __NR_bind:
    case __NR_connect:
    // Needed by arm devices.
    case __NR_getcpu:
    case __NR_getpeername:
    case __NR_getsockname:
    case __NR_listen:
    case __NR_sync:
      return Allow();
    default:
      if (SyscallSets::IsGoogle3Threading(sysno)) {
        return RestrictGoogle3Threading(sysno);
      }

      auto* sandbox_linux = SandboxLinux::GetInstance();
      if (sandbox_linux->ShouldBrokerHandleSyscall(sysno))
        return sandbox_linux->HandleViaBroker(sysno);

      return BPFBasePolicy::EvaluateSyscall(sysno);
  }
}

}  // namespace policy
}  // namespace sandbox
