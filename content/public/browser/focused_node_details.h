// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_FOCUSED_NODE_DETAILS_H_
#define CONTENT_PUBLIC_BROWSER_FOCUSED_NODE_DETAILS_H_

#include "content/public/browser/global_dom_node_id.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom.h"
#include "ui/gfx/geometry/rect.h"

namespace content {

struct FocusedNodeDetails {
  bool is_editable_node;
  gfx::Rect node_bounds_in_screen;
  blink::mojom::FocusType focus_type;
  GlobalDOMNodeId global_dom_node_id;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_FOCUSED_NODE_DETAILS_H_
