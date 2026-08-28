// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/public/resolv_reader.h"

#include <netinet/in.h>
#include <resolv.h>
#include <sys/types.h>

#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "build/build_config.h"
#include "net/base/ip_endpoint.h"

namespace net {

std::unique_ptr<ScopedResState> ResolvReader::GetResState() {
  auto res = std::make_unique<ScopedResState>();
  if (!res->IsValid())
    return nullptr;
  return res;
}

bool ResolvReader::IsLikelySystemdResolved() {
#if BUILDFLAG(IS_LINUX)
  // Look for a single 127.0.0.53:53 nameserver endpoint. The only known
  // significant usage of such a configuration is the systemd-resolved local
  // resolver, so it is then a fairly safe assumption that any DNS queries to
  // the nameserver will be handled by systemd-resolved.
  //
  // This code path is only reachable if the system has nss-resolve configured
  // in nsswitch.conf, which is another indicator that systemd-resolved is
  // likely to be in use.
  std::unique_ptr<ScopedResState> res = GetResState();
  if (res) {
    std::optional<std::vector<IPEndPoint>> nameservers =
        GetNameservers(res->state());
    if (nameservers) {
      return nameservers->size() == 1 &&
             nameservers->front() == IPEndPoint(IPAddress(127, 0, 0, 53), 53);
    }
  }
#endif

  return false;
}

std::optional<std::vector<IPEndPoint>> GetNameservers(
    const struct __res_state& res) {
  std::vector<IPEndPoint> nameservers;

  if (!(res.options & RES_INIT))
    return std::nullopt;

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_FREEBSD)
  union res_sockaddr_union addresses[MAXNS];
  int nscount = res_getservers(const_cast<res_state>(&res), addresses, MAXNS);
  // res_getservers() is not documented to return a negative value or more
  // entries than requested, but handle those cases defensively rather than
  // trusting the libc, since the count bounds the buffer accesses below.
  if (nscount < 0 || nscount > MAXNS) {
    return std::nullopt;
  }
  for (const auto& address :
       base::span(addresses).first(static_cast<size_t>(nscount))) {
    IPEndPoint ipe;
    if (!ipe.FromSockAddr(reinterpret_cast<const struct sockaddr*>(&address),
                          sizeof(address))) {
      return std::nullopt;
    }
    nameservers.push_back(ipe);
  }
#elif BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  static_assert(std::extent<decltype(res.nsaddr_list)>() >= MAXNS &&
                    std::extent<decltype(res._u._ext.nsaddrs)>() >= MAXNS,
                "incompatible libresolv res_state");
  // |nscount| tracks the number of configured nameservers and should never be
  // negative or exceed MAXNS, but handle those cases defensively rather than
  // trusting the libc, since the count bounds the buffer accesses below.
  if (res.nscount < 0 || res.nscount > MAXNS) {
    return std::nullopt;
  }
  const size_t nscount = static_cast<size_t>(res.nscount);
  // Initially, glibc stores IPv6 in |_ext.nsaddrs| and IPv4 in |nsaddr_list|.
  // In res_send.c:res_nsend, it merges |nsaddr_list| into |nsaddrs|,
  // but we have to combine the two arrays ourselves.
  auto nsaddr_list = base::span(res.nsaddr_list).first(nscount);
  auto nsaddrs = base::span(res._u._ext.nsaddrs).first(nscount);
  for (size_t i = 0; i < nscount; ++i) {
    IPEndPoint ipe;
    const struct sockaddr* addr = nullptr;
    size_t addr_len = 0;
    if (nsaddr_list[i].sin_family) {  // The indicator used by res_nsend.
      addr = reinterpret_cast<const struct sockaddr*>(&nsaddr_list[i]);
      addr_len = sizeof(nsaddr_list[i]);
    } else if (nsaddrs[i]) {
      addr = reinterpret_cast<const struct sockaddr*>(nsaddrs[i]);
      addr_len = sizeof(*nsaddrs[i]);
    } else {
      return std::nullopt;
    }
    if (!ipe.FromSockAddr(addr, addr_len))
      return std::nullopt;
    nameservers.push_back(ipe);
  }
#else  // !(BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_APPLE)
       // || BUILDFLAG(IS_FREEBSD))
  DCHECK_LE(res.nscount, MAXNS);
  for (int i = 0; i < res.nscount; ++i) {
    IPEndPoint ipe;
    if (!ipe.FromSockAddr(
            reinterpret_cast<const struct sockaddr*>(&res.nsaddr_list[i]),
            sizeof res.nsaddr_list[i])) {
      return std::nullopt;
    }
    nameservers.push_back(ipe);
  }
#endif

  return nameservers;
}

}  // namespace net
