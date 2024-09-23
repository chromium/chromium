// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/port_allocator.h"

#include <algorithm>
#include <map>
#include <string_view>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "remoting/base/logging.h"
#include "remoting/protocol/network_settings.h"
#include "remoting/protocol/transport_context.h"

namespace remoting::protocol {

PortAllocator::PortAllocator(
    std::unique_ptr<rtc::NetworkManager> network_manager,
    std::unique_ptr<rtc::PacketSocketFactory> socket_factory,
    scoped_refptr<TransportContext> transport_context)
    : BasicPortAllocator(network_manager.get(), socket_factory.get()),
      network_manager_(std::move(network_manager)),
      socket_factory_(std::move(socket_factory)),
      transport_context_(transport_context) {}

PortAllocator::~PortAllocator() = default;

void PortAllocator::ApplyNetworkSettings(
    const NetworkSettings& network_settings) {
  DCHECK(!network_settings_applied_);
  // We always use PseudoTcp to provide a reliable channel. It provides poor
  // performance when combined with TCP-based transport, so we have to disable
  // TCP ports. ENABLE_SHARED_UFRAG flag is specified so that the same username
  // fragment is shared between all candidates.
  // TODO(crbug.com/41175043): Ideally we want to add
  // PORTALLOCATOR_DISABLE_COSTLY_NETWORKS, but this is unreliable on iOS and
  // may end up removing mobile networks when no WiFi is available. We may want
  // to add this flag only if there is WiFi interface.
  int flags = cricket::PORTALLOCATOR_DISABLE_TCP |
              cricket::PORTALLOCATOR_ENABLE_IPV6 |
              cricket::PORTALLOCATOR_ENABLE_IPV6_ON_WIFI;

  if (!(network_settings.flags & NetworkSettings::NAT_TRAVERSAL_STUN)) {
    flags |= cricket::PORTALLOCATOR_DISABLE_STUN;
  }

  if (!(network_settings.flags & NetworkSettings::NAT_TRAVERSAL_RELAY)) {
    flags |= cricket::PORTALLOCATOR_DISABLE_RELAY;
  }

  set_flags(flags);
  SetPortRange(network_settings.port_range.min_port,
               network_settings.port_range.max_port);
  Initialize();
  network_settings_applied_ = true;
}

cricket::PortAllocatorSession* PortAllocator::CreateSessionInternal(
    std::string_view content_name,
    int component,
    std::string_view ice_username_fragment,
    std::string_view ice_password) {
  // Use `CHECK` since network settings not having been applied may lead to
  // policy violations.
  CHECK(network_settings_applied_) << "Network settings have not been applied.";
  return new PortAllocatorSession(this, std::string(content_name), component,
                                  std::string(ice_username_fragment),
                                  std::string(ice_password));
}

PortAllocatorSession::PortAllocatorSession(PortAllocator* allocator,
                                           const std::string& content_name,
                                           int component,
                                           const std::string& ice_ufrag,
                                           const std::string& ice_pwd)
    : BasicPortAllocatorSession(allocator,
                                content_name,
                                component,
                                ice_ufrag,
                                ice_pwd),
      transport_context_(allocator->transport_context()) {}

PortAllocatorSession::~PortAllocatorSession() = default;

void PortAllocatorSession::GetPortConfigurations() {
  // Don't need to make ICE config request if both STUN and Relay are disabled.
  if ((flags() & cricket::PORTALLOCATOR_DISABLE_STUN) &&
      (flags() & cricket::PORTALLOCATOR_DISABLE_RELAY)) {
    HOST_LOG << "Skipping ICE Config request as STUN and RELAY are disabled";
    OnIceConfig(IceConfig());
  } else {
    transport_context_->GetIceConfig(base::BindOnce(
        &PortAllocatorSession::OnIceConfig, weak_factory_.GetWeakPtr()));
  }
}

void PortAllocatorSession::OnIceConfig(const IceConfig& ice_config) {
  ice_config_ = ice_config;
  ConfigReady(GetPortConfiguration());
}

std::unique_ptr<cricket::PortConfiguration>
PortAllocatorSession::GetPortConfiguration() {
  cricket::ServerAddresses stun_servers;
  for (const auto& host : ice_config_.stun_servers) {
    stun_servers.insert(host);
  }

  std::unique_ptr<cricket::PortConfiguration> config(
      new cricket::PortConfiguration(stun_servers, username(), password()));

  if (relay_enabled()) {
    for (const auto& turn_server : ice_config_.turn_servers) {
      config->AddRelay(turn_server);
    }
  }

  return config;
}

}  // namespace remoting::protocol
