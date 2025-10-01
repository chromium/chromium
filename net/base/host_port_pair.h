// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_HOST_PORT_PAIR_H_
#define NET_BASE_HOST_PORT_PAIR_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <string_view>
#include <tuple>

#include "base/values.h"
#include "net/base/net_export.h"

class GURL;

namespace url {
class SchemeHostPort;
}  // namespace url

namespace net {

class IPEndPoint;

class NET_EXPORT HostPortPair {
 public:
  HostPortPair();
  // If |in_host| represents an IPv6 address, it should not bracket the address.
  HostPortPair(std::string_view in_host, uint16_t in_port);
  HostPortPair(const char* in_host, uint16_t in_port);
  HostPortPair(std::string&& in_host, uint16_t in_port);

  // Creates a HostPortPair for the origin of |url|.
  static HostPortPair FromURL(const GURL& url);

  static HostPortPair FromSchemeHostPort(
      const url::SchemeHostPort& scheme_host_port);

  // Creates a HostPortPair from an IPEndPoint.
  static HostPortPair FromIPEndPoint(const IPEndPoint& ipe);

  // Creates a HostPortPair from a string formatted in same manner as
  // ToString().
  static HostPortPair FromString(std::string_view str);

  // Nullopt if `value` is malformed to be deserialized to HostPortPair.
  static std::optional<HostPortPair> FromValue(const base::Value& value);

  friend bool operator==(const HostPortPair&, const HostPortPair&) = default;
  friend auto operator<=>(const HostPortPair&, const HostPortPair&) = default;

  bool IsEmpty() const {
    return host_.empty() && port_ == 0;
  }

  const std::string& host() const {
    return host_;
  }

  uint16_t port() const { return port_; }

  void set_host(std::string_view in_host) { host_ = in_host; }

  void set_port(uint16_t in_port) { port_ = in_port; }

  // ToString() will convert the HostPortPair to "host:port".  If |host_| is an
  // IPv6 literal, it will add brackets around |host_|.
  std::string ToString() const;

  // Returns |host_|, adding IPv6 brackets if needed.
  std::string HostForURL() const;

  base::Value ToValue() const;

 private:
  uint16_t port_;
  // If |host_| represents an IPv6 address, this string will not contain
  // brackets around the address.
  std::string host_;
};

}  // namespace net

#endif  // NET_BASE_HOST_PORT_PAIR_H_
