// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/base/ip_address.h"

#include <stddef.h>

#include <algorithm>
#include <array>
#include <climits>
#include <optional>
#include <string_view>

#include "base/check_op.h"
#include "base/debug/alias.h"
#include "base/debug/crash_logging.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/values.h"
#include "net/base/parse_number.h"
#include "url/gurl.h"
#include "url/url_canon_ip.h"

namespace net {
namespace {

// The prefix for IPv6 mapped IPv4 addresses.
// https://tools.ietf.org/html/rfc4291#section-2.5.5.2
constexpr uint8_t kIPv4MappedPrefix[] = {0, 0, 0, 0, 0,    0,
                                         0, 0, 0, 0, 0xFF, 0xFF};

// Note that this function assumes:
// * |ip_address| is at least |prefix_length_in_bits| (bits) long;
// * |ip_prefix| is at least |prefix_length_in_bits| (bits) long.
bool IPAddressPrefixCheck(const IPAddressBytes& ip_address,
                          const uint8_t* ip_prefix,
                          size_t prefix_length_in_bits) {
  // Compare all the bytes that fall entirely within the prefix.
  size_t num_entire_bytes_in_prefix = prefix_length_in_bits / 8;
  for (size_t i = 0; i < num_entire_bytes_in_prefix; ++i) {
    if (ip_address[i] != ip_prefix[i])
      return false;
  }

  // In case the prefix was not a multiple of 8, there will be 1 byte
  // which is only partially masked.
  size_t remaining_bits = prefix_length_in_bits % 8;
  if (remaining_bits != 0) {
    uint8_t mask = 0xFF << (8 - remaining_bits);
    size_t i = num_entire_bytes_in_prefix;
    if ((ip_address[i] & mask) != (ip_prefix[i] & mask))
      return false;
  }
  return true;
}

bool CreateIPMask(IPAddressBytes* ip_address,
                  size_t prefix_length_in_bits,
                  size_t ip_address_length) {
  if (ip_address_length != IPAddress::kIPv4AddressSize &&
      ip_address_length != IPAddress::kIPv6AddressSize) {
    return false;
  }
  if (prefix_length_in_bits > ip_address_length * 8) {
    return false;
  }

  ip_address->Resize(ip_address_length);
  size_t idx = 0;
  // Set all fully masked bytes
  size_t num_entire_bytes_in_prefix = prefix_length_in_bits / 8;
  for (size_t i = 0; i < num_entire_bytes_in_prefix; ++i) {
    (*ip_address)[idx++] = 0xff;
  }

  // In case the prefix was not a multiple of 8, there will be 1 byte
  // which is only partially masked.
  size_t remaining_bits = prefix_length_in_bits % 8;
  if (remaining_bits != 0) {
    uint8_t remaining_bits_mask = 0xFF << (8 - remaining_bits);
    (*ip_address)[idx++] = remaining_bits_mask;
  }

  // Zero out any other bytes.
  size_t bytes_remaining = ip_address_length - num_entire_bytes_in_prefix -
                           (remaining_bits != 0 ? 1 : 0);
  for (size_t i = 0; i < bytes_remaining; ++i) {
    (*ip_address)[idx++] = 0;
  }

  return true;
}

// Returns false if |ip_address| matches any of the reserved IPv4 ranges. This
// method operates on a list of reserved IPv4 ranges. Some ranges are
// consolidated.
// Sources for info:
// www.iana.org/assignments/ipv4-address-space/ipv4-address-space.xhtml
// www.iana.org/assignments/iana-ipv4-special-registry/
// iana-ipv4-special-registry.xhtml
bool IsPubliclyRoutableIPv4(const IPAddressBytes& ip_address) {
  // Different IP versions have different range reservations.
  DCHECK_EQ(IPAddress::kIPv4AddressSize, ip_address.size());
  struct {
    const uint8_t address[4];
    size_t prefix_length_in_bits;
  } static const kReservedIPv4Ranges[] = {
      {{0, 0, 0, 0}, 8},      {{10, 0, 0, 0}, 8},     {{100, 64, 0, 0}, 10},
      {{127, 0, 0, 0}, 8},    {{169, 254, 0, 0}, 16}, {{172, 16, 0, 0}, 12},
      {{192, 0, 0, 0}, 24},   {{192, 0, 2, 0}, 24},   {{192, 88, 99, 0}, 24},
      {{192, 168, 0, 0}, 16}, {{198, 18, 0, 0}, 15},  {{198, 51, 100, 0}, 24},
      {{203, 0, 113, 0}, 24}, {{224, 0, 0, 0}, 3}};

  for (const auto& range : kReservedIPv4Ranges) {
    if (IPAddressPrefixCheck(ip_address, range.address,
                             range.prefix_length_in_bits)) {
      return false;
    }
  }

  return true;
}

// Returns false if |ip_address| matches any of the IPv6 ranges IANA reserved
// for local networks. This method operates on an allowlist of non-reserved
// IPv6 ranges, plus the list of reserved IPv4 ranges mapped to IPv6.
// Sources for info:
// www.iana.org/assignments/ipv6-address-space/ipv6-address-space.xhtml
bool IsPubliclyRoutableIPv6(const IPAddressBytes& ip_address) {
  DCHECK_EQ(IPAddress::kIPv6AddressSize, ip_address.size());
  struct {
    const uint8_t address_prefix[2];
    size_t prefix_length_in_bits;
  } static const kPublicIPv6Ranges[] = {// 2000::/3  -- Global Unicast
                                        {{0x20, 0}, 3},
                                        // ff00::/8  -- Multicast
                                        {{0xff, 0}, 8}};

  for (const auto& range : kPublicIPv6Ranges) {
    if (IPAddressPrefixCheck(ip_address, range.address_prefix,
                             range.prefix_length_in_bits)) {
      return true;
    }
  }

  IPAddress addr(ip_address);
  if (addr.IsIPv4MappedIPv6()) {
    IPAddress ipv4 = ConvertIPv4MappedIPv6ToIPv4(addr);
    return IsPubliclyRoutableIPv4(ipv4.bytes());
  }

  return false;
}

bool ParseIPLiteralToBytes(std::string_view ip_literal, IPAddressBytes* bytes) {
  // |ip_literal| could be either an IPv4 or an IPv6 literal. If it contains
  // a colon however, it must be an IPv6 address.
  if (ip_literal.find(':') != std::string_view::npos) {
    // GURL expects IPv6 hostnames to be surrounded with brackets.
    std::string host_brackets = base::StrCat({"[", ip_literal, "]"});
    url::Component host_comp(0, host_brackets.size());

    // Try parsing the hostname as an IPv6 literal.
    bytes->Resize(16);  // 128 bits.
    return url::IPv6AddressToNumber(host_brackets.data(), host_comp,
                                    bytes->data());
  }

  // Otherwise the string is an IPv4 address.
  bytes->Resize(4);  // 32 bits.
  url::Component host_comp(0, ip_literal.size());
  int num_components;
  url::CanonHostInfo::Family family = url::IPv4AddressToNumber(
      ip_literal.data(), host_comp, bytes->data(), &num_components);
  return family == url::CanonHostInfo::IPV4;
}

}  // namespace

IPAddressBytes::IPAddressBytes() : size_(0) {}

IPAddressBytes::IPAddressBytes(base::span<const uint8_t> data) {
  Assign(data);
}

IPAddressBytes::~IPAddressBytes() = default;
IPAddressBytes::IPAddressBytes(IPAddressBytes const& other) = default;

void IPAddressBytes::Assign(base::span<const uint8_t> data) {
  CHECK_GE(16u, data.size());
  size_ = data.size();
  base::span(*this).copy_from(data);
}

bool IPAddressBytes::operator<(const IPAddressBytes& other) const {
  if (size_ == other.size_)
    return std::lexicographical_compare(begin(), end(), other.begin(),
                                        other.end());
  return size_ < other.size_;
}

bool IPAddressBytes::operator==(const IPAddressBytes& other) const {
  return base::ranges::equal(*this, other);
}

bool IPAddressBytes::operator!=(const IPAddressBytes& other) const {
  return !(*this == other);
}

void IPAddressBytes::Append(base::span<const uint8_t> data) {
  CHECK_LE(data.size(), static_cast<size_t>(16 - size_));
  size_ += data.size();
  base::span(*this).last(data.size()).copy_from(data);
}

size_t IPAddressBytes::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(bytes_);
}

// static
std::optional<IPAddress> IPAddress::FromValue(const base::Value& value) {
  if (!value.is_string()) {
    return std::nullopt;
  }

  return IPAddress::FromIPLiteral(value.GetString());
}

// static
std::optional<IPAddress> IPAddress::FromIPLiteral(std::string_view ip_literal) {
  IPAddress address;
  if (!address.AssignFromIPLiteral(ip_literal)) {
    return std::nullopt;
  }
  DCHECK(address.IsValid());
  return address;
}

IPAddress::IPAddress() = default;

IPAddress::IPAddress(const IPAddress& other) = default;

IPAddress::IPAddress(const IPAddressBytes& address) : ip_address_(address) {}

IPAddress::IPAddress(base::span<const uint8_t> address)
    : ip_address_(address) {}

IPAddress::IPAddress(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3) {
  const uint8_t bytes[] = {b0, b1, b2, b3};
  ip_address_.Assign(bytes);
}

IPAddress::IPAddress(uint8_t b0,
                     uint8_t b1,
                     uint8_t b2,
                     uint8_t b3,
                     uint8_t b4,
                     uint8_t b5,
                     uint8_t b6,
                     uint8_t b7,
                     uint8_t b8,
                     uint8_t b9,
                     uint8_t b10,
                     uint8_t b11,
                     uint8_t b12,
                     uint8_t b13,
                     uint8_t b14,
                     uint8_t b15) {
  const uint8_t bytes[] = {b0, b1, b2,  b3,  b4,  b5,  b6,  b7,
                           b8, b9, b10, b11, b12, b13, b14, b15};
  ip_address_.Assign(bytes);
}

IPAddress::~IPAddress() = default;

bool IPAddress::IsIPv4() const {
  return ip_address_.size() == kIPv4AddressSize;
}

bool IPAddress::IsIPv6() const {
  return ip_address_.size() == kIPv6AddressSize;
}

bool IPAddress::IsValid() const {
  return IsIPv4() || IsIPv6();
}

bool IPAddress::IsPubliclyRoutable() const {
  if (IsIPv4()) {
    return IsPubliclyRoutableIPv4(ip_address_);
  } else if (IsIPv6()) {
    return IsPubliclyRoutableIPv6(ip_address_);
  }
  return true;
}

bool IPAddress::IsZero() const {
  for (auto x : ip_address_) {
    if (x != 0)
      return false;
  }

  return !empty();
}

bool IPAddress::IsIPv4MappedIPv6() const {
  return IsIPv6() && IPAddressStartsWith(*this, kIPv4MappedPrefix);
}

bool IPAddress::IsLoopback() const {
  // 127.0.0.1/8
  if (IsIPv4())
    return ip_address_[0] == 127;

  // ::1
  if (IsIPv6()) {
    for (size_t i = 0; i + 1 < ip_address_.size(); ++i) {
      if (ip_address_[i] != 0)
        return false;
    }
    return ip_address_.back() == 1;
  }

  return false;
}

bool IPAddress::IsLinkLocal() const {
  // 169.254.0.0/16
  if (IsIPv4())
    return (ip_address_[0] == 169) && (ip_address_[1] == 254);

  // [::ffff:169.254.0.0]/112
  if (IsIPv4MappedIPv6())
    return (ip_address_[12] == 169) && (ip_address_[13] == 254);

  // [fe80::]/10
  if (IsIPv6())
    return (ip_address_[0] == 0xFE) && ((ip_address_[1] & 0xC0) == 0x80);

  return false;
}

bool IPAddress::IsUniqueLocalIPv6() const {
  // [fc00::]/7
  return IsIPv6() && ((ip_address_[0] & 0xFE) == 0xFC);
}

bool IPAddress::AssignFromIPLiteral(std::string_view ip_literal) {
  bool success = ParseIPLiteralToBytes(ip_literal, &ip_address_);
  if (!success)
    ip_address_.Resize(0);
  return success;
}

std::vector<uint8_t> IPAddress::CopyBytesToVector() const {
  return std::vector<uint8_t>(ip_address_.begin(), ip_address_.end());
}

// static
IPAddress IPAddress::IPv4Localhost() {
  static const uint8_t kLocalhostIPv4[] = {127, 0, 0, 1};
  return IPAddress(kLocalhostIPv4);
}

// static
IPAddress IPAddress::IPv6Localhost() {
  static const uint8_t kLocalhostIPv6[] = {0, 0, 0, 0, 0, 0, 0, 0,
                                           0, 0, 0, 0, 0, 0, 0, 1};
  return IPAddress(kLocalhostIPv6);
}

// static
IPAddress IPAddress::AllZeros(size_t num_zero_bytes) {
  CHECK_LE(num_zero_bytes, 16u);
  IPAddress result;
  for (size_t i = 0; i < num_zero_bytes; ++i)
    result.ip_address_.push_back(0u);
  return result;
}

// static
IPAddress IPAddress::IPv4AllZeros() {
  return AllZeros(kIPv4AddressSize);
}

// static
IPAddress IPAddress::IPv6AllZeros() {
  return AllZeros(kIPv6AddressSize);
}

// static
bool IPAddress::CreateIPv4Mask(IPAddress* ip_address,
                               size_t mask_prefix_length) {
  return CreateIPMask(&(ip_address->ip_address_), mask_prefix_length,
                      kIPv4AddressSize);
}

// static
bool IPAddress::CreateIPv6Mask(IPAddress* ip_address,
                               size_t mask_prefix_length) {
  return CreateIPMask(&(ip_address->ip_address_), mask_prefix_length,
                      kIPv6AddressSize);
}

bool IPAddress::operator==(const IPAddress& that) const {
  return ip_address_ == that.ip_address_;
}

bool IPAddress::operator!=(const IPAddress& that) const {
  return ip_address_ != that.ip_address_;
}

bool IPAddress::operator<(const IPAddress& that) const {
  // Sort IPv4 before IPv6.
  if (ip_address_.size() != that.ip_address_.size()) {
    return ip_address_.size() < that.ip_address_.size();
  }

  return ip_address_ < that.ip_address_;
}

std::string IPAddress::ToString() const {
  std::string str;
  url::StdStringCanonOutput output(&str);

  if (IsIPv4()) {
    url::AppendIPv4Address(ip_address_.data(), &output);
  } else if (IsIPv6()) {
    url::AppendIPv6Address(ip_address_.data(), &output);
  }

  output.Complete();
  return str;
}

base::Value IPAddress::ToValue() const {
  DCHECK(IsValid());
  return base::Value(ToString());
}

size_t IPAddress::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(ip_address_);
}

std::string IPAddressToStringWithPort(const IPAddress& address, uint16_t port) {
  std::string address_str = address.ToString();
  if (address_str.empty())
    return address_str;

  if (address.IsIPv6()) {
    // Need to bracket IPv6 addresses since they contain colons.
    return base::StringPrintf("[%s]:%d", address_str.c_str(), port);
  }
  return base::StringPrintf("%s:%d", address_str.c_str(), port);
}

std::string IPAddressToPackedString(const IPAddress& address) {
  return std::string(reinterpret_cast<const char*>(address.bytes().data()),
                     address.size());
}

IPAddress ConvertIPv4ToIPv4MappedIPv6(const IPAddress& address) {
  CHECK(address.IsIPv4());
  // IPv4-mapped addresses are formed by:
  // <80 bits of zeros>  + <16 bits of ones> + <32-bit IPv4 address>.
  IPAddressBytes bytes;
  bytes.Append(kIPv4MappedPrefix);
  bytes.Append(address.bytes());
  return IPAddress(bytes);
}

IPAddress ConvertIPv4MappedIPv6ToIPv4(const IPAddress& address) {
  DCHECK(address.IsIPv4MappedIPv6());

  IPAddressBytes bytes;
  bytes.Append(
      base::span(address.bytes()).subspan(std::size(kIPv4MappedPrefix)));
  return IPAddress(bytes);
}

bool IPAddressMatchesPrefix(const IPAddress& ip_address,
                            const IPAddress& ip_prefix,
                            size_t prefix_length_in_bits) {
  // Both the input IP address and the prefix IP address should be either IPv4
  // or IPv6.
  CHECK(ip_address.IsValid());
  CHECK(ip_prefix.IsValid());

  CHECK_LE(prefix_length_in_bits, ip_prefix.size() * 8);

  // In case we have an IPv6 / IPv4 mismatch, convert the IPv4 addresses to
  // IPv6 addresses in order to do the comparison.
  if (ip_address.size() != ip_prefix.size()) {
    if (ip_address.IsIPv4()) {
      return IPAddressMatchesPrefix(ConvertIPv4ToIPv4MappedIPv6(ip_address),
                                    ip_prefix, prefix_length_in_bits);
    }
    return IPAddressMatchesPrefix(ip_address,
                                  ConvertIPv4ToIPv4MappedIPv6(ip_prefix),
                                  96 + prefix_length_in_bits);
  }

  return IPAddressPrefixCheck(ip_address.bytes(), ip_prefix.bytes().data(),
                              prefix_length_in_bits);
}

bool ParseCIDRBlock(std::string_view cidr_literal,
                    IPAddress* ip_address,
                    size_t* prefix_length_in_bits) {
  // We expect CIDR notation to match one of these two templates:
  //   <IPv4-literal> "/" <number of bits>
  //   <IPv6-literal> "/" <number of bits>

  std::vector<std::string_view> parts = base::SplitStringPiece(
      cidr_literal, "/", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (parts.size() != 2)
    return false;

  // Parse the IP address.
  if (!ip_address->AssignFromIPLiteral(parts[0]))
    return false;

  // Parse the prefix length.
  uint32_t number_of_bits;
  if (!ParseUint32(parts[1], ParseIntFormat::NON_NEGATIVE, &number_of_bits)) {
    return false;
  }

  // Make sure the prefix length is in a valid range.
  if (number_of_bits > ip_address->size() * 8)
    return false;

  *prefix_length_in_bits = number_of_bits;
  return true;
}

bool ParseURLHostnameToAddress(std::string_view hostname,
                               IPAddress* ip_address) {
  if (hostname.size() >= 2 && hostname.front() == '[' &&
      hostname.back() == ']') {
    // Strip the square brackets that surround IPv6 literals.
    auto ip_literal = std::string_view(hostname).substr(1, hostname.size() - 2);
    return ip_address->AssignFromIPLiteral(ip_literal) && ip_address->IsIPv6();
  }

  return ip_address->AssignFromIPLiteral(hostname) && ip_address->IsIPv4();
}

size_t CommonPrefixLength(const IPAddress& a1, const IPAddress& a2) {
  DCHECK_EQ(a1.size(), a2.size());
  for (size_t i = 0; i < a1.size(); ++i) {
    unsigned diff = a1.bytes()[i] ^ a2.bytes()[i];
    if (!diff)
      continue;
    for (unsigned j = 0; j < CHAR_BIT; ++j) {
      if (diff & (1 << (CHAR_BIT - 1)))
        return i * CHAR_BIT + j;
      diff <<= 1;
    }
    NOTREACHED_IN_MIGRATION();
  }
  return a1.size() * CHAR_BIT;
}

size_t MaskPrefixLength(const IPAddress& mask) {
  IPAddressBytes all_ones;
  all_ones.Resize(mask.size());
  std::fill(all_ones.begin(), all_ones.end(), 0xFF);
  return CommonPrefixLength(mask, IPAddress(all_ones));
}

Dns64PrefixLength ExtractPref64FromIpv4onlyArpaAAAA(const IPAddress& address) {
  DCHECK(address.IsIPv6());
  IPAddress ipv4onlyarpa0(192, 0, 0, 170);
  IPAddress ipv4onlyarpa1(192, 0, 0, 171);
  auto span = base::span(address.bytes());

  if (base::ranges::equal(ipv4onlyarpa0.bytes(), span.subspan(12u)) ||
      base::ranges::equal(ipv4onlyarpa1.bytes(), span.subspan(12u))) {
    return Dns64PrefixLength::k96bit;
  }
  if (base::ranges::equal(ipv4onlyarpa0.bytes(), span.subspan(9u, 4u)) ||
      base::ranges::equal(ipv4onlyarpa1.bytes(), span.subspan(9u, 4u))) {
    return Dns64PrefixLength::k64bit;
  }
  IPAddressBytes ipv4;
  ipv4.Append(span.subspan(7u, 1u));
  ipv4.Append(span.subspan(9u, 3u));
  if (base::ranges::equal(ipv4onlyarpa0.bytes(), ipv4) ||
      base::ranges::equal(ipv4onlyarpa1.bytes(), ipv4)) {
    return Dns64PrefixLength::k56bit;
  }
  ipv4 = IPAddressBytes();
  ipv4.Append(span.subspan(6u, 2u));
  ipv4.Append(span.subspan(9u, 2u));
  if (base::ranges::equal(ipv4onlyarpa0.bytes(), ipv4) ||
      base::ranges::equal(ipv4onlyarpa1.bytes(), ipv4)) {
    return Dns64PrefixLength::k48bit;
  }
  ipv4 = IPAddressBytes();
  ipv4.Append(span.subspan(5u, 3u));
  ipv4.Append(span.subspan(9u, 1u));
  if (base::ranges::equal(ipv4onlyarpa0.bytes(), ipv4) ||
      base::ranges::equal(ipv4onlyarpa1.bytes(), ipv4)) {
    return Dns64PrefixLength::k40bit;
  }
  if (base::ranges::equal(ipv4onlyarpa0.bytes(), span.subspan(4u, 4u)) ||
      base::ranges::equal(ipv4onlyarpa1.bytes(), span.subspan(4u, 4u))) {
    return Dns64PrefixLength::k32bit;
  }
  // if ipv4onlyarpa address is not found return 0
  return Dns64PrefixLength::kInvalid;
}

IPAddress ConvertIPv4ToIPv4EmbeddedIPv6(const IPAddress& ipv4_address,
                                        const IPAddress& ipv6_address,
                                        Dns64PrefixLength prefix_length) {
  DCHECK(ipv4_address.IsIPv4());
  DCHECK(ipv6_address.IsIPv6());

  IPAddressBytes bytes;

  constexpr uint8_t kZeroBits[8] = {0x00, 0x00, 0x00, 0x00,
                                    0x00, 0x00, 0x00, 0x00};

  switch (prefix_length) {
    case Dns64PrefixLength::k96bit:
      bytes.Append(base::span(ipv6_address.bytes()).first(12u));
      bytes.Append(ipv4_address.bytes());
      return IPAddress(bytes);
    case Dns64PrefixLength::k64bit:
      bytes.Append(base::span(ipv6_address.bytes()).first(8u));
      bytes.Append(base::span(kZeroBits).first(1u));
      bytes.Append(ipv4_address.bytes());
      bytes.Append(base::span(kZeroBits).first(3u));
      return IPAddress(bytes);
    case Dns64PrefixLength::k56bit: {
      bytes.Append(base::span(ipv6_address.bytes()).first(7u));
      auto [first, second] = base::span(ipv4_address.bytes()).split_at(1u);
      bytes.Append(first);
      bytes.Append(base::span(kZeroBits).first(1u));
      bytes.Append(second);
      bytes.Append(base::span(kZeroBits).first(4u));
      return IPAddress(bytes);
    }
    case Dns64PrefixLength::k48bit: {
      bytes.Append(base::span(ipv6_address.bytes()).first(6u));
      auto [first, second] = base::span(ipv4_address.bytes()).split_at(2u);
      bytes.Append(first);
      bytes.Append(base::span(kZeroBits).first(1u));
      bytes.Append(second);
      bytes.Append(base::span(kZeroBits).first(5u));
      return IPAddress(bytes);
    }
    case Dns64PrefixLength::k40bit: {
      bytes.Append(base::span(ipv6_address.bytes()).first(5u));
      auto [first, second] = base::span(ipv4_address.bytes()).split_at(3u);
      bytes.Append(first);
      bytes.Append(base::span(kZeroBits).first(1u));
      bytes.Append(second);
      bytes.Append(base::span(kZeroBits).first(6u));
      return IPAddress(bytes);
    }
    case Dns64PrefixLength::k32bit:
      bytes.Append(base::span(ipv6_address.bytes()).first(4u));
      bytes.Append(ipv4_address.bytes());
      bytes.Append(base::span(kZeroBits).first(8u));
      return IPAddress(bytes);
    case Dns64PrefixLength::kInvalid:
      return ipv4_address;
  }
}

}  // namespace net
