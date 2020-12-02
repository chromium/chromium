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

// Returns the IPAddressSpace from an IPAddress.
//
// WARNING: This can only be used as-is for subresource requests loaded over the
// network. For other cases, see the Calculate*AddressSpace() functions below.
mojom::IPAddressSpace COMPONENT_EXPORT(NETWORK_CPP)
    IPAddressToIPAddressSpace(const net::IPAddress& address);

// Returns whether |lhs| is less public than |rhs|.
//
// This comparator is compatible with std::less.
//
// Address spaces go from most public to least public in the following order:
//
//  - public and unknown
//  - private
//  - local
//
bool COMPONENT_EXPORT(NETWORK_CPP)
    IsLessPublicAddressSpace(mojom::IPAddressSpace lhs,
                             mojom::IPAddressSpace rhs);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_IP_ADDRESS_SPACE_UTIL_H_
