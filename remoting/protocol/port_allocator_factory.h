// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_PORT_ALLOCATOR_FACTORY_H_
#define REMOTING_PROTOCOL_PORT_ALLOCATOR_FACTORY_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"

namespace cricket {
class PortAllocator;
}  // namespace cricket

namespace remoting {
namespace protocol {

class SessionOptionsProvider;
class TransportContext;

// Factory class used for creating cricket::PortAllocator that is used
// to allocate ICE candidates.
class PortAllocatorFactory {
 public:
  virtual ~PortAllocatorFactory() {}

  virtual std::unique_ptr<cricket::PortAllocator> CreatePortAllocator(
      scoped_refptr<TransportContext> transport_context,
      base::WeakPtr<SessionOptionsProvider> session_options_provider) = 0;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_PORT_ALLOCATOR_FACTORY_H_
