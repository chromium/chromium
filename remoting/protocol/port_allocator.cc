// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/port_allocator.h"

#include <algorithm>
#include <map>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "remoting/protocol/network_settings.h"
#include "remoting/protocol/transport_context.h"

namespace remoting {
namespace protocol {

PortAllocator::PortAllocator(
    std::unique_ptr<rtc::NetworkManager> network_manager,
    std::unique_ptr<rtc::PacketSocketFactory> socket_factory,
    scoped_refptr<TransportContext> transport_context)
    : BasicPortAllocator(network_manager.get(), socket_factory.get()),
      network_manager_(std::move(network_manager)),
      socket_factory_(std::move(socket_factory)),
      transport_context_(transport_context) {
  // We always use PseudoTcp to provide a reliable channel. It provides poor
  // performance when combined with TCP-based transport, so we have to disable
  // TCP ports. ENABLE_SHARED_UFRAG flag is specified so that the same username
  // fragment is shared between all candidates.
  // TODO(crbug.com/488760): Ideally we want to add
  // PORTALLOCATOR_DISABLE_COSTLY_NETWORKS, but this is unreliable on iOS and
  // may end up removing mobile networks when no WiFi is available. We may want
  // to add this flag only if there is WiFi interface.
  int flags = cricket::PORTALLOCATOR_DISABLE_TCP |
              cricket::PORTALLOCATOR_ENABLE_IPV6 |
              cricket::PORTALLOCATOR_ENABLE_IPV6_ON_WIFI;

  NetworkSettings network_settings = transport_context_->network_settings();

  if (!(network_settings.flags & NetworkSettings::NAT_TRAVERSAL_STUN))
    flags |= cricket::PORTALLOCATOR_DISABLE_STUN;

  if (!(network_settings.flags & NetworkSettings::NAT_TRAVERSAL_RELAY))
    flags |= cricket::PORTALLOCATOR_DISABLE_RELAY;

  set_flags(flags);
  SetPortRange(network_settings.port_range.min_port,
               network_settings.port_range.max_port);
  Initialize();
}

PortAllocator::~PortAllocator() = default;

cricket::PortAllocatorSession* PortAllocator::CreateSessionInternal(
    const std::string& content_name,
    int component,
    const std::string& ice_username_fragment,
    const std::string& ice_password) {
  return new PortAllocatorSession(this, content_name, component,
                                  ice_username_fragment, ice_password);
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
  transport_context_->GetIceConfig(base::Bind(
      &PortAllocatorSession::OnIceConfig, weak_factory_.GetWeakPtr()));
}

void PortAllocatorSession::OnIceConfig(const IceConfig& ice_config) {
  ice_config_ = ice_config;
  ConfigReady(GetPortConfiguration().release());
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

}  // namespace protocol
}  // namespace remoting
