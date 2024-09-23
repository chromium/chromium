// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CHROMIUM_PORT_ALLOCATOR_FACTORY_H_
#define REMOTING_PROTOCOL_CHROMIUM_PORT_ALLOCATOR_FACTORY_H_

#include "base/memory/scoped_refptr.h"
#include "remoting/protocol/port_allocator_factory.h"

namespace remoting::protocol {

class ChromiumPortAllocatorFactory : public PortAllocatorFactory {
 public:
  ChromiumPortAllocatorFactory();

  ChromiumPortAllocatorFactory(const ChromiumPortAllocatorFactory&) = delete;
  ChromiumPortAllocatorFactory& operator=(const ChromiumPortAllocatorFactory&) =
      delete;

  ~ChromiumPortAllocatorFactory() override;

  // PortAllocatorFactory interface.
  CreatePortAllocatorResult CreatePortAllocator(
      scoped_refptr<TransportContext> transport_context,
      base::WeakPtr<SessionOptionsProvider> session_options_provider) override;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_CHROMIUM_PORT_ALLOCATOR_FACTORY_H_
