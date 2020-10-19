// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk.h"

#include "third_party/blink/renderer/platform/wtf/size_assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

struct SameSizeAsPaintChunk {
  wtf_size_t begin_index;
  wtf_size_t end_index;
  Color background_color;
  uint64_t background_color_area;
  PaintChunk::Id id;
  PropertyTreeState properties;
  IntRect bounds;
  IntRect drawable_bounds;
  void* hit_test_data;
  bool b[2];
};

ASSERT_SIZE(PaintChunk, SameSizeAsPaintChunk);

bool PaintChunk::EqualsForUnderInvalidationChecking(
    const PaintChunk& other) const {
  return size() == other.size() && id == other.id &&
         properties == other.properties && bounds == other.bounds &&
         drawable_bounds == other.drawable_bounds &&
         raster_effect_outset == other.raster_effect_outset &&
         ((!hit_test_data && !other.hit_test_data) ||
          (hit_test_data && other.hit_test_data &&
           *hit_test_data == *other.hit_test_data));
  // known_to_be_opaque is not checked ]because it's updated when we create the
  // next chunk or release chunks. We ensure its correctness with unit tests and
  // under-invalidation checking of display items.
}

size_t PaintChunk::MemoryUsageInBytes() const {
  size_t total_size = sizeof(*this);
  if (hit_test_data) {
    total_size += sizeof(*hit_test_data);
    total_size +=
        hit_test_data->touch_action_rects.capacity() * sizeof(TouchActionRect);
    total_size += hit_test_data->wheel_event_rects.capacity() * sizeof(IntRect);
  }
  return total_size;
}

String PaintChunk::ToString() const {
  StringBuilder sb;
  sb.AppendFormat(
      "PaintChunk(begin=%u, end=%u, id=%s cacheable=%d props=(%s) bounds=%s "
      "known_to_be_opaque=%d",
      begin_index, end_index, id.ToString().Utf8().c_str(), is_cacheable,
      properties.ToString().Utf8().c_str(), bounds.ToString().Utf8().c_str(),
      known_to_be_opaque);
  if (hit_test_data) {
    sb.Append(", hit_test_data=");
    sb.Append(hit_test_data->ToString());
  }
  sb.Append(')');
  return sb.ToString();
}

std::ostream& operator<<(std::ostream& os, const PaintChunk& chunk) {
  return os << chunk.ToString().Utf8() << "\n";
}

}  // namespace blink
