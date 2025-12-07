// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_TRANSPORT_CONTEXT_H_
#define REMOTING_PROTOCOL_TRANSPORT_CONTEXT_H_

#include <list>
#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "remoting/protocol/ice_config.h"
#include "remoting/protocol/transport.h"
#include "third_party/webrtc/api/video_codecs/sdp_video_format.h"

namespace remoting::protocol {

class PortAllocatorFactory;
class IceConfigFetcher;

// TransportContext is responsible for storing all parameters required for
// P2P transport initialization. It's also responsible for fetching STUN and
// TURN configuration.
class TransportContext : public base::RefCountedThreadSafe<TransportContext> {
 public:
  using OnIceConfigCallback =
      base::OnceCallback<void(const IceConfig& ice_config)>;

  static scoped_refptr<TransportContext> ForTests(TransportRole role);

  TransportContext(std::unique_ptr<PortAllocatorFactory> port_allocator_factory,
                   webrtc::SocketFactory* socket_factory,
                   std::unique_ptr<IceConfigFetcher> ice_config_fetcher,
                   TransportRole role);

  TransportContext(const TransportContext&) = delete;
  TransportContext& operator=(const TransportContext&) = delete;

  void set_turn_ice_config(const IceConfig& ice_config) {
    DCHECK(!ice_config.is_null());
    // If an external entity provides a valid ICE Config, then disable the local
    // caching logic and use the provided config.
    //
    // Note: Using this method to provide a config means the caller must ensure
    // the ICE config is valid and has not expired.
    last_request_completion_time_ = base::Time::Max();
    ice_config_ = ice_config;
  }

  // Requests fresh STUN and TURN information.
  void GetIceConfig(OnIceConfigCallback callback);

  PortAllocatorFactory* port_allocator_factory() {
    return port_allocator_factory_.get();
  }
  webrtc::SocketFactory* socket_factory() const { return socket_factory_; }
  TransportRole role() const { return role_; }

  // Modifies the SDP messages sent from the client to prefer a specific video
  // format. If this option is not set, the default video codec (VP8) is used.
  const std::optional<webrtc::SdpVideoFormat>& preferred_video_format() {
    return preferred_video_format_;
  }
  void set_preferred_video_format(webrtc::SdpVideoFormat format) {
    CHECK(role_ == TransportRole::CLIENT)
        << "Preferred video format can only be set by the client";
    preferred_video_format_ = std::move(format);
  }

  // Returns the suggested bandwidth cap for TURN relay connections, or 0 if
  // no rate-limit is set in the IceConfig.
  int GetTurnMaxRateKbps() const;

 private:
  friend class base::RefCountedThreadSafe<TransportContext>;

  ~TransportContext();

  void EnsureFreshIceConfig();
  void OnIceConfig(std::optional<IceConfig> ice_config);

  std::unique_ptr<PortAllocatorFactory> port_allocator_factory_;
  raw_ptr<webrtc::SocketFactory> socket_factory_;
  TransportRole role_;

  std::optional<webrtc::SdpVideoFormat> preferred_video_format_;

  IceConfig ice_config_;
  base::Time last_request_completion_time_;
  bool ice_config_request_in_flight_ = false;
  std::unique_ptr<IceConfigFetcher> ice_config_fetcher_;

  // Called once |ice_config_request_| completes.
  std::list<OnIceConfigCallback> pending_ice_config_callbacks_;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_TRANSPORT_CONTEXT_H_
