// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/chromium_port_allocator_factory.h"

#include "base/memory/ptr_util.h"
#include "remoting/protocol/chromium_socket_factory.h"
#include "remoting/protocol/port_allocator.h"
#include "remoting/protocol/transport_context.h"

namespace remoting::protocol {

ChromiumPortAllocatorFactory::ChromiumPortAllocatorFactory() = default;
ChromiumPortAllocatorFactory::~ChromiumPortAllocatorFactory() = default;

std::unique_ptr<cricket::PortAllocator>
ChromiumPortAllocatorFactory::CreatePortAllocator(
    scoped_refptr<TransportContext> transport_context,
    base::WeakPtr<SessionOptionsProvider> session_options_provider) {
  rtc::SocketFactory* socket_factory = transport_context->socket_factory();
  DCHECK(socket_factory);
  return std::make_unique<PortAllocator>(
      base::WrapUnique(new rtc::BasicNetworkManager(socket_factory)),
      base::WrapUnique(
          new ChromiumPacketSocketFactory(session_options_provider)),
      transport_context);
}

}  // namespace remoting::protocol
