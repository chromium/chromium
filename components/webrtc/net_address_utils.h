// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBRTC_NET_ADDRESS_UTILS_H_
#define COMPONENTS_WEBRTC_NET_ADDRESS_UTILS_H_

#include "third_party/webrtc/rtc_base/ip_address.h"
#include "third_party/webrtc/rtc_base/socket_address.h"

namespace net {
class IPAddress;
class IPEndPoint;
}  // namespace net

namespace webrtc {

// Chromium and libjingle represent socket addresses differently. The
// following two functions are used to convert addresses from one
// representation to another.
bool IPEndPointToSocketAddress(const net::IPEndPoint& ip_endpoint,
                               webrtc::SocketAddress* address);
bool SocketAddressToIPEndPoint(const webrtc::SocketAddress& address,
                               net::IPEndPoint* ip_endpoint);

webrtc::IPAddress NetIPAddressToRtcIPAddress(const net::IPAddress& ip_address);

net::IPAddress RtcIPAddressToNetIPAddress(const webrtc::IPAddress& ip_address);

}  // namespace webrtc

#endif  // COMPONENTS_WEBRTC_NET_ADDRESS_UTILS_H_
