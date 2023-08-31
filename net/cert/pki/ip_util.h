// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_PKI_IP_UTIL_H_
#define NET_CERT_PKI_IP_UTIL_H_

#include "net/base/net_export.h"
#include "net/der/input.h"

namespace net {

inline constexpr size_t kIPv4AddressSize = 4;
inline constexpr size_t kIPv6AddressSize = 16;

// Returns whether `mask` is a valid netmask. I.e., whether it the length of an
// IPv4 or IPv6 address, and is some number of ones, followed by some number of
// zeros.
NET_EXPORT_PRIVATE bool IsValidNetmask(der::Input mask);

// Returns whether `addr1` and `addr2` are equal under the netmask `mask`.
NET_EXPORT_PRIVATE bool IPAddressMatchesWithNetmask(der::Input addr1,
                                                    der::Input addr2,
                                                    der::Input mask);

}  // namespace net

#endif  // NET_CERT_PKI_IP_UTIL_H_
