// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_IP_ADDRESS_SPACE_UTIL_H_
#define SERVICES_NETWORK_PUBLIC_CPP_IP_ADDRESS_SPACE_UTIL_H_

#include <optional>
#include <string_view>
#include <vector>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/stack_allocated.h"
#include "services/network/public/mojom/ip_address_space.mojom-forward.h"
#include "services/network/public/mojom/parsed_headers.mojom-forward.h"

class GURL;

namespace net {

class IPAddress;
class IPEndPoint;
struct TransportInfo;

}  // namespace net

namespace network {

// Returns a human-readable string representing `space`, suitable for logging.
std::string_view COMPONENT_EXPORT(NETWORK_CPP)
    IPAddressSpaceToStringPiece(mojom::IPAddressSpace space);

// Returns the `IPAddressSpace` to which `address` belongs.
// Returns `kUnknown` for invalid IP addresses.
//
// WARNING: Most callers will want to use `TransportInfoToIPAddressSpace()`
// below instead, as this does not properly account for proxies nor for
// command-line overrides.
mojom::IPAddressSpace COMPONENT_EXPORT(NETWORK_CPP)
    IPAddressToIPAddressSpace(const net::IPAddress& address);

// Returns the `IPAddressSpace` to which the endpoint of `transport` belongs.
//
// When the transport is a proxied connection, returns `kUnknown`. See
// https://github.com/WICG/private-network-access/issues/62 for more details on
// the reasoning there.
//
// When the transport is a direct connection, returns the IP address space of
// `info.endpoint`. This returns `kUnknown` for invalid IP addresses. Otherwise,
// takes into account the `--ip-address-space-overrides` command-line switch. If
// no override applies, then follows this algorithm:
// https://wicg.github.io/private-network-access/#determine-the-ip-address-space
//
// `info.endpoint`'s port is only used for matching to command-line overrides.
// It is ignored otherwise. In particular, if no overrides are specified on the
// command-line, then this function ignores the port entirely.
//
// WARNING: This can only be used as-is for subresource requests loaded over the
// network. Special URL schemes and resource headers must also be taken into
// account at higher layers.
mojom::IPAddressSpace COMPONENT_EXPORT(NETWORK_CPP)
    TransportInfoToIPAddressSpace(const net::TransportInfo& info);

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

// Represents optional parameters of CalculateClientAddressSpace().
// This is effectively a subset of network::mojom::URLResponseHead.
// WARNING: This struct just keeps references to parameters and does not own
// them nor make copy of them. Parameters must outlive this struct. For example,
// passing net::IPEndPoint() as `remote_endpoint` is invalid.
struct COMPONENT_EXPORT(NETWORK_CPP) CalculateClientAddressSpaceParams {
  STACK_ALLOCATED();

 public:
  ~CalculateClientAddressSpaceParams();

  const std::optional<mojom::IPAddressSpace>
      client_address_space_inherited_from_service_worker;
  const raw_ptr<const mojom::ParsedHeadersPtr> parsed_headers;
  const raw_ptr<const net::IPEndPoint> remote_endpoint;
};

// Given a request URL and `params`, this function calculates the
// IPAddressSpace which should be associated with documents or worker global
// scopes (collectively: request clients) instantiated from this resource.
//
// `params` is optional. If `params` contain values, values must be valid and
// `params.value().parsed_headers` must be populated with the result of
// parsing the corresponding response headers.
//
// WARNING: This function is defined here for proximity with related code and
// the data structures involved. However since it deals with higher-level
// concepts too (documents and worker global scopes), it should probably only be
// used at the content/ layer or above.
//
// See: https://wicg.github.io/cors-rfc1918/#address-space
mojom::IPAddressSpace COMPONENT_EXPORT(NETWORK_CPP) CalculateClientAddressSpace(
    const GURL& url,
    std::optional<CalculateClientAddressSpaceParams> params);

// Given a response URL and the IP endpoint the requested resource was fetched
// from, this function calculates the IPAddressSpace of the requested resource.
//
// As opposed to CalculateClientAddressSpace(), this function is used to
// determine the address space of the *target* of a fetch, for comparison with
// that of the client of the fetch.
//
// See: https://wicg.github.io/cors-rfc1918/#integration-fetch
mojom::IPAddressSpace COMPONENT_EXPORT(NETWORK_CPP)
    CalculateResourceAddressSpace(const GURL& url,
                                  const net::IPEndPoint& endpoint);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_IP_ADDRESS_SPACE_UTIL_H_
