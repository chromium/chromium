// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/fake_port_allocator.h"

#include <memory>
#include <string_view>

#include "base/functional/callback_helpers.h"
#include "remoting/protocol/transport_context.h"
#include "remoting/test/fake_network_dispatcher.h"
#include "remoting/test/fake_network_manager.h"
#include "remoting/test/fake_socket_factory.h"
#include "third_party/webrtc/p2p/client/basic_port_allocator.h"

namespace remoting {

namespace {

class FakePortAllocatorSession : public cricket::BasicPortAllocatorSession {
 public:
  FakePortAllocatorSession(FakePortAllocator* allocator,
                           const std::string& content_name,
                           int component,
                           const std::string& ice_username_fragment,
                           const std::string& ice_password);

  FakePortAllocatorSession(const FakePortAllocatorSession&) = delete;
  FakePortAllocatorSession& operator=(const FakePortAllocatorSession&) = delete;

  ~FakePortAllocatorSession() override;
};

FakePortAllocatorSession::FakePortAllocatorSession(
    FakePortAllocator* allocator,
    const std::string& content_name,
    int component,
    const std::string& ice_username_fragment,
    const std::string& ice_password)
    : BasicPortAllocatorSession(allocator,
                                content_name,
                                component,
                                ice_username_fragment,
                                ice_password) {}

FakePortAllocatorSession::~FakePortAllocatorSession() = default;

}  // namespace

FakePortAllocator::FakePortAllocator(
    rtc::NetworkManager* network_manager,
    rtc::PacketSocketFactory* socket_factory,
    scoped_refptr<protocol::TransportContext> transport_context)
    : BasicPortAllocator(network_manager, socket_factory),
      transport_context_(transport_context) {
  set_flags(cricket::PORTALLOCATOR_DISABLE_TCP |
            cricket::PORTALLOCATOR_ENABLE_IPV6 |
            cricket::PORTALLOCATOR_DISABLE_STUN |
            cricket::PORTALLOCATOR_DISABLE_RELAY);
  Initialize();
}

FakePortAllocator::~FakePortAllocator() = default;

cricket::PortAllocatorSession* FakePortAllocator::CreateSessionInternal(
    std::string_view content_name,
    int component,
    std::string_view ice_username_fragment,
    std::string_view ice_password) {
  return new FakePortAllocatorSession(
      this, std::string(content_name), component,
      std::string(ice_username_fragment), std::string(ice_password));
}

FakePortAllocatorFactory::FakePortAllocatorFactory(
    scoped_refptr<FakeNetworkDispatcher> fake_network_dispatcher) {
  socket_factory_ =
      std::make_unique<FakePacketSocketFactory>(fake_network_dispatcher.get());
  network_manager_ =
      std::make_unique<FakeNetworkManager>(socket_factory_->GetAddress());
}

FakePortAllocatorFactory::~FakePortAllocatorFactory() = default;

protocol::PortAllocatorFactory::CreatePortAllocatorResult
FakePortAllocatorFactory::CreatePortAllocator(
    scoped_refptr<protocol::TransportContext> transport_context,
    base::WeakPtr<protocol::SessionOptionsProvider> session_options_provider) {
  CreatePortAllocatorResult result;
  result.allocator = std::make_unique<FakePortAllocator>(
      network_manager_.get(), socket_factory_.get(), transport_context);
  result.apply_network_settings = base::DoNothing();
  return result;
}

}  // namespace remoting
