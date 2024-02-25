// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webrtc/net_address_utils.h"

#include <stdint.h>

#include <memory>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "third_party/webrtc/rtc_base/byte_order.h"
#include "third_party/webrtc/rtc_base/socket_address.h"

namespace webrtc {

bool IPEndPointToSocketAddress(const net::IPEndPoint& ip_endpoint,
                               rtc::SocketAddress* address) {
  sockaddr_storage addr;
  socklen_t len = sizeof(addr);
  return ip_endpoint.ToSockAddr(reinterpret_cast<sockaddr*>(&addr), &len) &&
         rtc::SocketAddressFromSockAddrStorage(addr, address);
}

bool SocketAddressToIPEndPoint(const rtc::SocketAddress& address,
                               net::IPEndPoint* ip_endpoint) {
  sockaddr_storage addr;
  int size = address.ToSockAddrStorage(&addr);
  return (size > 0) &&
         ip_endpoint->FromSockAddr(reinterpret_cast<sockaddr*>(&addr), size);
}

rtc::IPAddress NetIPAddressToRtcIPAddress(const net::IPAddress& ip_address) {
  if (ip_address.IsIPv4()) {
    uint32_t address;
    memcpy(&address, ip_address.bytes().data(), sizeof(uint32_t));
    address = rtc::NetworkToHost32(address);
    return rtc::IPAddress(address);
  }
  if (ip_address.IsIPv6()) {
    in6_addr address;
    memcpy(&address, ip_address.bytes().data(), sizeof(in6_addr));
    return rtc::IPAddress(address);
  }
  return rtc::IPAddress();
}

net::IPAddress RtcIPAddressToNetIPAddress(const rtc::IPAddress& ip_address) {
  rtc::SocketAddress socket_address(ip_address, 0);
  net::IPEndPoint ip_endpoint;
  webrtc::SocketAddressToIPEndPoint(socket_address, &ip_endpoint);
  return ip_endpoint.address();
}

}  // namespace webrtc
