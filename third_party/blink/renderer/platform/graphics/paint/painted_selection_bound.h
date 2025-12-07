// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINTED_SELECTION_BOUND_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINTED_SELECTION_BOUND_H_

#include "third_party/blink/renderer/platform/platform_export.h"

#include "ui/gfx/geometry/point.h"
#include "ui/gfx/selection_bound.h"

namespace blink {

// Blink's notion of cc::LayerSelectionBound. Note that the points are
// gfx::Points to match the painted selection rect, which is always pixel
// aligned. There are also no layer_id and hidden as those are determined
// at composition time.
struct PLATFORM_EXPORT PaintedSelectionBound {
  gfx::SelectionBound::Type type;
  gfx::Point edge_start;
  gfx::Point edge_end;

  DISALLOW_NEW();
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINTED_SELECTION_BOUND_H_
