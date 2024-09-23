// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_PORT_ALLOCATOR_H_
#define REMOTING_PROTOCOL_PORT_ALLOCATOR_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "remoting/protocol/ice_config.h"
#include "remoting/protocol/network_settings.h"
#include "remoting/protocol/transport_context.h"
#include "third_party/webrtc/p2p/client/basic_port_allocator.h"

namespace remoting::protocol {

class PortAllocator : public cricket::BasicPortAllocator {
 public:
  PortAllocator(std::unique_ptr<rtc::NetworkManager> network_manager,
                std::unique_ptr<rtc::PacketSocketFactory> socket_factory,
                scoped_refptr<TransportContext> transport_context);
  ~PortAllocator() override;

  void ApplyNetworkSettings(const NetworkSettings& network_settings);

  scoped_refptr<TransportContext> transport_context() {
    return transport_context_;
  }

  cricket::PortAllocatorSession* CreateSessionInternal(
      std::string_view content_name,
      int component,
      std::string_view ice_ufrag,
      std::string_view ice_pwd) override;

  base::WeakPtr<PortAllocator> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  std::unique_ptr<rtc::NetworkManager> network_manager_;
  std::unique_ptr<rtc::PacketSocketFactory> socket_factory_;
  scoped_refptr<TransportContext> transport_context_;
  bool network_settings_applied_ = false;

  base::WeakPtrFactory<PortAllocator> weak_factory_{this};
};

class PortAllocatorSession : public cricket::BasicPortAllocatorSession {
 public:
  PortAllocatorSession(PortAllocator* allocator,
                       const std::string& content_name,
                       int component,
                       const std::string& ice_ufrag,
                       const std::string& ice_pwd);
  ~PortAllocatorSession() override;

 private:
  bool relay_enabled() {
    return !(flags() & cricket::PORTALLOCATOR_DISABLE_RELAY);
  }

  // BasicPortAllocatorSession overrides.
  void GetPortConfigurations() override;

  // Callback for TransportContext::GetIceConfig().
  void OnIceConfig(const IceConfig& ice_config);

  // Creates PortConfiguration that includes STUN and TURN servers from
  // |ice_config_|.
  std::unique_ptr<cricket::PortConfiguration> GetPortConfiguration();

  scoped_refptr<TransportContext> transport_context_;

  IceConfig ice_config_;

  base::WeakPtrFactory<PortAllocatorSession> weak_factory_{this};
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_PORT_ALLOCATOR_H_
