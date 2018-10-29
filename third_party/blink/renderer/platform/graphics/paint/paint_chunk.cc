// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk.h"

#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

struct SameSizeAsPaintChunk {
  size_t begin;
  size_t end;
  PaintChunk::Id id;
  PropertyTreeState properties;
  unsigned bools;
  float extend;
  FloatRect bounds;
  void* pointers[1];
};

static_assert(sizeof(PaintChunk) == sizeof(SameSizeAsPaintChunk),
              "PaintChunk should stay small");

String PaintChunk::ToString() const {
  String ret_val = String::Format(
      "PaintChunk(begin=%zu, end=%zu, id=%s cacheable=%d props=(%s) bounds=%s "
      "known_to_be_opaque=%d",
      begin_index, end_index, id.ToString().Ascii().data(), is_cacheable,
      properties.ToString().Ascii().data(), bounds.ToString().Ascii().data(),
      known_to_be_opaque);
  if (hit_test_data) {
    ret_val.append(String::Format(
        ", touch_action_rects=(rects: %u, bounds: %s), "
        "wheel_event_handler_region=(rects: %u, bounds: %s), "
        "non_fast_scrollable_region=(rects: %u, bounds: %s))",
        hit_test_data->touch_action_rects.size(),
        HitTestRect::GetBounds(hit_test_data->touch_action_rects)
            .ToString()
            .Ascii()
            .data(),
        hit_test_data->wheel_event_handler_region.size(),
        HitTestRect::GetBounds(hit_test_data->wheel_event_handler_region)
            .ToString()
            .Ascii()
            .data(),
        hit_test_data->non_fast_scrollable_region.size(),
        HitTestRect::GetBounds(hit_test_data->non_fast_scrollable_region)
            .ToString()
            .Ascii()
            .data()));
  } else {
    ret_val.append(")");
  }
  return ret_val;
}

std::ostream& operator<<(std::ostream& os, const PaintChunk& chunk) {
  return os << chunk.ToString().Utf8().data();
}

}  // namespace blink
