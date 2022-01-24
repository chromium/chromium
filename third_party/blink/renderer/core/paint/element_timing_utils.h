// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_ELEMENT_TIMING_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_ELEMENT_TIMING_UTILS_H_

namespace gfx {
class Rect;
class RectF;
}  // namespace gfx

namespace blink {

class LocalFrame;
class PropertyTreeStateOrAlias;

// Class containing methods shared between ImageElementTiming and
// TextElementTiming.
class ElementTimingUtils {
 public:
  // Computes the part a rect in a local transform space that is visible in the
  // specified frame, and returns a result in DIPs.
  static gfx::RectF ComputeIntersectionRect(LocalFrame*,
                                            const gfx::Rect&,
                                            const PropertyTreeStateOrAlias&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_ELEMENT_TIMING_UTILS_H_
