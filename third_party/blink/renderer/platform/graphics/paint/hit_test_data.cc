// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/hit_test_data.h"

#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
template <typename T>
static String RectsAsString(const Vector<T>& rects) {
  StringBuilder sb;
  sb.Append("[");
  bool first = true;
  for (const auto& rect : rects) {
    if (!first)
      sb.Append(", ");
    first = false;
    sb.Append("(");
    sb.Append(String(rect.ToString()));
    sb.Append(")");
  }
  sb.Append("]");
  return sb.ToString();
}

#if BUILDFLAG(IS_ANDROID)
static String XrRegionsAsString(const Vector<CompositorElementId>& xr_regions) {
  StringBuilder sb;
  sb.Append("[");
  bool first = true;
  for (const auto& id : xr_regions) {
    if (!first) {
      sb.Append(", ");
    }
    first = false;
    sb.Append("(");
    sb.Append(String(id.ToString()));
    sb.Append(")");
  }
  sb.Append("]");
  return sb.ToString();
}
#endif

String HitTestData::ToString() const {
  StringBuilder sb;
  sb.Append("{");

  bool printed_top_level_field = false;
  auto append_field = [&](const char* name, const String& value) {
    if (!printed_top_level_field) {
      printed_top_level_field = true;
    } else {
      sb.Append(", ");
    }
    sb.Append(name);
    sb.Append(value);
  };

  if (!touch_action_rects.empty()) {
    append_field("touch_action_rects: ",
                 RectsAsString<TouchActionRect>(touch_action_rects));
  }

  if (!wheel_event_rects.empty()) {
    append_field("wheel_event_rects: ",
                 RectsAsString<gfx::Rect>(wheel_event_rects));
  }

#if BUILDFLAG(IS_ANDROID)
  // NOTE: We may not want to dump xr_regions here, since this method
  // is used primarily in tests and this will require different
  // test code per-platform.
  if (!xr_regions.empty()) {
    append_field("xr_regions: ", XrRegionsAsString(xr_regions));
  }
#endif

  if (!scroll_hit_test_rect.IsEmpty()) {
    append_field("scroll_hit_test_rect: ",
                 String(scroll_hit_test_rect.ToString()));
  }

  if (scroll_translation) {
    append_field("scroll_translation: ",
                 String::Format("%p", scroll_translation.Get()));
    if (scrolling_contents_cull_rect != InfiniteIntRect()) {
      append_field("scrolling_contents_cull_rect: ",
                   String(scrolling_contents_cull_rect.ToString()));
    }
  }

  sb.Append("}");
  return sb.ToString();
}

std::ostream& operator<<(std::ostream& os, const HitTestData& data) {
  return os << data.ToString().Utf8();
}

std::ostream& operator<<(std::ostream& os, const HitTestData* data) {
  return os << (data ? data->ToString().Utf8() : "null");
}

}  // namespace blink
