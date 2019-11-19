// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_HIT_TEST_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_HIT_TEST_DATA_H_

#include "third_party/blink/renderer/platform/graphics/hit_test_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/transform_paint_property_node.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

struct PLATFORM_EXPORT HitTestData {
  Vector<HitTestRect> touch_action_rects;
  struct ScrollHitTest {
    const TransformPaintPropertyNode* scroll_offset;
    IntRect scroll_container_bounds;
    bool operator==(const ScrollHitTest& rhs) const {
      return scroll_offset == rhs.scroll_offset &&
             scroll_container_bounds == rhs.scroll_container_bounds;
    }
  };
  base::Optional<ScrollHitTest> scroll_hit_test;

  HitTestData() = default;
  HitTestData(const HitTestData& other)
      : touch_action_rects(other.touch_action_rects),
        scroll_hit_test(other.scroll_hit_test) {}

  bool operator==(const HitTestData& rhs) const {
    return touch_action_rects == rhs.touch_action_rects &&
           scroll_hit_test == rhs.scroll_hit_test;
  }

  void AppendTouchActionRect(const HitTestRect& rect) {
    touch_action_rects.push_back(rect);
  }

  void SetScrollHitTest(const TransformPaintPropertyNode* scroll_offset,
                        const IntRect& scroll_container_bounds) {
    DCHECK(!scroll_offset || scroll_offset->ScrollNode());
    scroll_hit_test = base::make_optional(
        ScrollHitTest{scroll_offset, scroll_container_bounds});
  }

  bool operator!=(const HitTestData& rhs) const { return !(*this == rhs); }

  String ToString() const;
};

PLATFORM_EXPORT std::ostream& operator<<(std::ostream&, const HitTestData&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_HIT_TEST_DATA_H_
