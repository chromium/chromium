// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_IP_ADDRESS_SPACE_UTIL_H_
#define SERVICES_NETWORK_PUBLIC_CPP_IP_ADDRESS_SPACE_UTIL_H_

#include "services/network/public/mojom/ip_address_space.mojom.h"

namespace net {

class IPAddress;

}  // namespace net

namespace network {

// Returns the IPAddressSpace to which `address` belongs.
//
// Returns `kUnknown` for invalid addresses. Otherwise, takes into account the
// `--ip-address-space-overrides` command-line switch.
//
// WARNING: This can only be used as-is for subresource requests loaded over the
// network. Special URL schemes and resource headers must also be taken into
// account at higher layers.
mojom::IPAddressSpace COMPONENT_EXPORT(NETWORK_CPP)
    IPAddressToIPAddressSpace(const net::IPAddress& address);

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
