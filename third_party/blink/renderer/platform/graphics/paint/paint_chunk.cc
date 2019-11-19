// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk.h"

#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

struct SameSizeAsPaintChunk {
  size_t begin_index;
  size_t end_index;
  PaintChunk::Id id;
  PropertyTreeState properties;
  FloatRect bounds;
  float outset_for_raster_effects;
  SkColor safe_opaque_background_color;
  unsigned bools;  // known_to_be_opaque, is_cacheable, client_is_just_created
  void* pointers[1];  // hit_test_data
};

static_assert(sizeof(PaintChunk) == sizeof(SameSizeAsPaintChunk),
              "PaintChunk should stay small");

String PaintChunk::ToString() const {
  StringBuilder sb;
  sb.AppendFormat(
      "PaintChunk(begin=%zu, end=%zu, id=%s cacheable=%d props=(%s) bounds=%s "
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
