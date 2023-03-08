// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/ip_address_space_util.h"

#include <utility>

#include "base/command_line.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/transport_info.h"
#include "services/network/public/cpp/content_security_policy/content_security_policy.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/mojom/ip_address_space.mojom.h"
#include "services/network/public/mojom/parsed_headers.mojom.h"
#include "url/gurl.h"

namespace network {
namespace {

using mojom::IPAddressSpace;
using net::IPAddress;
using net::IPEndPoint;

// Parses a string of the form "<URL-safe IP address>:<port>".
absl::optional<IPEndPoint> ParseEndpoint(base::StringPiece str) {
  // Find the last colon character in `str`. We do not use
  // `base::SplitStringPiece()` because IPv6 address literals may contain colon
  // characters too.
  const auto pos = str.rfind(':');
  if (pos == str.npos) {
    return absl::nullopt;
  }

  base::StringPiece address_str = str.substr(0, pos);

  // Skip the colon. Note that this is safe because if `pos` is not `npos`, it
  // is guaranteed to be < `str.size()`, and `substr()` accepts arguments that
  // are <= `str.size()`. In other words, if the colon character is the last in
  // `str`, then `port_str` is assigned "".
  base::StringPiece port_str = str.substr(pos + 1);

  IPAddress address;
  if (!net::ParseURLHostnameToAddress(address_str, &address)) {
    return absl::nullopt;
  }

  // Parse to a `unsigned int`, which is guaranteed to be at least 16 bits wide.
  // See https://en.cppreference.com/w/cpp/language/types.
  unsigned port_unsigned = 0;
  if (!base::StringToUint(port_str, &port_unsigned)) {
    return absl::nullopt;
  }

  if (!base::IsValueInRangeForNumericType<uint16_t>(port_unsigned)) {
    return absl::nullopt;
  }

  // Use `checked_cast()` for extra safety, though this should never `CHECK()`
  // thanks to the previous call to `IsValueInRangeForNumericType()`.
  uint16_t port = base::checked_cast<uint16_t>(port_unsigned);
  return IPEndPoint(address, port);
}

absl::optional<IPAddressSpace> ParseIPAddressSpace(base::StringPiece str) {
  if (str == "public") {
    return IPAddressSpace::kPublic;
  }

  if (str == "local") {
    return IPAddressSpace::kLocal;
  }

  if (str == "loopback") {
    return IPAddressSpace::kLoopback;
  }

  return absl::nullopt;
}

// Represents a single command-line-specified endpoint override.
struct EndpointOverride {
  // The IP endpoint to override the address space for.
  IPEndPoint endpoint;

  // The IP address space to which `endpoint` should be mapped.
  IPAddressSpace space;
};

// Parses an override from `str`, of the form "<endpoint>=<space>".
absl::optional<EndpointOverride> ParseEndpointOverride(base::StringPiece str) {
  std::vector<base::StringPiece> tokens = base::SplitStringPiece(
      str, "=", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  // There should be 2 parts: the endpoint and the address space.
  if (tokens.size() != 2) {
    return absl::nullopt;
  }

  base::StringPiece endpoint = tokens[0];
  base::StringPiece address_space = tokens[1];

  absl::optional<IPEndPoint> parsed_endpoint = ParseEndpoint(endpoint);
  if (!parsed_endpoint.has_value()) {
    return absl::nullopt;
  }

  absl::optional<IPAddressSpace> parsed_address_space =
      ParseIPAddressSpace(address_space);
  if (!parsed_address_space.has_value()) {
    return absl::nullopt;
  }

  EndpointOverride result;
  result.endpoint = std::move(*parsed_endpoint);
  result.space = *parsed_address_space;
  return result;
}

// Parses a comma-separated list of overrides. Ignores invalid entries.
std::vector<EndpointOverride> ParseEndpointOverrideList(
    base::StringPiece list) {
  // Since we skip invalid entries anyway, we can skip empty entries.
  std::vector<base::StringPiece> tokens = base::SplitStringPiece(
      list, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  std::vector<EndpointOverride> endpoint_overrides;
  for (base::StringPiece token : tokens) {
    absl::optional<EndpointOverride> parsed = ParseEndpointOverride(token);
    if (parsed.has_value()) {
      endpoint_overrides.push_back(*std::move(parsed));
    }
  }

  return endpoint_overrides;
}

// Applies overrides specified on the command-line to `endpoint`.
// Returns nullopt if no override matches `endpoint`.
absl::optional<IPAddressSpace> ApplyCommandLineOverrides(
    const IPEndPoint& endpoint) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (!command_line.HasSwitch(switches::kIpAddressSpaceOverrides)) {
    return absl::nullopt;
  }

  std::string switch_str =
      command_line.GetSwitchValueASCII(switches::kIpAddressSpaceOverrides);
  std::vector<EndpointOverride> endpoint_overrides =
      ParseEndpointOverrideList(switch_str);

  for (const auto& endpoint_override : endpoint_overrides) {
    if (endpoint_override.endpoint == endpoint) {
      return endpoint_override.space;
    }
  }

  return absl::nullopt;
}

// Represents a single entry of the form: subnet -> address space.
class AddressSpaceMapEntry {
 public:
  AddressSpaceMapEntry(IPAddress prefix,
                       size_t prefix_length,
                       IPAddressSpace space)
      : prefix_(std::move(prefix)),
        prefix_length_(prefix_length),
        space_(space) {}

  // Returns the assigned address space if `address` belongs to this instance's
  // subnet. Returns nullopt otherwise.
  absl::optional<IPAddressSpace> Apply(const IPAddress& address) const;

 private:
  IPAddress prefix_;
  size_t prefix_length_ = 0;
  IPAddressSpace space_ = IPAddressSpace::kUnknown;
};

absl::optional<IPAddressSpace> AddressSpaceMapEntry::Apply(
    const IPAddress& address) const {
  if (net::IPAddressMatchesPrefix(address, prefix_, prefix_length_)) {
    return space_;
  }

  return absl::nullopt;
}

// Maps IP addresses to IP address spaces.
class AddressSpaceMap {
 public:
  explicit AddressSpaceMap(std::vector<AddressSpaceMapEntry> entries)
      : entries_(std::move(entries)) {}

  // Applies entries in this map to `address`, in sequential order.
  // Returns the address space of the first matching entry.
  // Returns nullopt if no match is found.
  absl::optional<IPAddressSpace> Apply(const IPAddress& address) const;

 private:
  std::vector<AddressSpaceMapEntry> entries_;
};

absl::optional<IPAddressSpace> AddressSpaceMap::Apply(
    const IPAddress& address) const {
  absl::optional<IPAddressSpace> space;

  for (const AddressSpaceMapEntry& entry : entries_) {
    space = entry.Apply(address);
    if (space.has_value()) {
      break;
    }
  }

  // If we never found a match, `space` is still `nullopt`.
  return space;
}

// Returns a map containing all default-non-public subnets.
const AddressSpaceMap& NonPublicAddressSpaceMap() {
  // For brevity below, otherwise entries do not fit on single lines.
  using Entry = AddressSpaceMapEntry;

  // Have to repeat `AddressSpaceMap` because perfect forwarding does not deal
  // well with initializer lists.
  static const base::NoDestructor<AddressSpaceMap> kMap(AddressSpaceMap({
      // IPv6 Loopback (RFC 4291): ::1/128
      Entry(IPAddress::IPv6Localhost(), 128, IPAddressSpace::kLoopback),
      // IPv6 Unique-local (RFC 4193, RFC 8190): fc00::/7
      Entry(IPAddress(0xfc, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0), 7,
            IPAddressSpace::kLocal),
      // IPv6 Link-local unicast (RFC 4291): fe80::/10
      Entry(IPAddress(0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0), 10,
            IPAddressSpace::kLocal),
      // IPv4 Loopback (RFC 1122): 127.0.0.0/8
      Entry(IPAddress(127, 0, 0, 0), 8, IPAddressSpace::kLoopback),
      // IPv4 Private use (RFC 1918): 10.0.0.0/8, 172.16.0.0/12, 192.168.0.0/16
      Entry(IPAddress(10, 0, 0, 0), 8, IPAddressSpace::kLocal),
      Entry(IPAddress(172, 16, 0, 0), 12, IPAddressSpace::kLocal),
      Entry(IPAddress(192, 168, 0, 0), 16, IPAddressSpace::kLocal),
      // IPv4 Link-local (RFC 3927): 169.254.0.0/16
      Entry(IPAddress(169, 254, 0, 0), 16, IPAddressSpace::kLocal),
  }));
  return *kMap;
}

}  // namespace

IPAddressSpace IPAddressToIPAddressSpace(const IPAddress& address) {
  return NonPublicAddressSpaceMap().Apply(address).value_or(
      IPAddressSpace::kPublic);
}

namespace {

IPAddressSpace IPEndPointToIPAddressSpace(const IPEndPoint& endpoint) {
  if (!endpoint.address().IsValid()) {
    return IPAddressSpace::kUnknown;
  }

  absl::optional<IPAddressSpace> space = ApplyCommandLineOverrides(endpoint);
  if (space.has_value()) {
    return *space;
  }

  return IPAddressToIPAddressSpace(endpoint.address());
}

}  // namespace

base::StringPiece IPAddressSpaceToStringPiece(IPAddressSpace space) {
  switch (space) {
    case IPAddressSpace::kUnknown:
      return "unknown";
    case IPAddressSpace::kPublic:
      return "public";
    case IPAddressSpace::kLocal:
      return "local";
    case IPAddressSpace::kLoopback:
      return "loopback";
  }
}

IPAddressSpace TransportInfoToIPAddressSpace(const net::TransportInfo& info) {
  switch (info.type) {
    case net::TransportType::kDirect:
    case net::TransportType::kCached:
      return IPEndPointToIPAddressSpace(info.endpoint);
    case net::TransportType::kProxied:
    case net::TransportType::kCachedFromProxy:
      return mojom::IPAddressSpace::kUnknown;
  }
}

namespace {

// For comparison purposes, we treat kUnknown the same as kPublic.
IPAddressSpace CollapseUnknown(IPAddressSpace space) {
  if (space == IPAddressSpace::kUnknown) {
    return IPAddressSpace::kPublic;
  }
  return space;
}

}  // namespace

bool IsLessPublicAddressSpace(IPAddressSpace lhs, IPAddressSpace rhs) {
  // Apart from the special case for kUnknown, the built-in comparison operator
  // works just fine. The comment on IPAddressSpace's definition notes that the
  // enum values' ordering matters.
  return CollapseUnknown(lhs) < CollapseUnknown(rhs);
}

namespace {

// Helper for CalculateClientAddressSpace() with the same arguments.
//
// If the response was fetched via service workers, returns the last URL in the
// list. Otherwise returns `request_url`.
//
// See: https://fetch.spec.whatwg.org/#concept-response-url-list
const GURL& ResponseUrl(
    const GURL& request_url,
    absl::optional<CalculateClientAddressSpaceParams> params) {
  if (params.has_value() && !params->url_list_via_service_worker.empty()) {
    return params.value().url_list_via_service_worker.back();
  }
  return request_url;
}

}  // namespace

CalculateClientAddressSpaceParams::CalculateClientAddressSpaceParams(
    const std::vector<GURL>& url_list_via_service_worker,
    const mojom::ParsedHeadersPtr& parsed_headers,
    const net::IPEndPoint& remote_endpoint)
    : url_list_via_service_worker(url_list_via_service_worker),
      parsed_headers(parsed_headers),
      remote_endpoint(remote_endpoint) {}

CalculateClientAddressSpaceParams::~CalculateClientAddressSpaceParams() =
    default;

mojom::IPAddressSpace CalculateClientAddressSpace(
    const GURL& url,
    absl::optional<CalculateClientAddressSpaceParams> params) {
  if (ResponseUrl(url, params).SchemeIsFile()) {
    // See: https://wicg.github.io/cors-rfc1918/#file-url.
    return mojom::IPAddressSpace::kLoopback;
  }

  if (!params.has_value()) {
    return mojom::IPAddressSpace::kUnknown;
  }

  // First, check whether the response forces itself into a public address space
  // as per https://wicg.github.io/cors-rfc1918/#csp.
  DCHECK(params->parsed_headers) << "CalculateIPAddressSpace() called for URL "
                                 << url << " with null parsed_headers.";
  if (ShouldTreatAsPublicAddress(
          params->parsed_headers->content_security_policy)) {
    return mojom::IPAddressSpace::kPublic;
  }

  // Otherwise, calculate the address space via the provided IP address.
  return IPEndPointToIPAddressSpace(params->remote_endpoint);
}

mojom::IPAddressSpace CalculateResourceAddressSpace(
    const GURL& url,
    const net::IPEndPoint& endpoint) {
  if (url.SchemeIsFile()) {
    // See: https://wicg.github.io/cors-rfc1918/#file-url.
    return mojom::IPAddressSpace::kLoopback;
  }

  return IPEndPointToIPAddressSpace(endpoint);
}

}  // namespace network
