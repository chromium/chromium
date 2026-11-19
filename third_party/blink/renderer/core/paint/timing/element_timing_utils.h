// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_ELEMENT_TIMING_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_ELEMENT_TIMING_UTILS_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "ui/gfx/geometry/rect_f.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace blink {
class LocalFrame;
class PropertyTreeStateOrAlias;
}  // namespace blink

// Helpers shared by element timing and container timing.
namespace blink::element_timing_utils {

// Computes the visible portion of a rect in DIPs, as exposed by the
// `intersectionRect` of element timing and container timing entries.
//
// TODO(crbug.com/586868204): This does not intersect with the viewports of
// ancestor frames, as LCP does.
CORE_EXPORT gfx::RectF ComputeIntersectionRect(LocalFrame*,
                                               const gfx::Rect&,
                                               const PropertyTreeStateOrAlias&);

}  // namespace blink::element_timing_utils

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_ELEMENT_TIMING_UTILS_H_
