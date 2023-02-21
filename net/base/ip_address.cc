// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/ip_address.h"

#include <algorithm>
#include <climits>

#include "base/check_op.h"
#include "base/containers/stack_container.h"
#include "base/debug/alias.h"
#include "base/debug/crash_logging.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/values.h"
#include "net/base/parse_number.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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

bool ParseIPLiteralToBytes(base::StringPiece ip_literal,
                           IPAddressBytes* bytes) {
  // |ip_literal| could be either an IPv4 or an IPv6 literal. If it contains
  // a colon however, it must be an IPv6 address.
  if (ip_literal.find(':') != base::StringPiece::npos) {
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

IPAddressBytes::IPAddressBytes(const uint8_t* data, size_t data_len) {
  Assign(data, data_len);
}

IPAddressBytes::~IPAddressBytes() = default;
IPAddressBytes::IPAddressBytes(IPAddressBytes const& other) = default;

void IPAddressBytes::Assign(const uint8_t* data, size_t data_len) {
  size_ = data_len;
  CHECK_GE(16u, data_len);
  std::copy_n(data, data_len, bytes_.data());
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

size_t IPAddressBytes::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(bytes_);
}

// static
absl::optional<IPAddress> IPAddress::FromValue(const base::Value& value) {
  if (!value.is_string()) {
    return absl::nullopt;
  }

  return IPAddress::FromIPLiteral(value.GetString());
}

// static
absl::optional<IPAddress> IPAddress::FromIPLiteral(
    base::StringPiece ip_literal) {
  IPAddress address;
  if (!address.AssignFromIPLiteral(ip_literal)) {
    return absl::nullopt;
  }
  DCHECK(address.IsValid());
  return address;
}

IPAddress::IPAddress() = default;

IPAddress::IPAddress(const IPAddress& other) = default;

IPAddress::IPAddress(const IPAddressBytes& address) : ip_address_(address) {}

IPAddress::IPAddress(const uint8_t* address, size_t address_len)
    : ip_address_(address, address_len) {}

IPAddress::IPAddress(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3) {
  ip_address_.push_back(b0);
  ip_address_.push_back(b1);
  ip_address_.push_back(b2);
  ip_address_.push_back(b3);
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
  ip_address_.push_back(b0);
  ip_address_.push_back(b1);
  ip_address_.push_back(b2);
  ip_address_.push_back(b3);
  ip_address_.push_back(b4);
  ip_address_.push_back(b5);
  ip_address_.push_back(b6);
  ip_address_.push_back(b7);
  ip_address_.push_back(b8);
  ip_address_.push_back(b9);
  ip_address_.push_back(b10);
  ip_address_.push_back(b11);
  ip_address_.push_back(b12);
  ip_address_.push_back(b13);
  ip_address_.push_back(b14);
  ip_address_.push_back(b15);
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

bool IPAddress::AssignFromIPLiteral(base::StringPiece ip_literal) {
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
  // TODO(https://crbug.com/1414007): Remove crash key and use DCHECK() when
  // the cause is identified.
  if (!address.IsIPv4()) {
    static base::debug::CrashKeyString* crash_key =
        base::debug::AllocateCrashKeyString("ipaddress",
                                            base::debug::CrashKeySize::Size64);
    base::debug::ScopedCrashKeyString addr(crash_key, address.ToString());
    bool is_valid = address.IsValid();
    base::debug::Alias(&is_valid);
    LOG(FATAL) << "expected an IPv4 address but got " << address.ToString();
  }
  // IPv4-mapped addresses are formed by:
  // <80 bits of zeros>  + <16 bits of ones> + <32-bit IPv4 address>.
  base::StackVector<uint8_t, 16> bytes;
  bytes->insert(bytes->end(), std::begin(kIPv4MappedPrefix),
                std::end(kIPv4MappedPrefix));
  bytes->insert(bytes->end(), address.bytes().begin(), address.bytes().end());
  return IPAddress(bytes->data(), bytes->size());
}

IPAddress ConvertIPv4MappedIPv6ToIPv4(const IPAddress& address) {
  DCHECK(address.IsIPv4MappedIPv6());

  base::StackVector<uint8_t, 16> bytes;
  bytes->insert(bytes->end(),
                address.bytes().begin() + std::size(kIPv4MappedPrefix),
                address.bytes().end());
  return IPAddress(bytes->data(), bytes->size());
}

bool IPAddressMatchesPrefix(const IPAddress& ip_address,
                            const IPAddress& ip_prefix,
                            size_t prefix_length_in_bits) {
  // Both the input IP address and the prefix IP address should be either IPv4
  // or IPv6.
  DCHECK(ip_address.IsValid());
  DCHECK(ip_prefix.IsValid());

  DCHECK_LE(prefix_length_in_bits, ip_prefix.size() * 8);

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

bool ParseCIDRBlock(base::StringPiece cidr_literal,
                    IPAddress* ip_address,
                    size_t* prefix_length_in_bits) {
  // We expect CIDR notation to match one of these two templates:
  //   <IPv4-literal> "/" <number of bits>
  //   <IPv6-literal> "/" <number of bits>

  std::vector<base::StringPiece> parts = base::SplitStringPiece(
      cidr_literal, "/", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (parts.size() != 2)
    return false;

  // Parse the IP address.
  if (!ip_address->AssignFromIPLiteral(parts[0]))
    return false;

  // Parse the prefix length.
  uint32_t number_of_bits;
  if (!ParseUint32(parts[1], &number_of_bits))
    return false;

  // Make sure the prefix length is in a valid range.
  if (number_of_bits > ip_address->size() * 8)
    return false;

  *prefix_length_in_bits = number_of_bits;
  return true;
}

bool ParseURLHostnameToAddress(base::StringPiece hostname,
                               IPAddress* ip_address) {
  if (hostname.size() >= 2 && hostname.front() == '[' &&
      hostname.back() == ']') {
    // Strip the square brackets that surround IPv6 literals.
    auto ip_literal =
        base::StringPiece(hostname).substr(1, hostname.size() - 2);
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
    NOTREACHED();
  }
  return a1.size() * CHAR_BIT;
}

size_t MaskPrefixLength(const IPAddress& mask) {
  base::StackVector<uint8_t, 16> all_ones;
  all_ones->resize(mask.size(), 0xFF);
  return CommonPrefixLength(mask,
                            IPAddress(all_ones->data(), all_ones->size()));
}

Dns64PrefixLength ExtractPref64FromIpv4onlyArpaAAAA(const IPAddress& address) {
  DCHECK(address.IsIPv6());
  IPAddress ipv4onlyarpa0(192, 0, 0, 170);
  IPAddress ipv4onlyarpa1(192, 0, 0, 171);
  if (std::equal(ipv4onlyarpa0.bytes().begin(), ipv4onlyarpa0.bytes().end(),
                 address.bytes().begin() + 12u) ||
      std::equal(ipv4onlyarpa1.bytes().begin(), ipv4onlyarpa1.bytes().end(),
                 address.bytes().begin() + 12u)) {
    return Dns64PrefixLength::k96bit;
  } else if (std::equal(ipv4onlyarpa0.bytes().begin(),
                        ipv4onlyarpa0.bytes().end(),
                        address.bytes().begin() + 9u) ||
             std::equal(ipv4onlyarpa1.bytes().begin(),
                        ipv4onlyarpa1.bytes().end(),
                        address.bytes().begin() + 9u)) {
    return Dns64PrefixLength::k64bit;
  } else if ((std::equal(ipv4onlyarpa0.bytes().begin(),
                         ipv4onlyarpa0.bytes().begin() + 1u,
                         address.bytes().begin() + 7u) &&
              std::equal(ipv4onlyarpa0.bytes().begin() + 1u,
                         ipv4onlyarpa0.bytes().end(),
                         address.bytes().begin() + 9u)) ||
             (std::equal(ipv4onlyarpa1.bytes().begin(),
                         ipv4onlyarpa1.bytes().begin() + 1u,
                         address.bytes().begin() + 7u) &&
              std::equal(ipv4onlyarpa1.bytes().begin() + 1u,
                         ipv4onlyarpa1.bytes().end(),
                         address.bytes().begin() + 9u))) {
    return Dns64PrefixLength::k56bit;
  } else if ((std::equal(ipv4onlyarpa0.bytes().begin(),
                         ipv4onlyarpa0.bytes().begin() + 2u,
                         address.bytes().begin() + 6u) &&
              std::equal(ipv4onlyarpa0.bytes().begin() + 2u,
                         ipv4onlyarpa0.bytes().end(),
                         address.bytes().begin() + 9u)) ||
             ((std::equal(ipv4onlyarpa1.bytes().begin(),
                          ipv4onlyarpa1.bytes().begin() + 2u,
                          address.bytes().begin() + 6u) &&
               std::equal(ipv4onlyarpa1.bytes().begin() + 2u,
                          ipv4onlyarpa1.bytes().end(),
                          address.bytes().begin() + 9u)))) {
    return Dns64PrefixLength::k48bit;
  } else if ((std::equal(ipv4onlyarpa0.bytes().begin(),
                         ipv4onlyarpa0.bytes().begin() + 3u,
                         address.bytes().begin() + 5u) &&
              std::equal(ipv4onlyarpa0.bytes().begin() + 3u,
                         ipv4onlyarpa0.bytes().end(),
                         address.bytes().begin() + 9u)) ||
             (std::equal(ipv4onlyarpa1.bytes().begin(),
                         ipv4onlyarpa1.bytes().begin() + 3u,
                         address.bytes().begin() + 5u) &&
              std::equal(ipv4onlyarpa1.bytes().begin() + 3u,
                         ipv4onlyarpa1.bytes().end(),
                         address.bytes().begin() + 9u))) {
    return Dns64PrefixLength::k40bit;
  } else if (std::equal(ipv4onlyarpa0.bytes().begin(),
                        ipv4onlyarpa0.bytes().end(),
                        address.bytes().begin() + 4u) ||
             std::equal(ipv4onlyarpa1.bytes().begin(),
                        ipv4onlyarpa1.bytes().end(),
                        address.bytes().begin() + 4u)) {
    return Dns64PrefixLength::k32bit;
  } else {
    // if ipv4onlyarpa address is not found return 0
    return Dns64PrefixLength::kInvalid;
  }
}

IPAddress ConvertIPv4ToIPv4EmbeddedIPv6(const IPAddress& ipv4_address,
                                        const IPAddress& ipv6_address,
                                        Dns64PrefixLength prefix_length) {
  DCHECK(ipv4_address.IsIPv4());
  DCHECK(ipv6_address.IsIPv6());

  base::StackVector<uint8_t, 16> bytes;

  uint8_t zero_bits[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  switch (prefix_length) {
    case Dns64PrefixLength::k96bit:
      bytes->insert(bytes->end(), ipv6_address.bytes().begin(),
                    ipv6_address.bytes().begin() + 12u);
      bytes->insert(bytes->end(), ipv4_address.bytes().begin(),
                    ipv4_address.bytes().end());
      return IPAddress(bytes->data(), bytes->size());
    case Dns64PrefixLength::k64bit:
      bytes->insert(bytes->end(), ipv6_address.bytes().begin(),
                    ipv6_address.bytes().begin() + 8u);
      bytes->insert(bytes->end(), std::begin(zero_bits),
                    std::begin(zero_bits) + 1u);
      bytes->insert(bytes->end(), ipv4_address.bytes().begin(),
                    ipv4_address.bytes().end());
      bytes->insert(bytes->end(), std::begin(zero_bits),
                    std::begin(zero_bits) + 3u);
      return IPAddress(bytes->data(), bytes->size());
    case Dns64PrefixLength::k56bit:
      bytes->insert(bytes->end(), ipv6_address.bytes().begin(),
                    ipv6_address.bytes().begin() + 7u);
      bytes->insert(bytes->end(), ipv4_address.bytes().begin(),
                    ipv4_address.bytes().begin() + 1u);
      bytes->insert(bytes->end(), std::begin(zero_bits),
                    std::begin(zero_bits) + 1u);
      bytes->insert(bytes->end(), ipv4_address.bytes().begin() + 1u,
                    ipv4_address.bytes().end());
      bytes->insert(bytes->end(), std::begin(zero_bits),
                    std::begin(zero_bits) + 4u);
      return IPAddress(bytes->data(), bytes->size());
    case Dns64PrefixLength::k48bit:
      bytes->insert(bytes->end(), ipv6_address.bytes().begin(),
                    ipv6_address.bytes().begin() + 6u);
      bytes->insert(bytes->end(), ipv4_address.bytes().begin(),
                    ipv4_address.bytes().begin() + 2u);
      bytes->insert(bytes->end(), std::begin(zero_bits),
                    std::begin(zero_bits) + 1u);
      bytes->insert(bytes->end(), ipv4_address.bytes().begin() + 2u,
                    ipv4_address.bytes().end());
      bytes->insert(bytes->end(), std::begin(zero_bits),
                    std::begin(zero_bits) + 5u);
      return IPAddress(bytes->data(), bytes->size());
    case Dns64PrefixLength::k40bit:
      bytes->insert(bytes->end(), ipv6_address.bytes().begin(),
                    ipv6_address.bytes().begin() + 5u);
      bytes->insert(bytes->end(), ipv4_address.bytes().begin(),
                    ipv4_address.bytes().begin() + 3u);
      bytes->insert(bytes->end(), std::begin(zero_bits),
                    std::begin(zero_bits) + 1u);
      bytes->insert(bytes->end(), ipv4_address.bytes().begin() + 3u,
                    ipv4_address.bytes().end());
      bytes->insert(bytes->end(), std::begin(zero_bits),
                    std::begin(zero_bits) + 6u);
      return IPAddress(bytes->data(), bytes->size());
    case Dns64PrefixLength::k32bit:
      bytes->insert(bytes->end(), ipv6_address.bytes().begin(),
                    ipv6_address.bytes().begin() + 4u);
      bytes->insert(bytes->end(), ipv4_address.bytes().begin(),
                    ipv4_address.bytes().end());
      bytes->insert(bytes->end(), std::begin(zero_bits),
                    std::begin(zero_bits) + 8u);
      return IPAddress(bytes->data(), bytes->size());
    case Dns64PrefixLength::kInvalid:
      return ipv4_address;
  }
}

}  // namespace net
