// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/linux/bpf_libassistant_policy_linux.h"

#include <linux/membarrier.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

#include "base/system/sys_info.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/seccomp-bpf-helpers/sigsys_handlers.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_parameters_restrictions.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_sets.h"
#include "sandbox/linux/syscall_broker/broker_process.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"
#include "sandbox/policy/linux/sandbox_linux.h"

using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::Arg;
using sandbox::bpf_dsl::Error;
using sandbox::bpf_dsl::If;
using sandbox::bpf_dsl::Kill;
using sandbox::bpf_dsl::ResultExpr;
using sandbox::bpf_dsl::Trap;
using sandbox::syscall_broker::BrokerProcess;

namespace sandbox {
namespace policy {
namespace {

// Those are additional syscalls required for internal Linux-ChromeOS build:
// b/362834495.
bool IsInternalLinuxCrosSyscalls(int sysno) {
  if (base::SysInfo::IsRunningOnChromeOS()) {
    return false;
  }

  return sysno == __NR_membarrier || sysno == __NR_epoll_pwait2;
}

ResultExpr RestrictInternalLinuxCrosSyscalls(int sysno) {
  switch (sysno) {
    case __NR_membarrier: {
      const Arg<int> command(0);
      return If(AnyOf(command == MEMBARRIER_CMD_REGISTER_PRIVATE_EXPEDITED_RSEQ,
                      command == MEMBARRIER_CMD_PRIVATE_EXPEDITED_RSEQ),
                Allow())
          .Else(CrashSIGSYS());
    }
    case __NR_epoll_pwait2:
      return Allow();
  }

  CHECK(false) << "RestrictInternalLinuxCrosSyscalls must be called after "
                  "IsInternalLinuxCrosSyscalls";

  return Kill();
}
}  // namespace

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
      if (IsInternalLinuxCrosSyscalls(sysno)) {
        return RestrictInternalLinuxCrosSyscalls(sysno);
      }

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
