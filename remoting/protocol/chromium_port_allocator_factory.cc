// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/chromium_port_allocator_factory.h"

#include "base/memory/ptr_util.h"
#include "remoting/protocol/chromium_socket_factory.h"
#include "remoting/protocol/port_allocator.h"
#include "remoting/protocol/transport_context.h"

namespace remoting {
namespace protocol {

ChromiumPortAllocatorFactory::ChromiumPortAllocatorFactory() = default;
ChromiumPortAllocatorFactory::~ChromiumPortAllocatorFactory() = default;

std::unique_ptr<cricket::PortAllocator>
ChromiumPortAllocatorFactory::CreatePortAllocator(
    scoped_refptr<TransportContext> transport_context,
    base::WeakPtr<SessionOptionsProvider> session_options_provider) {
  return std::make_unique<PortAllocator>(
      base::WrapUnique(new rtc::BasicNetworkManager()),
      base::WrapUnique(
          new ChromiumPacketSocketFactory(session_options_provider)),
      transport_context);
}

}  // namespace protocol
}  // namespace remoting
