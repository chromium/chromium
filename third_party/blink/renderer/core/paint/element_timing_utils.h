// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_ELEMENT_TIMING_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_ELEMENT_TIMING_UTILS_H_

#include "third_party/blink/renderer/platform/geometry/float_rect.h"

namespace blink {

class IntRect;
class LocalFrame;
class PropertyTreeState;

// Class containing methods shared between ImageElementTiming and
// TextElementTiming.
class ElementTimingUtils {
 public:
  // Computes the part a rect in a local transform space that is visible in the
  // specified frame, and returns a result in DIPs.
  static FloatRect ComputeIntersectionRect(LocalFrame*,
                                           const IntRect&,
                                           const PropertyTreeState&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_ELEMENT_TIMING_UTILS_H_
