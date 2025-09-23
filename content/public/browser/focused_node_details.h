// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_FOCUSED_NODE_DETAILS_H_
#define CONTENT_PUBLIC_BROWSER_FOCUSED_NODE_DETAILS_H_

#include "third_party/blink/public/mojom/input/focus_type.mojom.h"
#include "ui/gfx/geometry/rect.h"

namespace content {

struct FocusedNodeDetails {
  bool is_editable_node;
  // TODO(crbug.com/446734707): Investigate which uses of
  // `node_bounds_in_screen` can be replaced by `node_bounds_in_root_view`.
  gfx::Rect node_bounds_in_screen;
  gfx::Rect node_bounds_in_root_view;
  blink::mojom::FocusType focus_type;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_FOCUSED_NODE_DETAILS_H_
