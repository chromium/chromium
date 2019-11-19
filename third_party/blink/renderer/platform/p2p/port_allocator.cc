// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/p2p/port_allocator.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/logging.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/p2p/socket_dispatcher.h"

namespace blink {

P2PPortAllocator::P2PPortAllocator(
    const scoped_refptr<P2PSocketDispatcher>& socket_dispatcher,
    std::unique_ptr<rtc::NetworkManager> network_manager,
    rtc::PacketSocketFactory* socket_factory,
    const Config& config,
    const GURL& origin)
    : cricket::BasicPortAllocator(network_manager.get(), socket_factory),
      network_manager_(std::move(network_manager)),
      socket_dispatcher_(socket_dispatcher),
      config_(config),
      origin_(origin) {
  DCHECK(socket_dispatcher);
  DCHECK(network_manager_);
  DCHECK(socket_factory);
  uint32_t flags = 0;
  if (!config_.enable_multiple_routes) {
    flags |= cricket::PORTALLOCATOR_DISABLE_ADAPTER_ENUMERATION;
  }
  if (!config_.enable_default_local_candidate) {
    flags |= cricket::PORTALLOCATOR_DISABLE_DEFAULT_LOCAL_CANDIDATE;
  }
  if (!config_.enable_nonproxied_udp) {
    flags |= cricket::PORTALLOCATOR_DISABLE_UDP |
             cricket::PORTALLOCATOR_DISABLE_STUN |
             cricket::PORTALLOCATOR_DISABLE_UDP_RELAY;
  }
  set_flags(flags);
  set_allow_tcp_listen(false);
  bool enable_webrtc_stun_origin =
      Platform::Current()->IsWebRtcStunOriginEnabled();
  if (enable_webrtc_stun_origin) {
    set_origin(origin_.spec());
  }
}

P2PPortAllocator::~P2PPortAllocator() {}

void P2PPortAllocator::Initialize() {
  BasicPortAllocator::Initialize();
  network_manager_->Initialize();
}

}  // namespace blink
