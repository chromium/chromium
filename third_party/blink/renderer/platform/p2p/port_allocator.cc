// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/p2p/port_allocator.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/check.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/webrtc/api/local_network_access_permission.h"
#include "third_party/webrtc_overrides/environment.h"

namespace blink {

P2PPortAllocator::P2PPortAllocator(
    std::unique_ptr<webrtc::NetworkManager> network_manager,
    webrtc::PacketSocketFactory* socket_factory,
    const Config& config,
    std::unique_ptr<webrtc::LocalNetworkAccessPermissionFactoryInterface>
        lna_permission_factory)
    : webrtc::BasicPortAllocator(WebRtcEnvironment(),
                                 network_manager.get(),
                                 socket_factory,
                                 /*turn_customizer=*/nullptr,
                                 /*relay_port_factory=*/nullptr,
                                 std::move(lna_permission_factory)),
      network_manager_(std::move(network_manager)),
      config_(config) {
  DCHECK(network_manager_);
  DCHECK(socket_factory);
  uint32_t flags = 0;
  if (!config_.enable_multiple_routes) {
    flags |= webrtc::PORTALLOCATOR_DISABLE_ADAPTER_ENUMERATION;
  }
  if (!config_.enable_default_local_candidate) {
    flags |= webrtc::PORTALLOCATOR_DISABLE_DEFAULT_LOCAL_CANDIDATE;
  }
  if (!config_.enable_nonproxied_udp) {
    flags |= webrtc::PORTALLOCATOR_DISABLE_UDP |
             webrtc::PORTALLOCATOR_DISABLE_STUN |
             webrtc::PORTALLOCATOR_DISABLE_UDP_RELAY;
  }
  set_flags(flags);
  set_allow_tcp_listen(false);
}

P2PPortAllocator::~P2PPortAllocator() {}

void P2PPortAllocator::Initialize() {
  BasicPortAllocator::Initialize();
  network_manager_->Initialize();
}

}  // namespace blink
