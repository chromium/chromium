// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_IP_ADDRESS_SPACE_UTIL_H_
#define SERVICES_NETWORK_PUBLIC_CPP_IP_ADDRESS_SPACE_UTIL_H_

#include "services/network/public/mojom/ip_address_space.mojom.h"

namespace net {

class IPEndPoint;

}  // namespace net

namespace network {

// Returns the IPAddressSpace to which `endpoint` belongs.
//
// Returns `kUnknown` for invalid IP addresses. Otherwise, takes into account
// the `--ip-address-space-overrides` command-line switch.
//
// `endpoint`'s port is only used for matching to command-line overrides. It is
// ignored otherwise. In particular, if no overrides are specified on the
// command-line, then this function ignores the port entirely.
//
// WARNING: This can only be used as-is for subresource requests loaded over the
// network. Special URL schemes and resource headers must also be taken into
// account at higher layers.
mojom::IPAddressSpace COMPONENT_EXPORT(NETWORK_CPP)
    IPEndPointToIPAddressSpace(const net::IPEndPoint& endpoint);

// Returns whether `lhs` is less public than `rhs`.
//
// This comparator is compatible with std::less.
//
// Address spaces go from most public to least public in the following order:
//
//  - public and unknown (equivalent)
//  - private
//  - local
//
bool COMPONENT_EXPORT(NETWORK_CPP)
    IsLessPublicAddressSpace(mojom::IPAddressSpace lhs,
                             mojom::IPAddressSpace rhs);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_IP_ADDRESS_SPACE_UTIL_H_
