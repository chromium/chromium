// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/ip_address_space_util.h"

#include <utility>

#include "base/command_line.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/transport_info.h"
#include "services/network/public/cpp/content_security_policy/content_security_policy.h"
#include "services/network/public/cpp/features.h"
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
std::optional<IPEndPoint> ParseEndpoint(std::string_view str) {
  // Find the last colon character in `str`. We do not use
  // `base::SplitStringPiece()` because IPv6 address literals may contain colon
  // characters too.
  const auto pos = str.rfind(':');
  if (pos == str.npos) {
    return std::nullopt;
  }

  std::string_view address_str = str.substr(0, pos);

  // Skip the colon. Note that this is safe because if `pos` is not `npos`, it
  // is guaranteed to be < `str.size()`, and `substr()` accepts arguments that
  // are <= `str.size()`. In other words, if the colon character is the last in
  // `str`, then `port_str` is assigned "".
  std::string_view port_str = str.substr(pos + 1);

  IPAddress address;
  if (!net::ParseURLHostnameToAddress(address_str, &address)) {
    return std::nullopt;
  }

  // Parse to a `unsigned int`, which is guaranteed to be at least 16 bits wide.
  // See https://en.cppreference.com/w/cpp/language/types.
  unsigned port_unsigned = 0;
  if (!base::StringToUint(port_str, &port_unsigned)) {
    return std::nullopt;
  }

  if (!base::IsValueInRangeForNumericType<uint16_t>(port_unsigned)) {
    return std::nullopt;
  }

  // Use `checked_cast()` for extra safety, though this should never `CHECK()`
  // thanks to the previous call to `IsValueInRangeForNumericType()`.
  uint16_t port = base::checked_cast<uint16_t>(port_unsigned);
  return IPEndPoint(address, port);
}

std::optional<IPAddressSpace> ParseIPAddressSpace(std::string_view str) {
  if (str == "public") {
    return IPAddressSpace::kPublic;
  }

  if (str == "private") {
    return IPAddressSpace::kPrivate;
  }

  if (str == "local") {
    return IPAddressSpace::kLocal;
  }

  return std::nullopt;
}

// Represents a single command-line-specified endpoint override.
struct EndpointOverride {
  // The IP endpoint to override the address space for.
  IPEndPoint endpoint;

  // The IP address space to which `endpoint` should be mapped.
  IPAddressSpace space;
};

// Parses an override from `str`, of the form "<endpoint>=<space>".
std::optional<EndpointOverride> ParseEndpointOverride(std::string_view str) {
  std::vector<std::string_view> tokens = base::SplitStringPiece(
      str, "=", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  // There should be 2 parts: the endpoint and the address space.
  if (tokens.size() != 2) {
    return std::nullopt;
  }

  std::string_view endpoint = tokens[0];
  std::string_view address_space = tokens[1];

  std::optional<IPEndPoint> parsed_endpoint = ParseEndpoint(endpoint);
  if (!parsed_endpoint.has_value()) {
    return std::nullopt;
  }

  std::optional<IPAddressSpace> parsed_address_space =
      ParseIPAddressSpace(address_space);
  if (!parsed_address_space.has_value()) {
    return std::nullopt;
  }

  EndpointOverride result;
  result.endpoint = std::move(*parsed_endpoint);
  result.space = *parsed_address_space;
  return result;
}

// Parses a comma-separated list of overrides. Ignores invalid entries.
std::vector<EndpointOverride> ParseEndpointOverrideList(std::string_view list) {
  // Since we skip invalid entries anyway, we can skip empty entries.
  std::vector<std::string_view> tokens = base::SplitStringPiece(
      list, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  std::vector<EndpointOverride> endpoint_overrides;
  for (std::string_view token : tokens) {
    std::optional<EndpointOverride> parsed = ParseEndpointOverride(token);
    if (parsed.has_value()) {
      endpoint_overrides.push_back(*std::move(parsed));
    }
  }

  return endpoint_overrides;
}

// Applies overrides specified on the command-line to `endpoint`.
// Returns nullopt if no override matches `endpoint`.
std::optional<IPAddressSpace> ApplyCommandLineOverrides(
    const IPEndPoint& endpoint) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (!command_line.HasSwitch(switches::kIpAddressSpaceOverrides)) {
    return std::nullopt;
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

  return std::nullopt;
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
  std::optional<IPAddressSpace> Apply(const IPAddress& address) const;

 private:
  IPAddress prefix_;
  size_t prefix_length_ = 0;
  IPAddressSpace space_ = IPAddressSpace::kUnknown;
};

std::optional<IPAddressSpace> AddressSpaceMapEntry::Apply(
    const IPAddress& address) const {
  if (net::IPAddressMatchesPrefix(address, prefix_, prefix_length_)) {
    return space_;
  }

  return std::nullopt;
}

// Maps IP addresses to IP address spaces.
class AddressSpaceMap {
 public:
  explicit AddressSpaceMap(std::vector<AddressSpaceMapEntry> entries)
      : entries_(std::move(entries)) {}

  // Applies entries in this map to `address`, in sequential order.
  // Returns the address space of the first matching entry.
  // Returns nullopt if no match is found.
  std::optional<IPAddressSpace> Apply(const IPAddress& address) const;

 private:
  std::vector<AddressSpaceMapEntry> entries_;
};

std::optional<IPAddressSpace> AddressSpaceMap::Apply(
    const IPAddress& address) const {
  std::optional<IPAddressSpace> space;

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
      Entry(IPAddress::IPv6Localhost(), 128, IPAddressSpace::kLocal),
      // IPv6 Unique-local (RFC 4193, RFC 8190): fc00::/7
      Entry(IPAddress(0xfc, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0), 7,
            IPAddressSpace::kPrivate),
      // IPv6 Link-local unicast (RFC 4291): fe80::/10
      Entry(IPAddress(0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0), 10,
            IPAddressSpace::kPrivate),
      // IPv4 Loopback (RFC 1122): 127.0.0.0/8
      Entry(IPAddress(127, 0, 0, 0), 8, IPAddressSpace::kLocal),
      // IPv4 Private use (RFC 1918): 10.0.0.0/8, 172.16.0.0/12, 192.168.0.0/16
      Entry(IPAddress(10, 0, 0, 0), 8, IPAddressSpace::kPrivate),
      Entry(IPAddress(172, 16, 0, 0), 12, IPAddressSpace::kPrivate),
      Entry(IPAddress(192, 168, 0, 0), 16, IPAddressSpace::kPrivate),
      // IPv4 Link-local (RFC 3927): 169.254.0.0/16
      Entry(IPAddress(169, 254, 0, 0), 16, IPAddressSpace::kPrivate),
      // IPv4 Null IP (RFC 5735): 0.0.0.0/32 is "this host on this network".
      // Other addresses in 0.0.0.0/8 may refer to "specified hosts on this
      // network". This is somewhat under-defined for the purposes of assigning
      // local vs private address space but we assign 0.0.0.0/32 to "local" and
      // the rest of the block to "private". Note that this mapping can be
      // overridden by a killswitch feature flag in IPAddressToIPAddressSpace()
      // since these addresses were previously treated as public. See
      // https://crbug.com/40058874.
      //
      // TODO(https://crbug.com/40058874): decide if we should do the same for
      // the all-zero IPv6 address.
      Entry(IPAddress(0, 0, 0, 0), 32, IPAddressSpace::kLocal),
      Entry(IPAddress(0, 0, 0, 0), 8, IPAddressSpace::kPrivate),
  }));
  return *kMap;
}

}  // namespace

IPAddressSpace IPAddressToIPAddressSpace(const IPAddress& address) {
  // The null IP block (0.0.0.0/8) was previously treated as public, but this
  // was a loophole in Private Network Access and thus these addresses are now
  // mapped to the local/private address space instead. This feature is a
  // killswitch for this behavior to revert these addresses to the public
  // address space.
  if (base::FeatureList::IsEnabled(
          network::features::kTreatNullIPAsPublicAddressSpace) &&
      address.IsIPv4() &&
      IPAddressMatchesPrefix(address, IPAddress(0, 0, 0, 0), 8)) {
    return IPAddressSpace::kPublic;
  }
  return NonPublicAddressSpaceMap().Apply(address).value_or(
      IPAddressSpace::kPublic);
}

namespace {

IPAddressSpace IPEndPointToIPAddressSpace(const IPEndPoint& endpoint) {
  if (!endpoint.address().IsValid()) {
    return IPAddressSpace::kUnknown;
  }

  std::optional<IPAddressSpace> space = ApplyCommandLineOverrides(endpoint);
  if (space.has_value()) {
    return *space;
  }

  return IPAddressToIPAddressSpace(endpoint.address());
}

}  // namespace

std::string_view IPAddressSpaceToStringPiece(IPAddressSpace space) {
  switch (space) {
    case IPAddressSpace::kUnknown:
      return "unknown";
    case IPAddressSpace::kPublic:
      return "public";
    case IPAddressSpace::kPrivate:
      return "private";
    case IPAddressSpace::kLocal:
      return "local";
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

CalculateClientAddressSpaceParams::~CalculateClientAddressSpaceParams() =
    default;

mojom::IPAddressSpace CalculateClientAddressSpace(
    const GURL& url,
    std::optional<CalculateClientAddressSpaceParams> params) {
  if (params.has_value() &&
      params->client_address_space_inherited_from_service_worker.has_value()) {
    return *params->client_address_space_inherited_from_service_worker;
  }

  if (url.SchemeIsFile()) {
    // See: https://wicg.github.io/cors-rfc1918/#file-url.
    return mojom::IPAddressSpace::kLocal;
  }

  if (!params.has_value()) {
    return mojom::IPAddressSpace::kUnknown;
  }

  // First, check whether the response forces itself into a public address space
  // as per https://wicg.github.io/cors-rfc1918/#csp.
  DCHECK(*params->parsed_headers) << "CalculateIPAddressSpace() called for URL "
                                  << url << " with null parsed_headers.";
  if (ShouldTreatAsPublicAddress(
          (*params->parsed_headers)->content_security_policy)) {
    return mojom::IPAddressSpace::kPublic;
  }

  // Otherwise, calculate the address space via the provided IP address.
  return IPEndPointToIPAddressSpace(*params->remote_endpoint);
}

mojom::IPAddressSpace CalculateResourceAddressSpace(
    const GURL& url,
    const net::IPEndPoint& endpoint) {
  if (url.SchemeIsFile()) {
    // See: https://wicg.github.io/cors-rfc1918/#file-url.
    return mojom::IPAddressSpace::kLocal;
  }

  return IPEndPointToIPAddressSpace(endpoint);
}

}  // namespace network
