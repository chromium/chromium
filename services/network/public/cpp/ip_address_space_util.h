// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_IP_ADDRESS_SPACE_UTIL_H_
#define SERVICES_NETWORK_PUBLIC_CPP_IP_ADDRESS_SPACE_UTIL_H_

#include "services/network/public/mojom/ip_address_space.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"

class GURL;

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

// Given a request URL and response information, this function calculates the
// IPAddressSpace which should be associated with documents or worker global
// scopes (collectively: request clients) instantiated from this resource.
//
// |response_head| may be nullptr. Caller retains ownership. If not nullptr,
// then |response_head->parsed_headers| must be populated with the result of
// parsing |response->headers|.
//
// WARNING: This function is defined here for proximity with related code and
// the data structures involved. However since it deals with higher-level
// concepts too (documents and worker global scopes), it should probably only be
// used at the content/ layer or above.
//
// See: https://wicg.github.io/cors-rfc1918/#address-space
//
// TODO(https://crbug.com/1134601): This implementation treats requests that
// don't use a URL loader (`about:blank`), as well as requests whose IP address
// is invalid (`about:srcdoc`, `blob:`, etc.) as `kUnknown`. This is incorrect.
// We'll eventually want to make sure we inherit from the client's creator
// in some cases), but safe, as `kUnknown` is treated the same as `kPublic`.
mojom::IPAddressSpace COMPONENT_EXPORT(NETWORK_CPP)
    CalculateClientAddressSpace(const GURL& url,
                                const mojom::URLResponseHead* response_head);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_IP_ADDRESS_SPACE_UTIL_H_
