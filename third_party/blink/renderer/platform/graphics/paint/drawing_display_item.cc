// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"

#include "cc/paint/display_item_list.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkData.h"

namespace blink {

#if DCHECK_IS_ON()
void DrawingDisplayItem::PropertiesAsJSON(JSONObject& json) const {
  DisplayItem::PropertiesAsJSON(json);
  json.SetBoolean("opaque", known_to_be_opaque_);
}
#endif

static SkBitmap RecordToBitmap(sk_sp<const PaintRecord> record,
                               const IntRect& bounds) {
  SkBitmap bitmap;
  bitmap.allocPixels(
      SkImageInfo::MakeN32Premul(bounds.Width(), bounds.Height()));
  SkiaPaintCanvas canvas(bitmap);
  canvas.clear(SK_ColorTRANSPARENT);
  canvas.translate(-bounds.X(), -bounds.Y());
  canvas.drawPicture(std::move(record));
  return bitmap;
}

static bool BitmapsEqual(sk_sp<const PaintRecord> record1,
                         sk_sp<const PaintRecord> record2,
                         const IntRect& bounds) {
  SkBitmap bitmap1 = RecordToBitmap(record1, bounds);
  SkBitmap bitmap2 = RecordToBitmap(record2, bounds);
  int mismatch_count = 0;
  constexpr int kMaxMismatches = 10;
  for (int y = 0; y < bounds.Height(); ++y) {
    for (int x = 0; x < bounds.Width(); ++x) {
      SkColor pixel1 = bitmap1.getColor(x, y);
      SkColor pixel2 = bitmap2.getColor(x, y);
      if (pixel1 != pixel2) {
        if (!RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled())
          return false;
        LOG(ERROR) << "x=" << x << " y=" << y << " " << std::hex << pixel1
                   << " vs " << std::hex << pixel2;
        if (++mismatch_count >= kMaxMismatches)
          return false;
      }
    }
  }
  return !mismatch_count;
}

bool DrawingDisplayItem::Equals(const DisplayItem& other) const {
  if (!DisplayItem::Equals(other))
    return false;

  const auto& record = GetPaintRecord();
  const auto& other_record =
      static_cast<const DrawingDisplayItem&>(other).GetPaintRecord();
  if (!record && !other_record)
    return true;
  if (!record || !other_record)
    return false;

  auto bounds = this->VisualRect();
  const auto& other_bounds = other.VisualRect();
  if (bounds != other_bounds)
    return false;

  if (*record == *other_record)
    return true;

  // Sometimes the client may produce different records for the same visual
  // result, which should be treated as equal.
  // Limit the bounds to prevent OOM.
  bounds.Intersect(IntRect(bounds.X(), bounds.Y(), 6000, 6000));
  return BitmapsEqual(std::move(record), std::move(other_record), bounds);
}

}  // namespace blink
