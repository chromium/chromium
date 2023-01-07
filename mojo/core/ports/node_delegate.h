// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_PORTS_NODE_DELEGATE_H_
#define MOJO_CORE_PORTS_NODE_DELEGATE_H_

#include <stddef.h>

#include "mojo/core/ports/event.h"
#include "mojo/core/ports/name.h"
#include "mojo/core/ports/port_ref.h"

namespace mojo {
namespace core {
namespace ports {

class NodeDelegate {
 public:
  virtual ~NodeDelegate() = default;

  // Forward an event (possibly asynchronously) to the specified node.
  virtual void ForwardEvent(const NodeName& node, ScopedEvent event) = 0;

  // Broadcast an event to all nodes.
  virtual void BroadcastEvent(ScopedEvent event) = 0;

  // Indicates that the port's status has changed recently. Use Node::GetStatus
  // to query the latest status of the port. Note, this event could be spurious
  // if another thread is simultaneously modifying the status of the port.
  virtual void PortStatusChanged(const PortRef& port_ref) = 0;
};

}  // namespace ports
}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_PORTS_NODE_DELEGATE_H_
