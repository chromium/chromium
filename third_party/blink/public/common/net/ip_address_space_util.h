// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_NET_IP_ADDRESS_SPACE_UTIL_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_NET_IP_ADDRESS_SPACE_UTIL_H_

#include "services/network/public/mojom/ip_address_space.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "third_party/blink/public/common/common_export.h"

class GURL;

namespace net {

class IPAddress;

}  // namespace net

namespace blink {

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
network::mojom::IPAddressSpace BLINK_COMMON_EXPORT CalculateClientAddressSpace(
    const GURL& url,
    const network::mojom::URLResponseHead* response_head);

// Given a response URL and the IP address the requested resource was fetched
// from, this function calculates the IPAddressSpace of the requested resource.
//
// As opposed to CalculateClientAddressSpace(), this function is used to
// determine the address space of the *target* of a fetch, for comparison with
// that of the client of the fetch.
//
// See: https://wicg.github.io/cors-rfc1918/#integration-fetch
network::mojom::IPAddressSpace BLINK_COMMON_EXPORT
CalculateResourceAddressSpace(const GURL& url, const net::IPAddress& address);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_NET_IP_ADDRESS_SPACE_UTIL_H_
