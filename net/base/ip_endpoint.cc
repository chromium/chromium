// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/ip_endpoint.h"

#include <string.h>

#include <optional>
#include <ostream>
#include <tuple>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_byteorder.h"
#include "base/values.h"
#include "build/build_config.h"
#include "net/base/ip_address.h"
#include "net/base/sys_addrinfo.h"

#if BUILDFLAG(IS_WIN)
#include <winsock2.h>

#include <ws2bth.h>

#include "net/base/winsock_util.h"  // For kBluetoothAddressSize
#endif

namespace net {

namespace {

// Value dictionary keys
constexpr std::string_view kValueAddressKey = "address";
constexpr std::string_view kValuePortKey = "port";

}  // namespace

// static
std::optional<IPEndPoint> IPEndPoint::FromValue(const base::Value& value) {
  const base::Value::Dict* dict = value.GetIfDict();
  if (!dict)
    return std::nullopt;

  const base::Value* address_value = dict->Find(kValueAddressKey);
  if (!address_value)
    return std::nullopt;
  std::optional<IPAddress> address = IPAddress::FromValue(*address_value);
  if (!address.has_value())
    return std::nullopt;
  // Expect IPAddress to only allow deserializing valid addresses.
  DCHECK(address.value().IsValid());

  std::optional<int> port = dict->FindInt(kValuePortKey);
  if (!port.has_value() ||
      !base::IsValueInRangeForNumericType<uint16_t>(port.value())) {
    return std::nullopt;
  }

  return IPEndPoint(address.value(),
                    base::checked_cast<uint16_t>(port.value()));
}

IPEndPoint::IPEndPoint() = default;

IPEndPoint::~IPEndPoint() = default;

IPEndPoint::IPEndPoint(const IPAddress& address, uint16_t port)
    : address_(address), port_(port) {}

IPEndPoint::IPEndPoint(const IPEndPoint& endpoint) = default;

uint16_t IPEndPoint::port() const {
#if BUILDFLAG(IS_WIN)
  DCHECK_NE(address_.size(), kBluetoothAddressSize);
#endif
  return port_;
}

AddressFamily IPEndPoint::GetFamily() const {
  return GetAddressFamily(address_);
}

int IPEndPoint::GetSockAddrFamily() const {
  switch (address_.size()) {
    case IPAddress::kIPv4AddressSize:
      return AF_INET;
    case IPAddress::kIPv6AddressSize:
      return AF_INET6;
#if BUILDFLAG(IS_WIN)
    case kBluetoothAddressSize:
      return AF_BTH;
#endif
    default:
      NOTREACHED_IN_MIGRATION() << "Bad IP address";
      return AF_UNSPEC;
  }
}

bool IPEndPoint::ToSockAddr(struct sockaddr* address,
                            socklen_t* address_length) const {
  // By definition, socklen_t is large enough to hold both sizes.
  constexpr socklen_t kSockaddrInSize =
      static_cast<socklen_t>(sizeof(struct sockaddr_in));
  constexpr socklen_t kSockaddrIn6Size =
      static_cast<socklen_t>(sizeof(struct sockaddr_in6));

  DCHECK(address);
  DCHECK(address_length);
#if BUILDFLAG(IS_WIN)
  DCHECK_NE(address_.size(), kBluetoothAddressSize);
#endif
  switch (address_.size()) {
    case IPAddress::kIPv4AddressSize: {
      if (*address_length < kSockaddrInSize)
        return false;
      *address_length = kSockaddrInSize;
      struct sockaddr_in* addr = reinterpret_cast<struct sockaddr_in*>(address);
      memset(addr, 0, sizeof(struct sockaddr_in));
      addr->sin_family = AF_INET;
      addr->sin_port = base::HostToNet16(port_);
      memcpy(&addr->sin_addr, address_.bytes().data(),
             IPAddress::kIPv4AddressSize);
      break;
    }
    case IPAddress::kIPv6AddressSize: {
      if (*address_length < kSockaddrIn6Size)
        return false;
      *address_length = kSockaddrIn6Size;
      struct sockaddr_in6* addr6 =
          reinterpret_cast<struct sockaddr_in6*>(address);
      memset(addr6, 0, sizeof(struct sockaddr_in6));
      addr6->sin6_family = AF_INET6;
      addr6->sin6_port = base::HostToNet16(port_);
      memcpy(&addr6->sin6_addr, address_.bytes().data(),
             IPAddress::kIPv6AddressSize);
      break;
    }
    default:
      return false;
  }
  return true;
}

bool IPEndPoint::FromSockAddr(const struct sockaddr* sock_addr,
                              socklen_t sock_addr_len) {
  DCHECK(sock_addr);
  switch (sock_addr->sa_family) {
    case AF_INET: {
      if (sock_addr_len < static_cast<socklen_t>(sizeof(struct sockaddr_in)))
        return false;
      const struct sockaddr_in* addr =
          reinterpret_cast<const struct sockaddr_in*>(sock_addr);
      *this = IPEndPoint(
          // `s_addr` is a `uint32_t`, but it is already in network byte order.
          IPAddress(base::as_bytes(base::span_from_ref(addr->sin_addr.s_addr))),
          base::NetToHost16(addr->sin_port));
      return true;
    }
    case AF_INET6: {
      if (sock_addr_len < static_cast<socklen_t>(sizeof(struct sockaddr_in6)))
        return false;
      const struct sockaddr_in6* addr =
          reinterpret_cast<const struct sockaddr_in6*>(sock_addr);
      *this = IPEndPoint(IPAddress(addr->sin6_addr.s6_addr),
                         base::NetToHost16(addr->sin6_port));
      return true;
    }
#if BUILDFLAG(IS_WIN)
    case AF_BTH: {
      if (sock_addr_len < static_cast<socklen_t>(sizeof(SOCKADDR_BTH)))
        return false;
      const SOCKADDR_BTH* addr =
          reinterpret_cast<const SOCKADDR_BTH*>(sock_addr);
      *this = IPEndPoint();
      // A bluetooth address is 6 bytes, but btAddr is a ULONGLONG, so we take a
      // prefix of it.
      address_ = IPAddress(base::as_bytes(base::span_from_ref(addr->btAddr))
                               .first(kBluetoothAddressSize));
      // Intentionally ignoring Bluetooth port. It is a ULONG, but
      // `IPEndPoint::port_` is a uint16_t. See https://crbug.com/1231273.
      return true;
    }
#endif
  }
  return false;  // Unrecognized |sa_family|.
}

std::string IPEndPoint::ToString() const {
#if BUILDFLAG(IS_WIN)
  DCHECK_NE(address_.size(), kBluetoothAddressSize);
#endif
  return IPAddressToStringWithPort(address_, port_);
}

std::string IPEndPoint::ToStringWithoutPort() const {
#if BUILDFLAG(IS_WIN)
  DCHECK_NE(address_.size(), kBluetoothAddressSize);
#endif
  return address_.ToString();
}

bool IPEndPoint::operator<(const IPEndPoint& other) const {
  // Sort IPv4 before IPv6.
  if (address_.size() != other.address_.size()) {
    return address_.size() < other.address_.size();
  }
  return std::tie(address_, port_) < std::tie(other.address_, other.port_);
}

bool IPEndPoint::operator==(const IPEndPoint& other) const {
  return address_ == other.address_ && port_ == other.port_;
}

bool IPEndPoint::operator!=(const IPEndPoint& that) const {
  return !(*this == that);
}

base::Value IPEndPoint::ToValue() const {
  base::Value::Dict dict;

  DCHECK(address_.IsValid());
  dict.Set(kValueAddressKey, address_.ToValue());
  dict.Set(kValuePortKey, port_);

  return base::Value(std::move(dict));
}

std::ostream& operator<<(std::ostream& os, const IPEndPoint& ip_endpoint) {
  return os << ip_endpoint.ToString();
}

}  // namespace net
