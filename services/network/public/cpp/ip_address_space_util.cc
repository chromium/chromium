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
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom-shared.h"
#include "url/gurl.h"

namespace network {
namespace {

using mojom::IPAddressSpace;
using net::IPAddress;
using net::IPEndPoint;
using CIDR = IPAddressSpaceOverrides::CIDR;
using AddressSpaceOverride = IPAddressSpaceOverrides::AddressSpaceOverride;

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

// Parses an IP block specifier from CIDR notation, with the IP prefix in a
// URL-safe format (e.g. `[2001::]/16`).  Unlike ParseEndpoint, no port is
// specified.
std::optional<CIDR> ParseCIDR(std::string_view str) {
  size_t mask_bits;
  std::optional<IPAddress> ip_address =
      net::ParseCIDRBlockNonStandardURLFormat(str, &mask_bits);
  if (!ip_address) {
    return std::nullopt;
  }

  CIDR cidr(std::move(*ip_address), mask_bits);
  return cidr;
  // cidr.ip_address = std::move(ip_address);
  // return std::make_optional({std::move(*ip_address), mask_bits});
}

std::optional<IPAddressSpace> ParseIPAddressSpace(std::string_view str) {
  if (str == "public") {
    return IPAddressSpace::kPublic;
  }

  // Keep 'private' as an alias for 'local' until usages of 'private' are
  // removed from Web Platform Test code base.
  //
  // TODO(crbug.com/418737577): remove private alias after Web Platform Test
  // code base moves to using "local"
  if (str == "private") {
    return IPAddressSpace::kLocal;
  }

  if (str == "local") {
    return IPAddressSpace::kLocal;
  }

  if (str == "loopback") {
    return IPAddressSpace::kLoopback;
  }

  return std::nullopt;
}

// Parses an override from `str`, of the form "<endpoint|range>=<space>".
std::optional<AddressSpaceOverride> ParseAddressSpaceOverride(
    std::string_view str) {
  std::vector<std::string_view> tokens = base::SplitStringPiece(
      str, "=", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  // There should be 2 parts: the CIDR/endpoint and the address space.
  if (tokens.size() != 2) {
    return std::nullopt;
  }

  std::string_view endpoint_or_cidr = tokens[0];
  std::string_view address_space = tokens[1];

  std::optional<IPAddressSpace> parsed_address_space =
      ParseIPAddressSpace(address_space);
  if (!parsed_address_space.has_value()) {
    return std::nullopt;
  }

  std::optional<IPEndPoint> parsed_endpoint = ParseEndpoint(endpoint_or_cidr);
  if (parsed_endpoint.has_value()) {
    AddressSpaceOverride result;
    result.endpoint = std::move(*parsed_endpoint);
    result.space = *parsed_address_space;
    return result;
  }

  std::optional<CIDR> parsed_cidr = ParseCIDR(endpoint_or_cidr);
  if (parsed_cidr.has_value()) {
    AddressSpaceOverride result;
    result.cidr = std::move(*parsed_cidr);
    result.space = *parsed_address_space;
    return result;
  }

  return std::nullopt;
}

std::optional<IPAddressSpace> ApplyOverrides(
    const IPEndPoint& endpoint,
    std::vector<AddressSpaceOverride>& address_space_overrides) {
  for (const auto& address_space_override : address_space_overrides) {
    if (address_space_override.endpoint) {
      if (address_space_override.endpoint == endpoint) {
        return address_space_override.space;
      }
      if ((address_space_override.endpoint->port() == 0) &&
          (address_space_override.endpoint->address() == endpoint.address())) {
        return address_space_override.space;
      }
    } else if (address_space_override.cidr) {
      // net::IPAddressMatchesPrefix will automatically convert IPv4 address to
      // IPv4-mapped IPv6 addresses if only one of the address is IPv4. To avoid
      // overrides in this case, a check is added to ensure that overrides only
      // occur when both addresses are IPv4 or both are IPv6.
      if (endpoint.address().IsIPv4() ==
              address_space_override.cidr->ip_address.IsIPv4() &&

          net::IPAddressMatchesPrefix(endpoint.address(),
                                      address_space_override.cidr->ip_address,
                                      address_space_override.cidr->mask_bits)) {
        return address_space_override.space;
      }
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
      // IPv4 Null IP (RFC 5735): 0.0.0.0/32 is "this host on this network".
      // Other addresses in 0.0.0.0/8 may refer to "specified hosts on this
      // network". This is somewhat under-defined for the purposes of assigning
      // local vs loopback address space but we assign 0.0.0.0/32 to "loopback"
      // and the rest of the block to "local". Note that this mapping can be
      // overridden by a killswitch feature flag in IPAddressToIPAddressSpace()
      // since these addresses were previously treated as public. See
      // https://crbug.com/40058874.
      Entry(IPAddress(0, 0, 0, 0), 32, IPAddressSpace::kLoopback),
      Entry(IPAddress(0, 0, 0, 0), 8, IPAddressSpace::kLocal),
      // IPv6 Null IP (RFC 1884): ::/128 is the unspecified address, but many
      // documentation sources consider it to be treated the same as 0.0.0.0/32,
      // so we map it to "loopback" out of an abundance of caution. RFC 1884
      // specifies that the unspecified address "must never be assigned to any
      // node" and "must not be used as the destination address of IPv6
      // datagrams".
      Entry(IPAddress(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0), 128,
            IPAddressSpace::kLoopback),
      // Carrier Grade NAT (RFC 6598): 100.64.0.0/10
      Entry(IPAddress(100, 64, 0, 0), 10, IPAddressSpace::kLocal),
      // IPv6 Documentation Address Prefixes (RFC 3849, RFC 9637): 2001:db8::/32
      // and 3fff::/20 are reserved for documrentation purposes, and they *must
      // not* be routed to the public (and in general should not be used for
      // production traffic). We include them for completeness. See
      // https://github.com/WICG/local-network-access/issues/15.
      Entry(
          IPAddress(0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
          32, IPAddressSpace::kLocal),
      Entry(IPAddress(0x3f, 0xff, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0), 20,
            IPAddressSpace::kLocal),
      // IPv6 Site Local Unicast (RFC 3513): fec0::/10
      // These are deprecated by RFC3879 but still allowed to be used.
      // See https://github.com/WICG/local-network-access/issues/15
      Entry(IPAddress(0xfe, 0xc0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0), 10,
            IPAddressSpace::kLocal),
  }));
  return *kMap;
}

}  // namespace

IPAddressSpace IPAddressToIPAddressSpace(const IPAddress& address) {
  return NonPublicAddressSpaceMap().Apply(address).value_or(
      IPAddressSpace::kPublic);
}

IPAddressSpace IPEndPointToIPAddressSpace(const IPEndPoint& endpoint) {
  if (!endpoint.address().IsValid()) {
    return IPAddressSpace::kUnknown;
  }

  std::optional<IPAddressSpace> space =
      IPAddressSpaceOverrides::GetInstance().HasOverride(endpoint);
  if (space.has_value()) {
    return *space;
  }

  return IPAddressToIPAddressSpace(endpoint.address());
}

std::string_view LocalNetworkAccessResultToStringPiece(
    mojom::LocalNetworkAccessResult result) {
  switch (result) {
    case mojom::LocalNetworkAccessResult::kGranted:
      return "granted";
    case mojom::LocalNetworkAccessResult::kDenied:
      return "denied";
    case mojom::LocalNetworkAccessResult::kRetryDueToCache:
      return "retryDueToCache";
  }
  // In case enum value gets corrupted.
  return "unknown";
}

std::string_view TransportTypeToStringPiece(
    mojom::TransportType transport_type) {
  switch (transport_type) {
    case mojom::TransportType::kDirect:
      return "direct";
    case mojom::TransportType::kProxied:
      return "proxied";
    case mojom::TransportType::kCached:
      return "cached";
    case mojom::TransportType::kCachedFromProxy:
      return "cachedFromProxy";
  }
  // In case enum value gets corrupted.
  return "unknown";
}

std::string_view IPAddressSpaceToStringPiece(IPAddressSpace space) {
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
  // In case enum value gets corrupted.
  return "unknown";
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

// For comparison purposes, we treat kLocal and kLoopback as equivalent
// (kLocal arbitrarily chosen over kLoopback).
IPAddressSpace CollapseLocalAndLoopback(IPAddressSpace space) {
  if (space == IPAddressSpace::kLoopback) {
    return IPAddressSpace::kLocal;
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

bool IsLessPublicAddressSpaceLNA(IPAddressSpace lhs, IPAddressSpace rhs) {
  // Similar to IsLessPublicAddressSpace but with additional collapsing of
  // kLocal and kLoopback.
  return CollapseLocalAndLoopback(CollapseUnknown(lhs)) <
         CollapseLocalAndLoopback(CollapseUnknown(rhs));
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
    return mojom::IPAddressSpace::kLoopback;
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
    return mojom::IPAddressSpace::kLoopback;
  }

  return IPEndPointToIPAddressSpace(endpoint);
}

std::optional<net::IPAddress> ParsePrivateIpFromUrl(const GURL& url) {
  net::IPAddress address;
  if (!address.AssignFromIPLiteral(url.HostNoBracketsPiece())) {
    return std::nullopt;
  }

  if (IPAddressToIPAddressSpace(address) != mojom::IPAddressSpace::kLocal) {
    return std::nullopt;
  }

  return address;
}

std::optional<mojom::IPAddressSpace> GetAddressSpaceFromUrl(const GURL& url) {
  if (url.DomainIs("local")) {
    return mojom::IPAddressSpace::kLocal;
  }

  if (url.DomainIs("localhost")) {
    // Check IP address space mapping for 127.0.0.1, on the off chance that
    // there is an override remapping this to something else.
    net::IPEndPoint endpoint(net::IPAddress::IPv4Localhost(),
                             url.EffectiveIntPort());
    return IPEndPointToIPAddressSpace(endpoint);
  }

  net::IPAddress address;
  if (!address.AssignFromIPLiteral(url.HostNoBracketsPiece())) {
    return std::nullopt;
  }
  net::IPEndPoint endpoint(address, url.EffectiveIntPort());
  return IPEndPointToIPAddressSpace(endpoint);
}

IPAddressSpaceOverrides& IPAddressSpaceOverrides::GetInstance() {
  static base::NoDestructor<IPAddressSpaceOverrides> s_instance;
  return *s_instance;
}

IPAddressSpaceOverrides::IPAddressSpaceOverrides() = default;

IPAddressSpaceOverrides::AddressSpaceOverride::AddressSpaceOverride() = default;
IPAddressSpaceOverrides::AddressSpaceOverride::~AddressSpaceOverride() =
    default;

IPAddressSpaceOverrides::AddressSpaceOverride::AddressSpaceOverride(
    const AddressSpaceOverride&) = default;
IPAddressSpaceOverrides::AddressSpaceOverride&
IPAddressSpaceOverrides::AddressSpaceOverride::operator=(
    const AddressSpaceOverride& other) = default;
IPAddressSpaceOverrides::AddressSpaceOverride::AddressSpaceOverride(
    AddressSpaceOverride&&) = default;
IPAddressSpaceOverrides::AddressSpaceOverride&
IPAddressSpaceOverrides::AddressSpaceOverride::operator=(
    AddressSpaceOverride&& other) = default;

void IPAddressSpaceOverrides::SetAuxiliaryOverrides(
    const std::string& auxiliary_overrides,
    std::vector<std::string>* rejected_patterns) {
  // Ignore empty entries
  std::vector<std::string_view> tokens =
      base::SplitStringPiece(auxiliary_overrides, ",", base::TRIM_WHITESPACE,
                             base::SPLIT_WANT_NONEMPTY);

  std::vector<AddressSpaceOverride> address_space_overrides;
  std::vector<std::string> address_space_overrides_str;
  for (std::string_view token : tokens) {
    std::optional<AddressSpaceOverride> parsed =
        ParseAddressSpaceOverride(token);
    if (parsed.has_value()) {
      address_space_overrides.push_back(*std::move(parsed));
      address_space_overrides_str.emplace_back(token);
    } else {
      rejected_patterns->emplace_back(token);
    }
  }

  base::AutoLock lock(lock_);

  auxiliary_overrides_ = std::move(address_space_overrides_str);
  parsed_auxiliary_overrides_ = std::move(address_space_overrides);
}

void IPAddressSpaceOverrides::ParseCmdlineIfNeeded() {
  lock_.AssertAcquired();
  if (has_cmdline_been_parsed_) {
    return;
  }

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  std::string cmdline_overrides_str = "";
  if (command_line.HasSwitch(switches::kIpAddressSpaceOverrides)) {
    cmdline_overrides_str =
        command_line.GetSwitchValueASCII(switches::kIpAddressSpaceOverrides);
  }

  std::vector<std::string_view> tokens =
      base::SplitStringPiece(cmdline_overrides_str, ",", base::TRIM_WHITESPACE,
                             base::SPLIT_WANT_NONEMPTY);

  for (std::string_view token : tokens) {
    std::optional<AddressSpaceOverride> parsed =
        ParseAddressSpaceOverride(token);
    if (parsed.has_value()) {
      parsed_cmdline_overrides_.push_back(*std::move(parsed));
      cmdline_overrides_.emplace_back(token);
    }
  }

  has_cmdline_been_parsed_ = true;
}

void IPAddressSpaceOverrides::ResetForTesting() {
  base::AutoLock lock(lock_);

  cmdline_overrides_.clear();
  parsed_cmdline_overrides_.clear();
  has_cmdline_been_parsed_ = false;

  auxiliary_overrides_.clear();
  parsed_auxiliary_overrides_.clear();
}

std::optional<IPAddressSpace> IPAddressSpaceOverrides::HasOverride(
    const IPEndPoint& endpoint) {
  base::AutoLock lock(lock_);
  ParseCmdlineIfNeeded();
  auto cmdline_address_space_override =
      ApplyOverrides(endpoint, parsed_cmdline_overrides_);
  if (cmdline_address_space_override) {
    return cmdline_address_space_override;
  }

  auto auxiliary_address_space_override =
      ApplyOverrides(endpoint, parsed_auxiliary_overrides_);
  if (auxiliary_address_space_override) {
    return auxiliary_address_space_override;
  }
  return std::nullopt;
}

std::vector<std::string> IPAddressSpaceOverrides::GetCurrentOverrides() {
  base::AutoLock lock(lock_);
  ParseCmdlineIfNeeded();

  std::vector<std::string> result;
  result.reserve(cmdline_overrides_.size() + auxiliary_overrides_.size());
  std::ranges::copy(cmdline_overrides_, std::back_inserter(result));
  std::ranges::copy(auxiliary_overrides_, std::back_inserter(result));
  return result;
}

}  // namespace network
