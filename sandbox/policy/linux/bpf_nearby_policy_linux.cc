// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/linux/bpf_nearby_policy_linux.h"

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

#include "sandbox/linux/seccomp-bpf-helpers/sigsys_handlers.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"
#include "sandbox/policy/linux/sandbox_linux.h"

using sandbox::bpf_dsl::AllOf;
using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::Arg;
using sandbox::bpf_dsl::If;
using sandbox::bpf_dsl::ResultExpr;

namespace sandbox::policy {

namespace {

ResultExpr RestrictSocketForNearbyProcess() {
  Arg<int> domain(0);
  Arg<int> type(1);
  Arg<int> protocol(2);

  // This is the explicit socket configuration used by `WifiDirectMedium`.
  return If(AllOf(domain == AF_INET, type == SOCK_STREAM,
                  protocol == IPPROTO_TCP),
            Allow())
      .Else(CrashSIGSYSSocket());
}

ResultExpr RestrictSetSockoptForNearbyProcess() {
  Arg<int> level(1);
  Arg<int> optname(2);

  ResultExpr socket_optname_switch =
      Switch(optname)
          .Cases({SO_KEEPALIVE, SO_REUSEADDR, SO_REUSEPORT, SO_RCVBUF,
                  SO_SNDBUF, SO_BROADCAST},
                 Allow())
          .Default(CrashSIGSYSSockopt());

  ResultExpr tcp_optname_switch =
      Switch(optname)
          .Cases({TCP_KEEPIDLE, TCP_KEEPINTVL, TCP_NODELAY}, Allow())
          .Default(CrashSIGSYSSockopt());

  return Switch(level)
      .Case(SOL_SOCKET, socket_optname_switch)
      .Case(SOL_TCP, tcp_optname_switch)
      .Default(CrashSIGSYSSockopt());
}

}  // namespace

NearbyProcessPolicy::NearbyProcessPolicy() = default;

NearbyProcessPolicy::~NearbyProcessPolicy() = default;

ResultExpr NearbyProcessPolicy::EvaluateSyscall(int sysno) const {
  switch (sysno) {
    case __NR_accept:
    case __NR_accept4:
    case __NR_connect:
    case __NR_bind:
    case __NR_listen:
    case __NR_getsockname:
    case __NR_sendmmsg:
    case __NR_getsockopt:
      return Allow();
    case __NR_setsockopt:
      return RestrictSetSockoptForNearbyProcess();
    case __NR_socket:
      return RestrictSocketForNearbyProcess();
    default:
      // Default on the baseline policy.
      return BPFBasePolicy::EvaluateSyscall(sysno);
  }
}

}  // namespace sandbox::policy
