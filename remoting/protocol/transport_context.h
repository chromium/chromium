// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_TRANSPORT_CONTEXT_H_
#define REMOTING_PROTOCOL_TRANSPORT_CONTEXT_H_

#include <list>
#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "remoting/protocol/ice_config.h"
#include "remoting/protocol/network_settings.h"
#include "remoting/protocol/transport.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace remoting {

class OAuthTokenGetter;

namespace protocol {

class PortAllocatorFactory;
class IceConfigRequest;

// TransportContext is responsible for storing all parameters required for
// P2P transport initialization. It's also responsible for fetching STUN and
// TURN configuration.
class TransportContext : public base::RefCountedThreadSafe<TransportContext> {
 public:
  typedef base::OnceCallback<void(const IceConfig& ice_config)>
      GetIceConfigCallback;

  static scoped_refptr<TransportContext> ForTests(TransportRole role);

  TransportContext(
      std::unique_ptr<PortAllocatorFactory> port_allocator_factory,
      rtc::SocketFactory* socket_factory,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      OAuthTokenGetter* oauth_token_getter,
      const NetworkSettings& network_settings,
      TransportRole role);

  TransportContext(const TransportContext&) = delete;
  TransportContext& operator=(const TransportContext&) = delete;

  void set_turn_ice_config(const IceConfig& ice_config) {
    DCHECK(!ice_config.is_null());
    // If an external entity provides a valid ICE Config, then disable the local
    // caching logic and use the provided config.
    //
    // Note: Using this method to provide a config means the caller is
    // responsible for ensuring the ICE config's validity and freshness.
    last_request_completion_time_ = base::Time::Max();
    ice_config_ = ice_config;
  }

  // Prepares fresh ICE configs. It may be called while connection is being
  // negotiated to minimize the chance that the following GetIceConfig() will
  // be blocking.
  void Prepare();

  // Requests fresh STUN and TURN information.
  void GetIceConfig(GetIceConfigCallback callback);

  PortAllocatorFactory* port_allocator_factory() {
    return port_allocator_factory_.get();
  }
  rtc::SocketFactory* socket_factory() const { return socket_factory_; }
  const NetworkSettings& network_settings() const { return network_settings_; }
  TransportRole role() const { return role_; }

  // Returns the suggested bandwidth cap for TURN relay connections, or 0 if
  // no rate-limit is set in the IceConfig.
  int GetTurnMaxRateKbps() const;

 private:
  friend class base::RefCountedThreadSafe<TransportContext>;

  ~TransportContext();

  void EnsureFreshIceConfig();
  void OnIceConfig(const IceConfig& ice_config);

  std::unique_ptr<PortAllocatorFactory> port_allocator_factory_;
  raw_ptr<rtc::SocketFactory> socket_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  raw_ptr<OAuthTokenGetter> oauth_token_getter_ = nullptr;
  NetworkSettings network_settings_;
  TransportRole role_;

  IceConfig ice_config_;

  base::Time last_request_completion_time_;
  std::unique_ptr<IceConfigRequest> ice_config_request_;

  // Called once |ice_config_request_| completes.
  std::list<GetIceConfigCallback> pending_ice_config_callbacks_;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_TRANSPORT_CONTEXT_H_
