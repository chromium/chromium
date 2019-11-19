// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_TRANSPORT_CONTEXT_H_
#define REMOTING_PROTOCOL_TRANSPORT_CONTEXT_H_

#include <array>
#include <list>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "remoting/protocol/ice_config.h"
#include "remoting/protocol/network_settings.h"
#include "remoting/protocol/transport.h"

namespace rtc {
class NetworkManager;
}  // namespace rtc

namespace remoting {

class UrlRequestFactory;

namespace protocol {

class PortAllocatorFactory;
class IceConfigRequest;

// TransportContext is responsible for storing all parameters required for
// P2P transport initialization. It's also responsible for fetching STUN and
// TURN configuration.
class TransportContext : public base::RefCountedThreadSafe<TransportContext> {
 public:
  // TODO(yuweih): See if we still need this enum.
  enum RelayMode {
    TURN,

    LAST_RELAYMODE = TURN
  };
  static const int kNumRelayModes = RelayMode::LAST_RELAYMODE + 1;

  typedef base::Callback<void(const IceConfig& ice_config)>
      GetIceConfigCallback;

  static scoped_refptr<TransportContext> ForTests(TransportRole role);

  TransportContext(std::unique_ptr<PortAllocatorFactory> port_allocator_factory,
                   std::unique_ptr<UrlRequestFactory> url_request_factory,
                   const NetworkSettings& network_settings,
                   TransportRole role);

  void set_turn_ice_config(const IceConfig& ice_config) {
    ice_config_[TURN] = ice_config;
  }

  // Sets relay mode for all future calls of GetIceConfig(). Doesn't affect
  // previous GetIceConfig() requests.
  void set_relay_mode(RelayMode relay_mode) { relay_mode_ = relay_mode; }

  // Sets a reference to the NetworkManager that holds the list of
  // network interfaces. If the NetworkManager is deleted while this
  // TransportContext is live, the caller should set this to nullptr.
  // TODO(crbug.com/848045): This should be a singleton - either a global
  // instance, or one that is owned by this TransportContext.
  void set_network_manager(rtc::NetworkManager* network_manager) {
    network_manager_ = network_manager;
  }

  // Prepares fresh ICE configs. It may be called while connection is being
  // negotiated to minimize the chance that the following GetIceConfig() will
  // be blocking.
  void Prepare();

  // Requests fresh STUN and TURN information.
  void GetIceConfig(const GetIceConfigCallback& callback);

  PortAllocatorFactory* port_allocator_factory() {
    return port_allocator_factory_.get();
  }
  UrlRequestFactory* url_request_factory() {
    return url_request_factory_.get();
  }
  const NetworkSettings& network_settings() const { return network_settings_; }
  TransportRole role() const { return role_; }
  rtc::NetworkManager* network_manager() const { return network_manager_; }

  // Returns the suggested bandwidth cap for TURN relay connections, or 0 if
  // no rate-limit is set in the IceConfig.
  int GetTurnMaxRateKbps() const;

 private:
  friend class base::RefCountedThreadSafe<TransportContext>;

  ~TransportContext();

  void EnsureFreshIceConfig();
  void OnIceConfig(RelayMode relay_mode, const IceConfig& ice_config);

  std::unique_ptr<PortAllocatorFactory> port_allocator_factory_;
  std::unique_ptr<UrlRequestFactory> url_request_factory_;
  NetworkSettings network_settings_;
  TransportRole role_;

  RelayMode relay_mode_ = RelayMode::TURN;

  rtc::NetworkManager* network_manager_ = nullptr;

  std::array<std::unique_ptr<IceConfigRequest>, kNumRelayModes>
      ice_config_request_;
  std::array<IceConfig, kNumRelayModes> ice_config_;

  // When there is an active |ice_config_request_| stores list of callbacks to
  // be called once the request is finished.
  std::array<std::list<GetIceConfigCallback>, kNumRelayModes>
      pending_ice_config_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(TransportContext);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_TRANSPORT_CONTEXT_H_
