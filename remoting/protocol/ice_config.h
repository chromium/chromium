// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_ICE_CONFIG_H_
#define REMOTING_PROTOCOL_ICE_CONFIG_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "third_party/webrtc/p2p/base/port_allocator.h"
#include "third_party/webrtc/rtc_base/socket_address.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace remoting {

namespace apis {
namespace v1 {
class GetIceConfigResponse;
}  // namespace v1
}  // namespace apis

namespace protocol {

struct IceConfig {
  IceConfig();
  IceConfig(const IceConfig& other);
  ~IceConfig();

  bool is_null() const { return expiration_time.is_null(); }

  // Parses JSON representation of the config. Returns null config if parsing
  // fails.
  static IceConfig Parse(const base::DictionaryValue& dictionary);
  static IceConfig Parse(const std::string& config_json);
  static IceConfig Parse(const apis::v1::GetIceConfigResponse& config);

  // Time when the config will stop being valid and need to be refreshed.
  base::Time expiration_time;

  std::vector<rtc::SocketAddress> stun_servers;

  // Standard TURN servers
  std::vector<cricket::RelayServerConfig> turn_servers;

  // If greater than 0, the max bandwidth used for relayed connections should
  // be set to this value.
  int max_bitrate_kbps = 0;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_ICE_CONFIG_H_
