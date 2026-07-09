// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/tracked_element_data.h"

#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

String TrackedElementRect::ToString() const {
  StringBuilder sb;
  sb.Append("{ id: ");
  sb.Append(id.value().ToString().c_str());
  sb.Append(", bounds: ");
  sb.Append(bounds.ToString().c_str());
  sb.Append(", should_exclude_fixed_and_sticky_occlusions: ");
  sb.Append(should_exclude_fixed_and_sticky_occlusions ? "true" : "false");
  if (frame_token.has_value()) {
    sb.Append(", frame_token: ");
    sb.Append(frame_token->ToString().c_str());
  }
  if (parent_frame_token.has_value()) {
    sb.Append(", parent_frame_token: ");
    sb.Append(parent_frame_token->ToString().c_str());
  }
  sb.Append(" }");
  return sb.ToString();
}

String TrackedElementRects::ToString() const {
  StringBuilder sb;
  sb.Append("{");
  for (auto it = map.begin(); it != map.end(); ++it) {
    if (it != map.begin()) {
      sb.Append(", ");
    }
    sb.Append("{");
    sb.Append("feature: ");
    sb.Append(static_cast<int32_t>(it->first));
    auto vec = it->second;
    sb.Append(", rects: [");
    for (const auto& rect : vec) {
      if (&rect != &*vec.begin()) {
        sb.Append(", ");
      }
      sb.Append(rect.ToString());
    }
    sb.Append("]");
    sb.Append("}");
  }
  sb.Append("}");
  return sb.ToString();
}

gfx::Rect TrackedElementSubRect::GetEffectiveBounds(
    const gfx::Rect& element_paint_rect) const {
  if (!sub_rect) {
    return element_paint_rect;
  }
  gfx::Rect rect(
      element_paint_rect.origin() + sub_rect->rect.OffsetFromOrigin(),
      sub_rect->rect.size());
  if (sub_rect->type ==
      TrackedElementSubRect::SubRect::Type::kIntersectWithElementRect) {
    rect.Intersect(element_paint_rect);
  }
  return rect;
}

}  // namespace blink
