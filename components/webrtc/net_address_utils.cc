// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "third_party/webrtc/api/candidate.h"
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

std::string SerializeP2PCandidate(const cricket::Candidate& candidate) {
  // TODO(sergeyu): Use SDP to format candidates?
  base::DictionaryValue value;
  value.SetString("ip", candidate.address().ipaddr().ToString());
  value.SetInteger("port", candidate.address().port());
  value.SetString("type", candidate.type());
  value.SetString("protocol", candidate.protocol());
  value.SetString("username", candidate.username());
  value.SetString("password", candidate.password());
  value.SetDouble("preference", candidate.preference());
  value.SetInteger("generation", candidate.generation());

  std::string result;
  base::JSONWriter::Write(value, &result);
  return result;
}

bool DeserializeP2PCandidate(const std::string& candidate_str,
                             cricket::Candidate* candidate) {
  std::unique_ptr<base::Value> value(base::JSONReader::ReadDeprecated(
      candidate_str, base::JSON_ALLOW_TRAILING_COMMAS));
  if (!value.get() || !value->is_dict()) {
    return false;
  }

  base::DictionaryValue* dic_value =
      static_cast<base::DictionaryValue*>(value.get());

  std::string ip;
  int port = 0;
  std::string type;
  std::string protocol;
  std::string username;
  std::string password;
  absl::optional<double> preference = dic_value->FindDoubleKey("preference");
  int generation = 0;

  if (!dic_value->GetString("ip", &ip) ||
      !dic_value->GetInteger("port", &port) ||
      !dic_value->GetString("type", &type) ||
      !dic_value->GetString("protocol", &protocol) ||
      !dic_value->GetString("username", &username) ||
      !dic_value->GetString("password", &password) || !preference ||
      !dic_value->GetInteger("generation", &generation)) {
    return false;
  }

  candidate->set_address(rtc::SocketAddress(ip, port));
  candidate->set_type(type);
  candidate->set_protocol(protocol);
  candidate->set_username(username);
  candidate->set_password(password);
  candidate->set_preference(static_cast<float>(*preference));
  candidate->set_generation(generation);

  return true;
}

}  // namespace webrtc
