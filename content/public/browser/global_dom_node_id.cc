// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/global_dom_node_id.h"

#include <ostream>

#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"

namespace content {

std::ostream& operator<<(std::ostream& os, const GlobalDOMNodeId& target_id) {
  const RenderFrameHost* rfh = target_id.document.AsRenderFrameHostIfValid();

  return os << "GlobalDOMNodeId("
            << (rfh ? rfh->GetGlobalId() : GlobalRenderFrameHostId()) << ", "
            << target_id.target_element_dom_id << ")";
}

}  // namespace content
