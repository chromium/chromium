// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/hit_test_rect.h"

#include "cc/base/region.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

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

String HitTestRect::ToString() const {
  // TODO(pdr): Print the value of |allowed_touch_action|.
  return rect.ToString();
}

std::ostream& operator<<(std::ostream& os, const HitTestRect& hit_test_rect) {
  return os << hit_test_rect.ToString();
}

}  // namespace blink
