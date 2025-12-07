// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_IP_ADDRESS_H_
#define NET_BASE_IP_ADDRESS_H_

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/values.h"
#include "net/base/net_export.h"
#include "url/url_canon_ip.h"

namespace net {

// Helper class to represent the sequence of bytes in an IP address.
// A vector<uint8_t> would be simpler but incurs heap allocation, so
// IPAddressBytes uses a fixed size std::array.
class NET_EXPORT IPAddressBytes {
 public:
  // Public solely for iterator types.
  using IPAddressStorage = std::array<uint8_t, 16>;

  using iterator = IPAddressStorage::iterator;
  using const_iterator = IPAddressStorage::const_iterator;

  constexpr IPAddressBytes() : bytes_{}, size_(0) {}
  constexpr explicit IPAddressBytes(base::span<const uint8_t> data) {
    Assign(data);
  }
  constexpr IPAddressBytes(const IPAddressBytes& other) = default;
  constexpr ~IPAddressBytes() = default;

  // Copies elements from |data| into this object.
  constexpr void Assign(base::span<const uint8_t> data) {
    CHECK_GE(16u, data.size());
    size_ = static_cast<uint8_t>(data.size());
    base::span(*this).copy_from(data);
  }

  // Returns the number of elements in the underlying array.
  constexpr size_t size() const { return size_; }

  // Sets the size to be |size|. Does not actually change the size
  // of the underlying array or zero-initialize the bytes.
  constexpr void Resize(size_t size) {
    DCHECK_LE(size, 16u);
    size_ = static_cast<uint8_t>(size);
  }

  // Returns true if the underlying array is empty.
  constexpr bool empty() const { return size_ == 0; }

  // Returns a pointer to the underlying array of bytes.
  constexpr const uint8_t* data() const { return bytes_.data(); }
  constexpr uint8_t* data() { return bytes_.data(); }

  // Returns an iterator to the first element.
  constexpr const_iterator begin() const { return bytes_.begin(); }
  constexpr iterator begin() { return bytes_.begin(); }

  // Returns an iterator past the last element.
  constexpr const_iterator end() const { return bytes_.begin() + size_; }
  constexpr iterator end() { return bytes_.begin() + size_; }

  // Returns the address as a span.
  constexpr base::span<const uint8_t> span() const {
    return base::span(bytes_).first(size_);
  }
  constexpr base::span<uint8_t> span() {
    return base::span(bytes_).first(size_);
  }

  // Returns a reference to the last element.
  constexpr uint8_t& back() {
    DCHECK(!empty());
    return bytes_[size_ - 1];
  }
  constexpr const uint8_t& back() const {
    DCHECK(!empty());
    return bytes_[size_ - 1];
  }

  // Appends |val| to the end and increments the size.
  constexpr void push_back(uint8_t val) {
    DCHECK_GT(16, size_);
    bytes_[size_++] = val;
  }

  // Appends `data` to the end and increments the size.
  void Append(base::span<const uint8_t> data);

  // Returns a reference to the byte at index |pos|.
  constexpr uint8_t& operator[](size_t pos) {
    DCHECK_LT(pos, size_);
    return bytes_[pos];
  }
  constexpr const uint8_t& operator[](size_t pos) const {
    DCHECK_LT(pos, size_);
    return bytes_[pos];
  }

  bool operator<(const IPAddressBytes& other) const;
  bool operator==(const IPAddressBytes& other) const;

  size_t EstimateMemoryUsage() const;

 private:
  // Underlying sequence of bytes.
  IPAddressStorage bytes_;

  // Number of elements in |bytes_|. Should be either kIPv4AddressSize
  // or kIPv6AddressSize or 0.
  uint8_t size_;
};

namespace internal {

constexpr bool ParseIPLiteralToBytes(std::string_view ip_literal,
                                     IPAddressBytes* bytes) {
  // |ip_literal| could be either an IPv4 or an IPv6 literal. If it contains
  // a colon however, it must be an IPv6 address.
  if (ip_literal.find(':') != std::string_view::npos) {
    // GURL expects IPv6 hostnames to be surrounded with brackets.
    // Not using base::StrCat() because it is not constexpr.
    std::string host_with_brackets;
    host_with_brackets.reserve(ip_literal.size() + 2);
    host_with_brackets.push_back('[');
    host_with_brackets.append(ip_literal);
    host_with_brackets.push_back(']');

    // Try parsing the hostname as an IPv6 literal.
    bytes->Resize(16);  // 128 bits.
    return url::IPv6AddressToNumber(host_with_brackets, bytes->span());
  }

  // Otherwise the string is an IPv4 address.
  bytes->Resize(4);  // 32 bits.
  int num_components;
  url::CanonHostInfo::Family family =
      url::IPv4AddressToNumber(ip_literal, bytes->span(), &num_components);
  return family == url::CanonHostInfo::IPV4;
}

}  // namespace internal

// Represent an IP address. Has built-in support for IPv4 and IPv6 addresses,
// though may also be used for Bluetooth addresses.
//
// See ip_address_util.h for helpers to convert an IPAddress to an in_addr or
// in6_addr.
class NET_EXPORT IPAddress {
 public:
  enum : size_t { kIPv4AddressSize = 4, kIPv6AddressSize = 16 };

  // Nullopt if `value` is malformed to be deserialized to IPAddress.
  static std::optional<IPAddress> FromValue(const base::Value& value);

  // Parses an IP address literal (either IPv4 or IPv6). Returns the resulting
  // IPAddress on success, or nullopt on error.
  static std::optional<IPAddress> FromIPLiteral(std::string_view ip_literal);

  // Creates a zero-sized, invalid address.
  constexpr IPAddress() = default;

  constexpr IPAddress(const IPAddress& other) = default;

  // Copies the input address to |ip_address_|.
  constexpr explicit IPAddress(const IPAddressBytes& address)
      : ip_address_(address) {}

  // Copies the input address to |ip_address_|. The input is expected to be in
  // network byte order.
  constexpr explicit IPAddress(base::span<const uint8_t> address)
      : ip_address_(address) {}

  // Initializes |ip_address_| from the 4 bX bytes to form an IPv4 address.
  // The bytes are expected to be in network byte order.
  constexpr IPAddress(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3) {
    ip_address_.Assign({b0, b1, b2, b3});
  }

  // Initializes |ip_address_| from the 16 bX bytes to form an IPv6 address.
  // The bytes are expected to be in network byte order.
  constexpr IPAddress(uint8_t b0,
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

  constexpr ~IPAddress() = default;

  // Returns true if the IP has |kIPv4AddressSize| elements.
  constexpr bool IsIPv4() const {
    return ip_address_.size() == kIPv4AddressSize;
  }

  // Returns true if the IP has |kIPv6AddressSize| elements.
  constexpr bool IsIPv6() const {
    return ip_address_.size() == kIPv6AddressSize;
  }

  // Returns true if the IP is either an IPv4 or IPv6 address. This function
  // only checks the address length.
  constexpr bool IsValid() const { return IsIPv4() || IsIPv6(); }

  // Returns true if the IP is not in a range reserved by the IANA for
  // local networks. Works with both IPv4 and IPv6 addresses.
  // IPv4-mapped-to-IPv6 addresses are considered publicly routable.
  bool IsPubliclyRoutable() const;

  // Returns true if the IP is "zero" (e.g. the 0.0.0.0 IPv4 address).
  bool IsZero() const;

  // Returns true if |ip_address_| is an IPv4-mapped IPv6 address.
  bool IsIPv4MappedIPv6() const;

  // Returns true if |ip_address_| is 127.0.0.1/8 or ::1/128
  bool IsLoopback() const;

  // Returns true if |ip_address_| is 169.254.0.0/16 or fe80::/10, or
  // ::ffff:169.254.0.0/112 (IPv4 mapped IPv6 link-local).
  bool IsLinkLocal() const;

  // Returns true if `ip_address_` is a unique local IPv6 address (fc00::/7).
  bool IsUniqueLocalIPv6() const;

  // The size in bytes of |ip_address_|.
  constexpr size_t size() const { return ip_address_.size(); }

  // Returns true if the IP is an empty, zero-sized (invalid) address.
  constexpr bool empty() const { return ip_address_.empty(); }

  // Returns the canonical string representation of an IP address.
  // For example: "192.168.0.1" or "::1". Returns the empty string when
  // |ip_address_| is invalid.
  std::string ToString() const;

  // Parses an IP address literal (either IPv4 or IPv6) to its numeric value.
  // Returns true on success and fills |ip_address_| with the numeric value.
  //
  // When parsing fails, the original value of |this| will be overwritten such
  // that |this->empty()| and |!this->IsValid()|.
  [[nodiscard]] constexpr bool AssignFromIPLiteral(
      std::string_view ip_literal) {
    bool success = internal::ParseIPLiteralToBytes(ip_literal, &ip_address_);
    if (!success) {
      ip_address_.Resize(0);
    }
    return success;
  }

  // Returns the underlying bytes.
  constexpr const IPAddressBytes& bytes() const { return ip_address_; }

  // Copies the bytes to a new vector. Generally callers should be using
  // |bytes()| and the IPAddressBytes abstraction. This method is provided as a
  // convenience for call sites that existed prior to the introduction of
  // IPAddressBytes.
  std::vector<uint8_t> CopyBytesToVector() const;

  // Returns an IPAddress instance representing the 127.0.0.1 address.
  static IPAddress IPv4Localhost();

  // Returns an IPAddress instance representing the ::1 address.
  static IPAddress IPv6Localhost();

  // Returns an IPAddress made up of |num_zero_bytes| zeros.
  static IPAddress AllZeros(size_t num_zero_bytes);

  // Returns an IPAddress instance representing the 0.0.0.0 address.
  static IPAddress IPv4AllZeros();

  // Returns an IPAddress instance representing the :: address.
  static IPAddress IPv6AllZeros();

  // Create an IPv4 mask with prefix |mask_prefix_length|
  // Returns false if |max_prefix_length| is greater than the maximum length of
  // an IPv4 address.
  static bool CreateIPv4Mask(IPAddress* ip_address, size_t mask_prefix_length);

  // Create an IPv6 mask with prefix |mask_prefix_length|
  // Returns false if |max_prefix_length| is greater than the maximum length of
  // an IPv6 address.
  static bool CreateIPv6Mask(IPAddress* ip_address, size_t mask_prefix_length);

  bool operator==(const IPAddress& that) const;
  bool operator!=(const IPAddress& that) const;
  bool operator<(const IPAddress& that) const;

  // Must be a valid address (per IsValid()).
  base::Value ToValue() const;

  size_t EstimateMemoryUsage() const;

 private:
  IPAddressBytes ip_address_;

  // This class is copyable and assignable.
};

using IPAddressList = std::vector<IPAddress>;

// Returns the canonical string representation of an IP address along with its
// port. For example: "192.168.0.1:99" or "[::1]:80".
NET_EXPORT std::string IPAddressToStringWithPort(const IPAddress& address,
                                                 uint16_t port);

// Converts an IPv4 address to an IPv4-mapped IPv6 address.
// For example 192.168.0.1 would be converted to ::ffff:192.168.0.1.
NET_EXPORT IPAddress ConvertIPv4ToIPv4MappedIPv6(const IPAddress& address);

// Converts an IPv4-mapped IPv6 address to IPv4 address. Should only be called
// on IPv4-mapped IPv6 addresses.
NET_EXPORT IPAddress ConvertIPv4MappedIPv6ToIPv4(const IPAddress& address);

// Compares an IP address to see if it falls within the specified IP block.
// Returns true if it does, false otherwise.
//
// The IP block is given by (|ip_prefix|, |prefix_length_in_bits|) -- any
// IP address whose |prefix_length_in_bits| most significant bits match
// |ip_prefix| will be matched.
//
// In cases when an IPv4 address is being compared to an IPv6 address prefix
// and vice versa, the IPv4 addresses will be converted to IPv4-mapped
// (IPv6) addresses.
NET_EXPORT bool IPAddressMatchesPrefix(const IPAddress& ip_address,
                                       const IPAddress& ip_prefix,
                                       size_t prefix_length_in_bits);

// Parses an IP block specifier from CIDR notation to an
// (IP address, prefix length) pair. Returns true on success and fills
// |*ip_address| with the numeric value of the IP address and sets
// |*prefix_length_in_bits| with the length of the prefix. On failure,
// |ip_address| will be cleared to an empty value.
//
// CIDR notation literals can use either IPv4 or IPv6 literals. Some examples:
//
//    10.10.3.1/20
//    a:b:c::/46
//    ::1/128
NET_EXPORT bool ParseCIDRBlock(std::string_view cidr_literal,
                               IPAddress* ip_address,
                               size_t* prefix_length_in_bits);

// Parses a URL-safe IP literal (see RFC 3986, Sec 3.2.2) to its numeric value.
// Returns true on success, and fills |ip_address| with the numeric value.
// In other words, |hostname| must be an IPv4 literal, or an IPv6 literal
// surrounded by brackets as in [::1]. On failure |ip_address| may have been
// overwritten and could contain an invalid IPAddress.
[[nodiscard]] NET_EXPORT bool ParseURLHostnameToAddress(
    std::string_view hostname,
    IPAddress* ip_address);

// Returns number of matching initial bits between the addresses |a1| and |a2|.
NET_EXPORT size_t CommonPrefixLength(const IPAddress& a1, const IPAddress& a2);

// Computes the number of leading 1-bits in |mask|.
NET_EXPORT size_t MaskPrefixLength(const IPAddress& mask);

// Checks whether |address| starts with |prefix|. This provides similar
// functionality as IPAddressMatchesPrefix() but doesn't perform automatic IPv4
// to IPv4MappedIPv6 conversions and only checks against full bytes.
template <size_t N>
constexpr bool IPAddressStartsWith(const IPAddress& address,
                                   const uint8_t (&prefix)[N]) {
  if (address.size() < N)
    return false;
  // SAFETY: N is size of `prefix` as inferred by the compiler.
  return std::equal(prefix, UNSAFE_BUFFERS(prefix + N),
                    address.bytes().begin());
}

// According to RFC6052 Section 2.2 IPv4-Embedded IPv6 Address Format.
// https://www.rfc-editor.org/rfc/rfc6052#section-2.2
// +--+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
// |PL| 0-------------32--40--48--56--64--72--80--88--96--104---------|
// +--+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
// |32|     prefix    |v4(32)         | u | suffix                    |
// +--+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
// |40|     prefix        |v4(24)     | u |(8)| suffix                |
// +--+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
// |48|     prefix            |v4(16) | u | (16)  | suffix            |
// +--+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
// |56|     prefix                |(8)| u |  v4(24)   | suffix        |
// +--+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
// |64|     prefix                    | u |   v4(32)      | suffix    |
// +--+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
// |96|     prefix                                    |    v4(32)     |
// +--+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
//
// The NAT64/DNS64 translation prefixes has one of the following lengths.
enum class Dns64PrefixLength {
  k32bit,
  k40bit,
  k48bit,
  k56bit,
  k64bit,
  k96bit,
  kInvalid
};

// Extracts the NAT64 translation prefix from the IPv6 address using the well
// known address ipv4only.arpa 192.0.0.170 and 192.0.0.171.
// Returns prefix length on success, or Dns64PrefixLength::kInvalid on failure
// (when the ipv4only.arpa IPv4 address is not found)
NET_EXPORT Dns64PrefixLength
ExtractPref64FromIpv4onlyArpaAAAA(const IPAddress& address);

// Converts an IPv4 address to an IPv4-embedded IPv6 address using the given
// prefix. For example 192.168.0.1 and 64:ff9b::/96 would be converted to
// 64:ff9b::192.168.0.1
// Returns converted IPv6 address when prefix_length is not
// Dns64PrefixLength::kInvalid, and returns the original IPv4 address when
// prefix_length is Dns64PrefixLength::kInvalid.
NET_EXPORT IPAddress
ConvertIPv4ToIPv4EmbeddedIPv6(const IPAddress& ipv4_address,
                              const IPAddress& ipv6_address,
                              Dns64PrefixLength prefix_length);

}  // namespace net

#endif  // NET_BASE_IP_ADDRESS_H_
