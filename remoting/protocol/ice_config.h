// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_ICE_CONFIG_H_
#define REMOTING_PROTOCOL_ICE_CONFIG_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/time/time.h"
#include "base/values.h"
#include "third_party/webrtc/p2p/base/port_allocator.h"
#include "third_party/webrtc/rtc_base/socket_address.h"

namespace remoting {

namespace apis::v1 {
class GetIceConfigResponse;
}  // namespace apis::v1

namespace protocol {

struct IceConfig {
  IceConfig();
  IceConfig(const IceConfig& other);
  ~IceConfig();

  bool is_null() const { return expiration_time.is_null(); }

  // Parses JSON representation of the config. Returns null config if parsing
  // fails.
  static IceConfig Parse(const base::Value::Dict& dictionary);
  static IceConfig Parse(const apis::v1::GetIceConfigResponse& config);

  // Parses a |url| in the form of stun:<host>[:<port>][?transport=<udp|tcp>]
  // and adds an entry to this instance.
  bool AddStunServer(std::string_view url);

  // Parses a |url| in the form of
  // <stun|turn|turns>:<host>[:<port>][?transport=<udp|tcp>]
  // and adds an entry to this instance.
  bool AddServer(std::string_view url,
                 const std::string& username,
                 const std::string& password);

  // Time when the config will stop being valid and need to be refreshed.
  base::Time expiration_time;

  std::vector<rtc::SocketAddress> stun_servers;

  // Standard TURN servers
  std::vector<cricket::RelayServerConfig> turn_servers;

  // If greater than 0, the max bandwidth used for relayed connections should be
  // set to this value.
  int max_bitrate_kbps = 0;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_ICE_CONFIG_H_
