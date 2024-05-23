// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

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

std::optional<std::vector<IPEndPoint>> GetNameservers(
    const struct __res_state& res) {
  std::vector<IPEndPoint> nameservers;

  if (!(res.options & RES_INIT))
    return std::nullopt;

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_FREEBSD)
  union res_sockaddr_union addresses[MAXNS];
  int nscount = res_getservers(const_cast<res_state>(&res), addresses, MAXNS);
  DCHECK_GE(nscount, 0);
  DCHECK_LE(nscount, MAXNS);
  for (int i = 0; i < nscount; ++i) {
    IPEndPoint ipe;
    if (!ipe.FromSockAddr(
            reinterpret_cast<const struct sockaddr*>(&addresses[i]),
            sizeof addresses[i])) {
      return std::nullopt;
    }
    nameservers.push_back(ipe);
  }
#elif BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  static_assert(std::extent<decltype(res.nsaddr_list)>() >= MAXNS &&
                    std::extent<decltype(res._u._ext.nsaddrs)>() >= MAXNS,
                "incompatible libresolv res_state");
  DCHECK_LE(res.nscount, MAXNS);
  // Initially, glibc stores IPv6 in |_ext.nsaddrs| and IPv4 in |nsaddr_list|.
  // In res_send.c:res_nsend, it merges |nsaddr_list| into |nsaddrs|,
  // but we have to combine the two arrays ourselves.
  for (int i = 0; i < res.nscount; ++i) {
    IPEndPoint ipe;
    const struct sockaddr* addr = nullptr;
    size_t addr_len = 0;
    if (res.nsaddr_list[i].sin_family) {  // The indicator used by res_nsend.
      addr = reinterpret_cast<const struct sockaddr*>(&res.nsaddr_list[i]);
      addr_len = sizeof res.nsaddr_list[i];
    } else if (res._u._ext.nsaddrs[i]) {
      addr = reinterpret_cast<const struct sockaddr*>(res._u._ext.nsaddrs[i]);
      addr_len = sizeof *res._u._ext.nsaddrs[i];
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
