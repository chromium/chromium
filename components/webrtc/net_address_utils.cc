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
  base::Value::Dict value;
  value.Set("ip", candidate.address().ipaddr().ToString());
  value.Set("port", candidate.address().port());
  value.Set("type", candidate.type());
  value.Set("protocol", candidate.protocol());
  value.Set("username", candidate.username());
  value.Set("password", candidate.password());
  value.Set("preference", candidate.preference());
  value.Set("generation", static_cast<int>(candidate.generation()));

  std::string result;
  base::JSONWriter::Write(value, &result);
  return result;
}

bool DeserializeP2PCandidate(const std::string& candidate_str,
                             cricket::Candidate* candidate) {
  absl::optional<base::Value> value(
      base::JSONReader::Read(candidate_str, base::JSON_ALLOW_TRAILING_COMMAS));
  if (!value || !value->is_dict()) {
    return false;
  }

  base::Value::Dict& dic_value = value->GetDict();

  std::string* ip = dic_value.FindString("ip");
  absl::optional<int> port = dic_value.FindInt("port");
  std::string* type = dic_value.FindString("type");
  std::string* protocol = dic_value.FindString("protocol");
  std::string* username = dic_value.FindString("username");
  std::string* password = dic_value.FindString("password");
  absl::optional<double> preference = dic_value.FindDouble("preference");
  absl::optional<int> generation = dic_value.FindInt("generation");

  if (!ip || !port || !type || !protocol || !username || !password ||
      !preference || !generation) {
    return false;
  }

  candidate->set_address(rtc::SocketAddress(std::move(*ip), *port));
  candidate->set_type(std::move(*type));
  candidate->set_protocol(std::move(*protocol));
  candidate->set_username(std::move(*username));
  candidate->set_password(std::move(*password));
  candidate->set_preference(static_cast<float>(*preference));
  candidate->set_generation(*generation);

  return true;
}

}  // namespace webrtc
