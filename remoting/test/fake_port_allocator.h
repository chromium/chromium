// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_FAKE_PORT_ALLOCATOR_H_
#define REMOTING_TEST_FAKE_PORT_ALLOCATOR_H_

#include <memory>
#include <set>
#include <string_view>

#include "base/memory/scoped_refptr.h"
#include "remoting/protocol/port_allocator_factory.h"
#include "third_party/webrtc/p2p/client/basic_port_allocator.h"

namespace remoting {

class FakeNetworkDispatcher;
class FakePacketSocketFactory;

class FakePortAllocator : public cricket::BasicPortAllocator {
 public:
  FakePortAllocator(
      rtc::NetworkManager* network_manager,
      rtc::PacketSocketFactory* socket_factory,
      scoped_refptr<protocol::TransportContext> transport_context_);

  FakePortAllocator(const FakePortAllocator&) = delete;
  FakePortAllocator& operator=(const FakePortAllocator&) = delete;

  ~FakePortAllocator() override;

  // cricket::BasicPortAllocator overrides.
  cricket::PortAllocatorSession* CreateSessionInternal(
      std::string_view content_name,
      int component,
      std::string_view ice_username_fragment,
      std::string_view ice_password) override;

 private:
  scoped_refptr<protocol::TransportContext> transport_context_;
};

class FakePortAllocatorFactory : public protocol::PortAllocatorFactory {
 public:
  FakePortAllocatorFactory(
      scoped_refptr<FakeNetworkDispatcher> fake_network_dispatcher);

  FakePortAllocatorFactory(const FakePortAllocatorFactory&) = delete;
  FakePortAllocatorFactory& operator=(const FakePortAllocatorFactory&) = delete;

  ~FakePortAllocatorFactory() override;

  FakePacketSocketFactory* socket_factory() { return socket_factory_.get(); }

  // PortAllocatorFactory interface.
  CreatePortAllocatorResult CreatePortAllocator(
      scoped_refptr<protocol::TransportContext> transport_context,
      base::WeakPtr<protocol::SessionOptionsProvider> session_options_provider)
      override;

 private:
  std::unique_ptr<rtc::NetworkManager> network_manager_;
  std::unique_ptr<FakePacketSocketFactory> socket_factory_;
};

}  // namespace remoting

#endif  // REMOTING_TEST_FAKE_PORT_ALLOCATOR_H_
