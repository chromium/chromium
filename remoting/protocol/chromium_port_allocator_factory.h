// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CHROMIUM_PORT_ALLOCATOR_FACTORY_H_
#define REMOTING_PROTOCOL_CHROMIUM_PORT_ALLOCATOR_FACTORY_H_

#include <memory>
#include <set>

#include "base/memory/ref_counted.h"
#include "remoting/protocol/port_allocator_factory.h"

namespace remoting {
namespace protocol {

class ChromiumPortAllocatorFactory : public PortAllocatorFactory {
 public:
  ChromiumPortAllocatorFactory();

  ChromiumPortAllocatorFactory(const ChromiumPortAllocatorFactory&) = delete;
  ChromiumPortAllocatorFactory& operator=(const ChromiumPortAllocatorFactory&) =
      delete;

  ~ChromiumPortAllocatorFactory() override;

   // PortAllocatorFactory interface.
  std::unique_ptr<cricket::PortAllocator> CreatePortAllocator(
      scoped_refptr<TransportContext> transport_context,
      base::WeakPtr<SessionOptionsProvider> session_options_provider) override;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_CHROMIUM_PORT_ALLOCATOR_FACTORY_H_
