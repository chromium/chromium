// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/hit_test_rect.h"

#include "base/containers/flat_map.h"
#include "cc/base/region.h"
#include "cc/layers/touch_action_region.h"

namespace blink {

// static
cc::TouchActionRegion HitTestRect::BuildRegion(
    const Vector<HitTestRect>& hit_test_rects) {
  base::flat_map<TouchAction, cc::Region> region_map;
  region_map.reserve(hit_test_rects.size());
  for (const HitTestRect& hit_test_rect : hit_test_rects) {
    const TouchAction& action = hit_test_rect.whitelisted_touch_action;
    const LayoutRect& rect = hit_test_rect.rect;
    region_map[action].Union(EnclosingIntRect(rect));
  }
  return cc::TouchActionRegion(std::move(region_map));
}

// static
LayoutRect HitTestRect::GetBounds(const Vector<HitTestRect>& hit_test_rects) {
  cc::Region region;
  for (const HitTestRect& hit_test_rect : hit_test_rects) {
    const LayoutRect& rect = hit_test_rect.rect;
    region.Union(EnclosingIntRect(rect));
  }
  const auto& rect = region.bounds();
  return LayoutRect(IntRect(rect));
}

}  // namespace blink
