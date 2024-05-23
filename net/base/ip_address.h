// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

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
#include "base/containers/span.h"
#include "base/values.h"
#include "net/base/net_export.h"

namespace net {

// Helper class to represent the sequence of bytes in an IP address.
// A vector<uint8_t> would be simpler but incurs heap allocation, so
// IPAddressBytes uses a fixed size array.
class NET_EXPORT IPAddressBytes {
 public:
  IPAddressBytes();
  explicit IPAddressBytes(base::span<const uint8_t> data);
  IPAddressBytes(const IPAddressBytes& other);
  ~IPAddressBytes();

  // Copies elements from |data| into this object.
  void Assign(base::span<const uint8_t> data);

  // Returns the number of elements in the underlying array.
  size_t size() const { return size_; }

  // Sets the size to be |size|. Does not actually change the size
  // of the underlying array or zero-initialize the bytes.
  void Resize(size_t size) {
    DCHECK_LE(size, 16u);
    size_ = static_cast<uint8_t>(size);
  }

  // Returns true if the underlying array is empty.
  bool empty() const { return size_ == 0; }

  // Returns a pointer to the underlying array of bytes.
  const uint8_t* data() const { return bytes_.data(); }
  uint8_t* data() { return bytes_.data(); }

  // Returns a pointer to the first element.
  const uint8_t* begin() const { return data(); }
  uint8_t* begin() { return data(); }

  // Returns a pointer past the last element.
  const uint8_t* end() const { return data() + size_; }
  uint8_t* end() { return data() + size_; }

  // Returns a reference to the last element.
  uint8_t& back() {
    DCHECK(!empty());
    return bytes_[size_ - 1];
  }
  const uint8_t& back() const {
    DCHECK(!empty());
    return bytes_[size_ - 1];
  }

  // Appends |val| to the end and increments the size.
  void push_back(uint8_t val) {
    DCHECK_GT(16, size_);
    bytes_[size_++] = val;
  }

  // Appends `data` to the end and increments the size.
  void Append(base::span<const uint8_t> data);

  // Returns a reference to the byte at index |pos|.
  uint8_t& operator[](size_t pos) {
    DCHECK_LT(pos, size_);
    return bytes_[pos];
  }
  const uint8_t& operator[](size_t pos) const {
    DCHECK_LT(pos, size_);
    return bytes_[pos];
  }

  bool operator<(const IPAddressBytes& other) const;
  bool operator!=(const IPAddressBytes& other) const;
  bool operator==(const IPAddressBytes& other) const;

  size_t EstimateMemoryUsage() const;

 private:
  // Underlying sequence of bytes
  std::array<uint8_t, 16> bytes_;

  // Number of elements in |bytes_|. Should be either kIPv4AddressSize
  // or kIPv6AddressSize or 0.
  uint8_t size_;
};

class NET_EXPORT IPAddress {
 public:
  enum : size_t { kIPv4AddressSize = 4, kIPv6AddressSize = 16 };

  // Nullopt if `value` is malformed to be deserialized to IPAddress.
  static std::optional<IPAddress> FromValue(const base::Value& value);

  // Parses an IP address literal (either IPv4 or IPv6). Returns the resulting
  // IPAddress on success, or nullopt on error.
  static std::optional<IPAddress> FromIPLiteral(std::string_view ip_literal);

  // Creates a zero-sized, invalid address.
  IPAddress();

  IPAddress(const IPAddress& other);

  // Copies the input address to |ip_address_|.
  explicit IPAddress(const IPAddressBytes& address);

  // Copies the input address to |ip_address_|. The input is expected to be in
  // network byte order.
  explicit IPAddress(base::span<const uint8_t> address);

  // Initializes |ip_address_| from the 4 bX bytes to form an IPv4 address.
  // The bytes are expected to be in network byte order.
  IPAddress(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3);

  // Initializes |ip_address_| from the 16 bX bytes to form an IPv6 address.
  // The bytes are expected to be in network byte order.
  IPAddress(uint8_t b0,
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
            uint8_t b15);

  ~IPAddress();

  // Returns true if the IP has |kIPv4AddressSize| elements.
  bool IsIPv4() const;

  // Returns true if the IP has |kIPv6AddressSize| elements.
  bool IsIPv6() const;

  // Returns true if the IP is either an IPv4 or IPv6 address. This function
  // only checks the address length.
  bool IsValid() const;

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
  size_t size() const { return ip_address_.size(); }

  // Returns true if the IP is an empty, zero-sized (invalid) address.
  bool empty() const { return ip_address_.empty(); }

  // Returns the canonical string representation of an IP address.
  // For example: "192.168.0.1" or "::1". Returns the empty string when
  // |ip_address_| is invalid.
  std::string ToString() const;

  // Parses an IP address literal (either IPv4 or IPv6) to its numeric value.
  // Returns true on success and fills |ip_address_| with the numeric value.
  //
  // When parsing fails, the original value of |this| will be overwritten such
  // that |this->empty()| and |!this->IsValid()|.
  [[nodiscard]] bool AssignFromIPLiteral(std::string_view ip_literal);

  // Returns the underlying bytes.
  const IPAddressBytes& bytes() const { return ip_address_; }

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

// Returns the address as a sequence of bytes in network-byte-order.
NET_EXPORT std::string IPAddressToPackedString(const IPAddress& address);

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
bool IPAddressStartsWith(const IPAddress& address, const uint8_t (&prefix)[N]) {
  if (address.size() < N)
    return false;
  return std::equal(prefix, prefix + N, address.bytes().begin());
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
