// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_IP_ENDPOINT_H_
#define NET_BASE_IP_ENDPOINT_H_

#include <stdint.h>

#include <optional>
#include <ostream>
#include <string>

#include "base/containers/span.h"
#include "base/values.h"
#include "build/build_config.h"
#include "net/base/address_family.h"
#include "net/base/ip_address.h"
#include "net/base/net_export.h"

// Replicate these from Windows headers to avoid pulling net/sys_addrinfo.h.
// Doing that transitively brings in windows.h. Including windows.h pollutes the
// global namespace with thousands of macro definitions. This file is
// transitively included in enough files that including windows.h potentially
// impacts build performance.
// Similarly, just pull in the minimal header necessary on non-Windows platforms
// to help with build performance.
struct sockaddr;
#if BUILDFLAG(IS_WIN)
typedef int socklen_t;
#else
#include <sys/socket.h>
#endif

namespace net {

// An IPEndPoint represents the address of a transport endpoint:
//  * IP address (either v4 or v6)
//  * Port
//  * IPv6 link local scope ID (only for IPv6 link local address)
class NET_EXPORT IPEndPoint {
 public:
  // Function signatures of if_nametoindex() and if_indextoname().
  using NameToIndexFunc = uint32_t (*)(const char*);
  using IndexToNameFunc = char* (*)(unsigned int, base::span<char>);

  // Set fake if_nametoindex() and if_indextoname() functions for testing.
  static void SetNameToIndexFuncForTesting(NameToIndexFunc func);
  static void SetIndexToNameFuncForTesting(IndexToNameFunc func);

  // Nullopt if `value` is malformed to be serialized to IPEndPoint.
  static std::optional<IPEndPoint> FromValue(const base::Value& value);

  IPEndPoint();
  ~IPEndPoint();
  IPEndPoint(const IPAddress& address,
             uint16_t port,
             std::optional<uint32_t> scope_id = std::nullopt);
  IPEndPoint(const IPEndPoint& endpoint);

  const IPAddress& address() const { return address_; }

  // Returns the IPv4/IPv6 port if it has been set by the constructor or
  // `FromSockAddr`. This function will crash if the IPEndPoint is for a
  // Bluetooth socket.
  uint16_t port() const;

  // Returns the IPv6 scope identifier if it has been set by FromSockAddr() and
  // the address is link-local.
  std::optional<uint32_t> scope_id() const { return scope_id_; }

  // Returns AddressFamily of the address. Returns ADDRESS_FAMILY_UNSPECIFIED if
  // this is the IPEndPoint for a Bluetooth socket.
  AddressFamily GetFamily() const;

  // Returns the sockaddr family of the address, AF_INET or AF_INET6. Returns
  // AF_BTH if this is the IPEndPoint for a Bluetooth socket.
  int GetSockAddrFamily() const;

  // Convert to a provided sockaddr struct. This function will crash if the
  // IPEndPoint is for a Bluetooth socket.
  // |address| is the sockaddr to copy into.  Should be at least
  //    sizeof(struct sockaddr_storage) bytes.
  // |address_length| is an input/output parameter.  On input, it is the
  //    size of data in |address| available.  On output, it is the size of
  //    the address that was copied into |address|.
  // Returns true on success, false on failure.
  [[nodiscard]] bool ToSockAddr(struct sockaddr* address,
                                socklen_t* address_length) const;

  // Convert from a sockaddr struct.
  // |address| is the address.
  // |address_length| is the length of |address|.
  // Returns true on success, false on failure.
  [[nodiscard]] bool FromSockAddr(const struct sockaddr* address,
                                  socklen_t address_length);

  // Returns value as a string (e.g. "127.0.0.1:80"). Returns the empty string
  // when |address_| is invalid (the port will be ignored). This function will
  // crash if the IPEndPoint is for a Bluetooth socket. This function doesn't
  // include IPv6 scope id intentionally because exposing scope id is
  // discouraged for web purpose. See
  // https://datatracker.ietf.org/doc/html/draft-ietf-6man-zone-ui#section-5
  std::string ToString() const;

  // As above, but without port. Returns the empty string when address_ is
  // invalid. The function will crash if the IPEndPoint is for a Bluetooth
  // socket. This function doesn't include IPv6 scope id intentionally.
  std::string ToStringWithoutPort() const;

  bool operator<(const IPEndPoint& that) const;
  friend bool operator==(const IPEndPoint&, const IPEndPoint&) = default;

  base::Value ToValue() const;

 private:
  static NameToIndexFunc name_to_index_func_for_testing_;
  static IndexToNameFunc index_to_name_func_for_testing_;

  // Returns a scope ID from `dict` when `dict` has a valid interface name that
  // can be converted to an interface index.
  static std::optional<uint32_t> ScopeIdFromDict(const base::Value::Dict& dict);

  // Converts `scope_id` to an interface name as a base::Value.
  static base::Value ScopeIdToValue(std::optional<uint32_t> scope_id);

  bool IsIPv6LinkLocal() const;

  IPAddress address_;
  uint16_t port_ = 0;
  std::optional<uint32_t> scope_id_;
};

NET_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& os,
                                            const IPEndPoint& ip_endpoint);

}  // namespace net

#endif  // NET_BASE_IP_ENDPOINT_H_
