// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_IP_ADDRESS_UTIL_H_
#define NET_BASE_IP_ADDRESS_UTIL_H_

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#endif

#include <stdint.h>

#include "net/base/net_export.h"

// This file contains helpers to convert a net::IPAddress to an in_addr or
// in6_addr. This is a separate file because using in_addr or in6_addr requires
// including winsock2.h on Windows, which brings in a massive number of
// dependencies. As a result, this file should not be included in any header
// files - it should only be used in CC files..

namespace net {

class IPAddress;

// Converts `ip_address` to an in_addr or in6_addr, respectively. CHECKs if
// `ip_address` is not of the requested type. Does not do IPv4/IPv6 conversions.
// Since in_addr values are arrays of bytes, rather than representing the entire
// address as a single value, there is no need to convert to network byte order.
NET_EXPORT in_addr ToInAddr(const IPAddress& ip_address);
NET_EXPORT in6_addr ToIn6Addr(const IPAddress& ip_address);

}  // namespace net

#endif  // NET_BASE_IP_ADDRESS_UTIL_H_
