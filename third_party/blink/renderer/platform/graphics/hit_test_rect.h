// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_HIT_TEST_RECT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_HIT_TEST_RECT_H_

#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/graphics/touch_action.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace cc {
class TouchActionRegion;
}

namespace blink {

class PaintLayer;

struct PLATFORM_EXPORT HitTestRect {
  // HitTestRect is a class shared by touch action region, wheel event handler
  // region and non fast scrollable region. Wheel event handler region and
  // non-fast scrollable rects use a |whitelisted_touch_action| of none.
  LayoutRect rect;
  TouchAction whitelisted_touch_action;

  HitTestRect(const LayoutRect& layout_rect)
      : HitTestRect(layout_rect, TouchAction::kTouchActionNone) {}
  HitTestRect(const LayoutRect& layout_rect, TouchAction action)
      : rect(layout_rect), whitelisted_touch_action(action) {}

  static cc::TouchActionRegion BuildRegion(const Vector<HitTestRect>&);
  static LayoutRect GetBounds(const Vector<HitTestRect>&);

  bool operator==(const HitTestRect& rhs) const {
    return rect == rhs.rect &&
           whitelisted_touch_action == rhs.whitelisted_touch_action;
  }

  bool operator!=(const HitTestRect& rhs) const { return !(*this == rhs); }
};

using LayerHitTestRects = WTF::HashMap<const PaintLayer*, Vector<HitTestRect>>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_HIT_TEST_RECT_H_
