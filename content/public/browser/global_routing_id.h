// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_GLOBAL_ROUTING_ID_H_
#define CONTENT_PUBLIC_BROWSER_GLOBAL_ROUTING_ID_H_

#include <compare>
#include <ostream>

#include "base/hash/hash.h"
#include "base/i18n/number_formatting.h"
#include "content/common/content_export.h"
#include "content/public/common/content_constants.h"
#include "ipc/ipc_message.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"

namespace perfetto::protos::pbzero {
class GlobalRenderFrameHostId;
}  // namespace perfetto::protos::pbzero

namespace content {

// Uniquely identifies a target that legacy IPCs can be routed to.
//
// These IDs can be considered to be unique for the lifetime of the browser
// process. While they are finite and thus must eventually roll over, this case
// may be considered sufficiently rare as to be ignorable.
struct CONTENT_EXPORT GlobalRoutingID {
  GlobalRoutingID() = default;

  GlobalRoutingID(int child_id, int route_id)
      : child_id(child_id), route_id(route_id) {}

  // The unique ID of the child process (this is different from OS's PID / this
  // should come from RenderProcessHost::GetID()).
  int child_id = kInvalidChildProcessUniqueId;

  // The route ID.
  int route_id = -1;

  constexpr friend auto operator<=>(const GlobalRoutingID&,
                                    const GlobalRoutingID&) = default;
  constexpr friend bool operator==(const GlobalRoutingID&,
                                   const GlobalRoutingID&) = default;
};

inline std::ostream& operator<<(std::ostream& os, const GlobalRoutingID& id) {
  os << "GlobalRoutingID(" << id.child_id << ", " << id.route_id << ")";
  return os;
}

// Same as GlobalRoutingID except the route_id must be a RenderFrameHost routing
// id.
//
// These IDs can be considered to be unique for the lifetime of the browser
// process. While they are finite and thus must eventually roll over, this case
// may be considered sufficiently rare as to be ignorable.
struct CONTENT_EXPORT GlobalRenderFrameHostId {
  GlobalRenderFrameHostId() = default;

  GlobalRenderFrameHostId(int child_id, int frame_routing_id)
      : child_id(child_id), frame_routing_id(frame_routing_id) {}

  // GlobalRenderFrameHostId is copyable.
  GlobalRenderFrameHostId(const GlobalRenderFrameHostId&) = default;
  GlobalRenderFrameHostId& operator=(const GlobalRenderFrameHostId&) = default;

  // The unique ID of the child process (this is different from OS's PID / this
  // should come from RenderProcessHost::GetID()).
  int child_id = 0;

  // The route ID of a RenderFrame - should come from
  // RenderFrameHost::GetRoutingID().
  int frame_routing_id = MSG_ROUTING_NONE;

  constexpr friend auto operator<=>(const GlobalRenderFrameHostId&,
                                    const GlobalRenderFrameHostId&) = default;
  constexpr friend bool operator==(const GlobalRenderFrameHostId&,
                                   const GlobalRenderFrameHostId&) = default;

  explicit operator bool() const {
    return frame_routing_id != MSG_ROUTING_NONE;
  }

  using TraceProto = perfetto::protos::pbzero::GlobalRenderFrameHostId;
  // Write a representation of this object into proto.
  void WriteIntoTrace(perfetto::TracedProto<TraceProto> proto) const;
};

// Similar to GlobalRenderFrameHostId except that it uses FrameTokens instead
// of routing ids.
//
// These tokens can be considered to be unique for the lifetime of the browser
// process.
struct GlobalRenderFrameHostToken {
  GlobalRenderFrameHostToken() = default;

  // GlobalRenderFrameHostToken is copyable.
  GlobalRenderFrameHostToken(const GlobalRenderFrameHostToken&) = default;
  GlobalRenderFrameHostToken& operator=(const GlobalRenderFrameHostToken&) =
      default;

  GlobalRenderFrameHostToken(int child_id,
                             const blink::LocalFrameToken& frame_token)
      : child_id(child_id), frame_token(frame_token) {}

  // The unique ID of the child process (this is different from OS's PID / this
  // should come from RenderProcessHost::GetID()).
  int child_id = kInvalidChildProcessUniqueId;

  // The `LocalFrameToken` of blink::WebLocalFrame - should come from
  // RenderFrameHost::GetFrameToken().
  blink::LocalFrameToken frame_token;

  constexpr friend auto operator<=>(const GlobalRenderFrameHostToken&,
                                    const GlobalRenderFrameHostToken&) =
      default;
  constexpr friend bool operator==(const GlobalRenderFrameHostToken&,
                                   const GlobalRenderFrameHostToken&) = default;
};

inline std::ostream& operator<<(std::ostream& os,
                                const GlobalRenderFrameHostId& id) {
  os << "GlobalRenderFrameHostId(" << id.child_id << ", " << id.frame_routing_id
     << ")";
  return os;
}

struct GlobalRenderFrameHostIdHasher {
  std::size_t operator()(const GlobalRenderFrameHostId& id) const {
    return base::HashInts(id.child_id, id.frame_routing_id);
  }
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_GLOBAL_ROUTING_ID_H_
