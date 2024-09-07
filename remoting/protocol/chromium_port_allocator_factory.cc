// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/chromium_port_allocator_factory.h"

#include "base/functional/bind.h"
#include "remoting/protocol/chromium_socket_factory.h"
#include "remoting/protocol/port_allocator.h"
#include "remoting/protocol/transport_context.h"

namespace remoting::protocol {

ChromiumPortAllocatorFactory::ChromiumPortAllocatorFactory() = default;
ChromiumPortAllocatorFactory::~ChromiumPortAllocatorFactory() = default;

PortAllocatorFactory::CreatePortAllocatorResult
ChromiumPortAllocatorFactory::CreatePortAllocator(
    scoped_refptr<TransportContext> transport_context,
    base::WeakPtr<SessionOptionsProvider> session_options_provider) {
  rtc::SocketFactory* socket_factory = transport_context->socket_factory();
  DCHECK(socket_factory);
  CreatePortAllocatorResult result;
  auto allocator = std::make_unique<PortAllocator>(
      std::make_unique<rtc::BasicNetworkManager>(socket_factory),
      std::make_unique<ChromiumPacketSocketFactory>(session_options_provider),
      transport_context);
  result.apply_network_settings = base::BindOnce(
      &PortAllocator::ApplyNetworkSettings, allocator->GetWeakPtr());
  result.allocator = std::move(allocator);
  return result;
}

}  // namespace remoting::protocol
