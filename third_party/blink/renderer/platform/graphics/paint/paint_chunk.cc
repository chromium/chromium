// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk.h"

#include "base/memory/values_equivalent.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_artifact.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

struct SameSizeAsPaintChunk {
  wtf_size_t begin_index;
  wtf_size_t end_index;
  PaintChunk::Id id;
  PaintChunk::BackgroundColorInfo background_color;
  TraceablePropertyTreeState properties;
  Member<HitTestData> hit_test_data;
  Member<RegionCaptureData> region_capture_data;
  Member<LayerSelectionData> layer_selection;
  gfx::Rect bounds;
  gfx::Rect drawable_bounds;
  gfx::Rect rect_known_to_be_opaque;
  uint8_t raster_effect_outset;
  uint8_t hit_test_opaqueness;
  bool b;
};

ASSERT_SIZE(PaintChunk, SameSizeAsPaintChunk);

bool PaintChunk::EqualsForUnderInvalidationChecking(
    const PaintChunk& other) const {
  return size() == other.size() && id == other.id &&
         properties == other.properties && bounds == other.bounds &&
         base::ValuesEquivalent(hit_test_data, other.hit_test_data) &&
         base::ValuesEquivalent(region_capture_data,
                                other.region_capture_data) &&
         drawable_bounds == other.drawable_bounds &&
         raster_effect_outset == other.raster_effect_outset &&
         hit_test_opaqueness == other.hit_test_opaqueness &&
         effectively_invisible == other.effectively_invisible;
  // Derived fields like rect_known_to_be_opaque are not checked because they
  // are updated when we create the next chunk or release chunks. We ensure
  // their correctness with unit tests and under-invalidation checking of
  // display items.
}

size_t PaintChunk::MemoryUsageInBytes() const {
  size_t total_size = sizeof(*this);
  if (hit_test_data) {
    total_size += sizeof(*hit_test_data);
    total_size += hit_test_data->touch_action_rects.CapacityInBytes();
    total_size += hit_test_data->wheel_event_rects.CapacityInBytes();
  }
  if (region_capture_data) {
    total_size += sizeof(*region_capture_data);
  }
  if (layer_selection_data) {
    total_size += sizeof(*layer_selection_data);
  }
  return total_size;
}

static String ToStringImpl(const PaintChunk& c,
                           const String& id_string,
                           bool concise) {
  StringBuilder sb;
  sb.AppendFormat("PaintChunk(%u-%u id=%s cacheable=%d bounds=%s from_cache=%d",
                  c.begin_index, c.end_index, id_string.Utf8().c_str(),
                  c.is_cacheable, c.bounds.ToString().c_str(),
                  c.is_moved_from_cached_subsequence);
  if (!concise) {
    sb.AppendFormat(
        " props=(%s) rect_known_to_be_opaque=%s hit_test_opaqueness=%s "
        "effectively_invisible=%d drawscontent=%d",
        c.properties.ToString().Utf8().c_str(),
        c.rect_known_to_be_opaque.ToString().c_str(),
        cc::HitTestOpaquenessToString(c.hit_test_opaqueness),
        c.effectively_invisible, c.DrawsContent());
    if (c.hit_test_data) {
      sb.Append(" hit_test_data=");
      sb.Append(c.hit_test_data->ToString());
    }
    if (c.region_capture_data) {
      sb.Append(" region_capture_data=");
      sb.Append(c.region_capture_data->ToString());
    }
  }
  sb.Append(')');
  return sb.ToString();
}

String PaintChunk::ToString(bool concise) const {
  return ToStringImpl(*this, id.ToString(), concise);
}

String PaintChunk::ToString(const PaintArtifact& paint_artifact,
                            bool concise) const {
  return ToStringImpl(*this, id.ToString(paint_artifact), concise);
}

std::ostream& operator<<(std::ostream& os, const PaintChunk& chunk) {
  return os << chunk.ToString().Utf8();
}

}  // namespace blink
