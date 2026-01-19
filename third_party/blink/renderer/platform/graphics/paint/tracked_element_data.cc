// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/tracked_element_data.h"

#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

String TrackedElementData::ToString() const {
  StringBuilder sb;
  sb.Append("{");
  for (auto it = map.begin(); it != map.end(); ++it) {
    if (it != map.begin()) {
      sb.Append(", ");
    }
    sb.Append("{");
    sb.Append(it->first->ToString().c_str());
    sb.Append(": ");
    sb.Append(it->second.ToString().c_str());
    sb.Append("}");
  }
  sb.Append("}");
  return sb.ToString();
}

gfx::Rect TrackedElementRect::GetEffectiveBounds(
    const gfx::Rect& element_paint_rect) const {
  if (!sub_rect) {
    return element_paint_rect;
  }
  gfx::Rect rect(
      element_paint_rect.origin() + sub_rect->rect.OffsetFromOrigin(),
      sub_rect->rect.size());
  if (sub_rect->type ==
      TrackedElementRect::SubRect::Type::kIntersectWithElementRect) {
    rect.Intersect(element_paint_rect);
  }
  return rect;
}

}  // namespace blink
