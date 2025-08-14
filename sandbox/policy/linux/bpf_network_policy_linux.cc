// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/linux/bpf_network_policy_linux.h"

#include <memory>

#include <fcntl.h>
#include <linux/ioctl.h>
#include <linux/net.h>
#include <linux/netlink.h>
#include <linux/sockios.h>
#include <linux/wireless.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/inotify.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <unistd.h>

#include "base/feature_list.h"
#include "build/build_config.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl_forward.h"
#include "sandbox/linux/seccomp-bpf-helpers/sigsys_handlers.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_parameters_restrictions.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_sets.h"
#include "sandbox/linux/syscall_broker/broker_process.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"
#include "sandbox/policy/features.h"
#include "sandbox/policy/linux/bpf_base_policy_linux.h"
#include "sandbox/policy/linux/sandbox_linux.h"

#if BUILDFLAG(IS_LINUX)
#include "net/base/features.h"  // nogncheck
#endif

using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::Arg;
using sandbox::bpf_dsl::BoolExpr;
using sandbox::bpf_dsl::Error;
using sandbox::bpf_dsl::If;
using sandbox::bpf_dsl::ResultExpr;
using sandbox::bpf_dsl::Trap;
using sandbox::syscall_broker::BrokerProcess;

#define CASES SANDBOX_BPF_DSL_CASES

// Ioctl number used by sqlite.
#if !defined(F2FS_IOC_GET_FEATURES)
#define F2FS_IOC_GET_FEATURES _IOR(0xf5, 12, uint32_t)
#endif

namespace sandbox::policy {

namespace {

ResultExpr DefaultErrorResult() {
  return CrashSIGSYS();
}

ResultExpr RestrictIoctlForNetworkService() {
  const Arg<int> request(1);
  return Switch(request)
      // sqlite uses this ioctl for feature detection when creating databases,
      // and tries to use f2fs for atomic batch writes. This avoids a rollback
      // journal and is flash-storage friendly. But f2fs is only used on Android
      // so the network process does not need to allow all the ioctls necessary
      // for f2fs atomic batch writes, so just EPERM.
      .Case(F2FS_IOC_GET_FEATURES, Error(EPERM))
      // SIOCETHTOOL, SIOCGIWNAME, and SIOCGIFNAME are needed by
      // GetNetworkList() on Linux.
      .Cases({SIOCETHTOOL, SIOCGIWNAME, SIOCGIFNAME}, Allow())
      .Case(SIOCGIFINDEX, Allow())  // For glibc's __inet6_scopeid_pton().
      .Default(RestrictIoctl());
}

ResultExpr RestrictGetSockoptForNetworkService() {
  Arg<int> level(1);
  Arg<int> optname(2);

  ResultExpr socket_optname_switch =
      Switch(optname).Case(SO_ERROR, Allow()).Default(CrashSIGSYSSockopt());
  ResultExpr ipv6_optname_switch =
      Switch(optname)
          .Cases({IPV6_V6ONLY, IPV6_TCLASS}, Allow())
          .Default(CrashSIGSYSSockopt());
  ResultExpr tcp_optname_switch =
      Switch(optname).Case(TCP_INFO, Allow()).Default(CrashSIGSYSSockopt());
  ResultExpr ip_optname_switch =
      Switch(optname).Case(IP_TOS, Allow()).Default(CrashSIGSYSSockopt());

  return Switch(level)
      .Case(SOL_SOCKET, socket_optname_switch)
      .Case(SOL_IPV6, ipv6_optname_switch)
      .Case(SOL_TCP, tcp_optname_switch)
      .Case(SOL_IP, ip_optname_switch)
      .Default(CrashSIGSYSSockopt());
}

ResultExpr RestrictSetSockoptForNetworkService() {
  Arg<int> level(1);
  Arg<int> optname(2);

  ResultExpr socket_optname_switch =
      Switch(optname)
          .Cases({SO_KEEPALIVE, SO_REUSEADDR, SO_REUSEPORT, SO_RCVBUF,
                  SO_SNDBUF, SO_BROADCAST},
                 Allow())
          .Default(CrashSIGSYSSockopt());
  // glibc's getaddrinfo() needs to enable icmp with IP[V6]_RECVERR, for both
  // ipv4 and ipv6.
  //
  // A number of optnames are for APIs of pepper and extensions. These include:
  // * IP[V6[_MULTICAST_LOOP for UDPSocketPosix::SetMulticastOptions().
  // * IP_MULTICAST_TTL, IPV6_MULTICAST_HOPS for
  //   UDPSocketPosix::SetMulticastOptions().
  //
  // IP[V6]_MULTICAST_IF, IP_ADD_MEMBERSHIP, IP_DROP_MEMBERSHIP,
  // IPV6_JOIN_GROUP, IPV6_LEAVE_GROUP are for mDNS, as well as Pepper and
  // extensions.
  //
  // IP_TOS and IPV6_TCLASS are for P2P sockets.
  ResultExpr ipv4_optname_switch =
      Switch(optname)
          .Cases({IP_RECVERR, IP_MTU_DISCOVER, IP_MULTICAST_LOOP,
                  IP_MULTICAST_TTL, IP_MULTICAST_IF, IP_ADD_MEMBERSHIP,
                  IP_DROP_MEMBERSHIP, IP_TOS, IP_RECVTOS},
                 Allow())
          .Default(CrashSIGSYSSockopt());
  ResultExpr ipv6_optname_switch =
      Switch(optname)
          .Cases({IPV6_RECVERR, IPV6_MTU_DISCOVER, IPV6_MULTICAST_LOOP,
                  IPV6_MULTICAST_HOPS, IPV6_MULTICAST_IF, IPV6_JOIN_GROUP,
                  IPV6_LEAVE_GROUP, IPV6_TCLASS, IPV6_V6ONLY, IPV6_RECVTCLASS},
                 Allow())
          .Default(CrashSIGSYSSockopt());
  ResultExpr tcp_optname_switch =
      Switch(optname)
          .Cases({TCP_KEEPIDLE, TCP_KEEPINTVL, TCP_NODELAY}, Allow())
          .Default(CrashSIGSYSSockopt());

  return Switch(level)
      .Case(SOL_SOCKET, socket_optname_switch)
      .Case(SOL_IP, ipv4_optname_switch)
      .Case(SOL_IPV6, ipv6_optname_switch)
      .Case(SOL_TCP, tcp_optname_switch)
      .Default(CrashSIGSYSSockopt());
}

ResultExpr RestrictSocketForNetworkService() {
  Arg<int> domain(0);
  Arg<int> type(1);
  Arg<int> protocol(2);

  // Flags that can be set in |type| and should be allowed.
  const int kAllowedTypeFlags = SOCK_NONBLOCK | SOCK_CLOEXEC;

  // Unix sockets needed for Mojo and other communication.
  ResultExpr unix_type_switch =
      Switch(type & ~kAllowedTypeFlags)
          .Case(SOCK_STREAM,
                If(protocol == 0, Allow()).Else(CrashSIGSYSSocket()))
          .Case(SOCK_DGRAM, Error(EPERM))  // For glibc's __inet6_scopeid_pton.
          .Default(CrashSIGSYSSocket());

  // AddressTrackerLinux needs netlink sockets for tracking system network
  // changes (which is important for e.g. reestablishing connections when an IP
  // address changes). AddressTrackerLinux may run in the network service on
  // some systems.
  bool use_netlink_in_network_service;
#if BUILDFLAG(IS_LINUX)
  // AddressTrackerLinux is brokered on Linux (depending on the feature flag),
  // but not ChromeOS.
  // TODO(crbug.com/40220507): once the kill-switch is removed, this check
  // should be removed along with the DEPS and BUILD.gn modifications to allow
  // depending on net/base/features.h.
  use_netlink_in_network_service = !base::FeatureList::IsEnabled(
      net::features::kAddressTrackerLinuxIsProxied);
#else   // !BUILDFLAG(IS_LINUX)
  // TODO(crbug.com/40220507): remove the netlink allowance when
  // AddressTrackerLinux no longer runs in the network service on ChromeOS.
  use_netlink_in_network_service = true;
#endif  // !BUILDFLAG(IS_LINUX)
  ResultExpr netlink_type_switch;
  if (use_netlink_in_network_service) {
    ResultExpr netlink_protocol_switch = Switch(protocol)
                                             .Case(NETLINK_ROUTE, Allow())
                                             .Default(CrashSIGSYSSocket());
    netlink_type_switch = Switch(type & ~kAllowedTypeFlags)
                              .Case(SOCK_RAW, netlink_protocol_switch)
                              .Default(CrashSIGSYSSocket());
  } else {
    netlink_type_switch = CrashSIGSYSSocket();
  }

  // Allow UDP and TCP sockets over ipv4 and ipv6.
  ResultExpr inet_type_switch =
      Switch(type & ~kAllowedTypeFlags)
          .Case(SOCK_DGRAM,
                If(AnyOf(protocol == 0, protocol == IPPROTO_UDP), Allow())
                    .Else(CrashSIGSYSSocket()))
          .Case(SOCK_STREAM,
                If(AnyOf(protocol == 0, protocol == SOL_TCP), Allow())
                    .Else(CrashSIGSYSSocket()))
          .Default(CrashSIGSYSSocket());

  return Switch(domain)
      .Case(AF_UNIX, unix_type_switch)
      .Case(AF_NETLINK, netlink_type_switch)
      .Cases({AF_INET, AF_INET6}, inet_type_switch)
      .Default(CrashSIGSYSSocket());
}

}  // namespace

NetworkProcessPolicy::NetworkProcessPolicy() = default;

NetworkProcessPolicy::~NetworkProcessPolicy() = default;

ResultExpr NetworkProcessPolicy::EvaluateSyscall(int sysno) const {
  auto* sandbox_linux = SandboxLinux::GetInstance();
  if (sandbox_linux->ShouldBrokerHandleSyscall(sysno)) {
    return sandbox_linux->HandleViaBroker(sysno);
  }

  // If the fine-grained syscall filter is allowed, allow all other syscalls.
  if (!base::FeatureList::IsEnabled(features::kNetworkServiceSyscallFilter)) {
    return Allow();
  }

  // inotify_add_watch should be handled above by the broker.
  if (SyscallSets::IsInotify(sysno) && sysno != __NR_inotify_add_watch) {
    return Allow();
  }

  switch (sysno) {
    // Restrictable syscalls not networking-specific:
    case __NR_fcntl:
      return RestrictFcntlCommands();
#if defined(__arm__) && defined(__ARM_EABI__)
    case __NR_arm_fadvise64_64: {
      Arg<int> advice(1);
#else
    case __NR_fadvise64: {
      Arg<int> advice(3);
#endif
      return If(advice == POSIX_FADV_WILLNEED, Allow())
          .Else(DefaultErrorResult());
    }
    case __NR_ioctl:
      return RestrictIoctlForNetworkService();

    // Mostly harmless, unrestrictable system calls:
    case __NR_sysinfo:
    case __NR_uname:
    case __NR_pwrite64:
    case __NR_pread64:
    case __NR_fdatasync:
    case __NR_fsync:
    case __NR_mremap:
#if !defined(__aarch64__)
    case __NR_getdents:
#endif
    case __NR_getdents64:
      return Allow();

    // Networking system calls:
    case __NR_getsockopt:
      return RestrictGetSockoptForNetworkService();
    case __NR_setsockopt:
      return RestrictSetSockoptForNetworkService();
    case __NR_listen:  // Used by extension and pepper APIs, and also the
                       // devtools server.
#if defined(__NR_accept)
    case __NR_accept:  // Same as listen().
#endif
    case __NR_accept4:  // Same as listen().
    case __NR_connect:
    case __NR_bind:
    case __NR_getsockname:
    case __NR_sendmmsg:
      return Allow();
    case __NR_socket:
      return RestrictSocketForNetworkService();
#if defined(__NR_socketcall)
    case __NR_socketcall:
      // Unfortunately there's no easy way to restrict socketcall as it uses a
      // struct pointer to pass its arguments. So this rewrites socketcall()
      // calls into direct sockets-API syscalls, which will be filtered like
      // normal.
      return CanRewriteSocketcall() ? RewriteSocketcallSIGSYS() : Allow();
#endif
  }

  return BPFBasePolicy::EvaluateSyscall(sysno);
}

}  // namespace sandbox::policy
