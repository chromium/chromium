// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_HIT_TEST_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_HIT_TEST_DATA_H_

#include "third_party/blink/renderer/platform/geometry/region.h"
#include "third_party/blink/renderer/platform/graphics/hit_test_rect.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

class DisplayItemClient;
class GraphicsContext;

using HitTestRects = Vector<HitTestRect>;

struct PLATFORM_EXPORT HitTestData {
  HitTestRects touch_action_rects;
  HitTestRects wheel_event_handler_region;
  HitTestRects non_fast_scrollable_region;

  HitTestData() = default;
  HitTestData(const HitTestData& other)
      : touch_action_rects(other.touch_action_rects),
        wheel_event_handler_region(other.wheel_event_handler_region),
        non_fast_scrollable_region(other.non_fast_scrollable_region) {}

  bool operator==(const HitTestData& rhs) const {
    return touch_action_rects == rhs.touch_action_rects &&
           wheel_event_handler_region == rhs.wheel_event_handler_region &&
           non_fast_scrollable_region == rhs.non_fast_scrollable_region;
  }

  bool operator!=(const HitTestData& rhs) const { return !(*this == rhs); }

  // Records a display item for hit testing to ensure a paint chunk exists and
  // is sized to include hit test rects, then adds the hit test rect to
  // |HitTestData.touch_action_rects|.
  static void RecordHitTestRect(GraphicsContext&,
                                const DisplayItemClient&,
                                const HitTestRect&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_HIT_TEST_DATA_H_
