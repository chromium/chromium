// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_GLOBAL_ROUTING_ID_H_
#define CONTENT_PUBLIC_BROWSER_GLOBAL_ROUTING_ID_H_

#include <tuple>

#include "base/hash/hash.h"
#include "ipc/ipc_message.h"

namespace content {

// Uniquely identifies a target that legacy IPCs can be routed to.
struct GlobalRoutingID {
  GlobalRoutingID() : child_id(-1), route_id(-1) {}

  GlobalRoutingID(int child_id, int route_id)
      : child_id(child_id), route_id(route_id) {}

  // The unique ID of the child process (this is different from OS's PID / this
  // should come from RenderProcessHost::GetID()).
  int child_id;

  // The route ID.
  int route_id;

  bool operator<(const GlobalRoutingID& other) const {
    return std::tie(child_id, route_id) <
           std::tie(other.child_id, other.route_id);
  }
  bool operator==(const GlobalRoutingID& other) const {
    return child_id == other.child_id && route_id == other.route_id;
  }
  bool operator!=(const GlobalRoutingID& other) const {
    return !(*this == other);
  }
};

// Same as GlobalRoutingID except the route_id must be a RenderFrameHost routing
// id.
struct GlobalFrameRoutingId {
  GlobalFrameRoutingId() : child_id(0), frame_routing_id(MSG_ROUTING_NONE) {}

  GlobalFrameRoutingId(int child_id, int frame_routing_id)
      : child_id(child_id), frame_routing_id(frame_routing_id) {}

  // GlobalFrameRoutingId is copyable.
  GlobalFrameRoutingId(const GlobalFrameRoutingId&) = default;
  GlobalFrameRoutingId& operator=(const GlobalFrameRoutingId&) = default;

  // The unique ID of the child process (this is different from OS's PID / this
  // should come from RenderProcessHost::GetID()).
  int child_id;

  // The route ID of a RenderFrame - should come from
  // RenderFrameHost::GetRoutingID().
  int frame_routing_id;

  bool operator<(const GlobalFrameRoutingId& other) const {
    return std::tie(child_id, frame_routing_id) <
           std::tie(other.child_id, other.frame_routing_id);
  }
  bool operator==(const GlobalFrameRoutingId& other) const {
    return child_id == other.child_id &&
           frame_routing_id == other.frame_routing_id;
  }
  bool operator!=(const GlobalFrameRoutingId& other) const {
    return !(*this == other);
  }
};

struct GlobalFrameRoutingIdHasher {
  std::size_t operator()(const GlobalFrameRoutingId& id) const {
    return base::HashInts(id.child_id, id.frame_routing_id);
  }
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_GLOBAL_ROUTING_ID_H_
