// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/graphics/paint/raster_invalidation_tracking.h"

#include <algorithm>

#include "base/logging.h"
#include "cc/layers/layer.h"
#include "third_party/blink/renderer/platform/geometry/geometry_as_json.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_recorder.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/skia/include/core/SkImageFilter.h"

namespace blink {

static bool g_simulate_raster_under_invalidations = false;

void RasterInvalidationTracking::SimulateRasterUnderInvalidations(bool enable) {
  g_simulate_raster_under_invalidations = enable;
}

bool RasterInvalidationTracking::ShouldAlwaysTrack() {
  return RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled() ||
         IsTracingRasterInvalidations();
}

bool RasterInvalidationTracking::IsTracingRasterInvalidations() {
  bool tracing_enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      TRACE_DISABLED_BY_DEFAULT("blink.invalidation"), &tracing_enabled);
  return tracing_enabled;
}

void RasterInvalidationTracking::AddInvalidation(
    DisplayItemClientId client_id,
    const String& debug_name,
    const gfx::Rect& rect,
    PaintInvalidationReason reason) {
  if (rect.IsEmpty())
    return;

  RasterInvalidationInfo info;
  info.client_id = client_id;
  info.client_debug_name = debug_name;
  info.rect = rect;
  info.reason = reason;
  invalidations_.push_back(info);

  // TODO(crbug.com/496260): Some antialiasing effects overflow the paint
  // invalidation rect.
  gfx::Rect r = rect;
  r.Outset(1);
  invalidation_region_since_last_paint_.Union(r);
}

static bool CompareRasterInvalidationInfo(const RasterInvalidationInfo& a,
                                          const RasterInvalidationInfo& b) {
  // Sort by rect first, bigger rects before smaller ones.
  if (a.rect.width() != b.rect.width())
    return a.rect.width() > b.rect.width();
  if (a.rect.height() != b.rect.height())
    return a.rect.height() > b.rect.height();
  if (a.rect.x() != b.rect.x())
    return a.rect.x() > b.rect.x();
  if (a.rect.y() != b.rect.y())
    return a.rect.y() > b.rect.y();

  // Then compare clientDebugName, in alphabetic order.
  int name_compare_result =
      CodeUnitCompare(a.client_debug_name, b.client_debug_name);
  if (name_compare_result != 0)
    return name_compare_result < 0;

  return a.reason < b.reason;
}

void RasterInvalidationTracking::AsJSON(JSONObject* json, bool detailed) const {
  if (!invalidations_.empty()) {
    // Sort to make the output more readable and easier to see the differences
    // by a human.
    auto sorted = invalidations_;
    std::sort(sorted.begin(), sorted.end(), &CompareRasterInvalidationInfo);
    auto invalidations_json = std::make_unique<JSONArray>();
    gfx::Rect last_rect;
    for (auto it = sorted.begin(); it != sorted.end(); ++it) {
      const auto& info = *it;
      if (detailed) {
        auto info_json = std::make_unique<JSONObject>();
        info_json->SetArray("rect", RectAsJSONArray(info.rect));
        info_json->SetString("object", info.client_debug_name);
        info_json->SetString("reason",
                             PaintInvalidationReasonToString(info.reason));
        invalidations_json->PushObject(std::move(info_json));
      } else if (std::none_of(sorted.begin(), it, [&info](auto& previous) {
                   return previous.rect.Contains(info.rect);
                 })) {
        invalidations_json->PushArray(RectAsJSONArray(info.rect));
        last_rect = info.rect;
      }
    }
    json->SetArray("invalidations", std::move(invalidations_json));
  }

  if (!under_invalidations_.empty()) {
    auto under_invalidations_json = std::make_unique<JSONArray>();
    for (auto& under_invalidation : under_invalidations_) {
      auto under_invalidation_json = std::make_unique<JSONObject>();
      under_invalidation_json->SetDouble("x", under_invalidation.x);
      under_invalidation_json->SetDouble("y", under_invalidation.y);
      // TODO(https://crbug.com/1351544): This should use SkColor4f.
      under_invalidation_json->SetString(
          "oldPixel", Color::FromSkColor(under_invalidation.old_pixel)
                          .NameForLayoutTreeAsText());
      under_invalidation_json->SetString(
          "newPixel", Color::FromSkColor(under_invalidation.new_pixel)
                          .NameForLayoutTreeAsText());
      under_invalidations_json->PushObject(std::move(under_invalidation_json));
    }
    json->SetArray("underInvalidations", std::move(under_invalidations_json));
  }
}

void RasterInvalidationTracking::AddToLayerDebugInfo(
    cc::LayerDebugInfo& debug_info) const {
  // This is not sorted because the output is for client programs, and the
  // invalidations may be accumulated in debug_info.
  for (auto& info : invalidations_) {
    if (info.rect.IsEmpty())
      continue;
    debug_info.invalidations.push_back(
        {gfx::Rect(info.rect), PaintInvalidationReasonToString(info.reason),
         info.client_debug_name.Utf8()});
  }
}

static bool PixelComponentsDiffer(int c1, int c2) {
  // Compare strictly for saturated values.
  if (c1 == 0 || c1 == 255 || c2 == 0 || c2 == 255)
    return c1 != c2;
  // Tolerate invisible differences that may occur in gradients etc.
  return abs(c1 - c2) > 2;
}

static bool PixelsDiffer(SkColor p1, SkColor p2) {
  return PixelComponentsDiffer(SkColorGetA(p1), SkColorGetA(p2)) ||
         PixelComponentsDiffer(SkColorGetR(p1), SkColorGetR(p2)) ||
         PixelComponentsDiffer(SkColorGetG(p1), SkColorGetG(p2)) ||
         PixelComponentsDiffer(SkColorGetB(p1), SkColorGetB(p2));
}

void RasterInvalidationTracking::CheckUnderInvalidations(
    const String& layer_debug_name,
    PaintRecord new_record,
    const gfx::Rect& new_interest_rect) {
  auto old_interest_rect = last_interest_rect_;
  cc::Region invalidation_region;
  if (!g_simulate_raster_under_invalidations)
    invalidation_region = invalidation_region_since_last_paint_;
  std::optional<PaintRecord> old_record = std::move(last_painted_record_);

  last_painted_record_ = new_record;
  last_interest_rect_ = new_interest_rect;
  invalidation_region_since_last_paint_ = cc::Region();

  if (!old_record)
    return;

  gfx::Rect rect = gfx::IntersectRects(old_interest_rect, new_interest_rect);
  if (rect.IsEmpty())
    return;

  SkBitmap old_bitmap;
  if (!old_bitmap.tryAllocPixels(
          SkImageInfo::MakeN32Premul(rect.width(), rect.height())))
    return;
  {
    SkiaPaintCanvas canvas(old_bitmap);
    canvas.clear(SkColors::kTransparent);
    canvas.translate(-rect.x(), -rect.y());
    canvas.drawPicture(std::move(*old_record));
  }

  SkBitmap new_bitmap;
  if (!new_bitmap.tryAllocPixels(
          SkImageInfo::MakeN32Premul(rect.width(), rect.height())))
    return;
  {
    SkiaPaintCanvas canvas(new_bitmap);
    canvas.clear(SkColors::kTransparent);
    canvas.translate(-rect.x(), -rect.y());
    canvas.drawPicture(std::move(new_record));
  }

  int mismatching_pixels = 0;
  static const int kMaxMismatchesToReport = 50;
  for (int bitmap_y = 0; bitmap_y < rect.height(); ++bitmap_y) {
    // In the common case of no under-invalidation, memcmp/memset is much faster
    // than the pixel-by-pixel comparison below.
    void* new_row_addr = new_bitmap.pixmap().writable_addr(0, bitmap_y);
    if (memcmp(old_bitmap.pixmap().addr(0, bitmap_y), new_row_addr,
               new_bitmap.rowBytes()) == 0) {
      memset(new_row_addr, 0, new_bitmap.rowBytes());
      continue;
    }

    int layer_y = bitmap_y + rect.y();
    for (int bitmap_x = 0; bitmap_x < rect.width(); ++bitmap_x) {
      int layer_x = bitmap_x + rect.x();
      SkColor old_pixel = old_bitmap.getColor(bitmap_x, bitmap_y);
      SkColor new_pixel = new_bitmap.getColor(bitmap_x, bitmap_y);
      if (PixelsDiffer(old_pixel, new_pixel) &&
          !invalidation_region.Contains(gfx::Point(layer_x, layer_y))) {
        if (mismatching_pixels < kMaxMismatchesToReport) {
          RasterUnderInvalidation under_invalidation = {layer_x, layer_y,
                                                        old_pixel, new_pixel};
          under_invalidations_.push_back(under_invalidation);
          LOG(ERROR) << layer_debug_name
                     << " Uninvalidated old/new pixels mismatch at " << layer_x
                     << "," << layer_y << " old:" << std::hex << old_pixel
                     << " new:" << new_pixel;
        } else if (mismatching_pixels == kMaxMismatchesToReport) {
          LOG(ERROR) << "and more...";
        }
        ++mismatching_pixels;
        *new_bitmap.getAddr32(bitmap_x, bitmap_y) =
            SkColorSetARGB(0xFF, 0xA0, 0, 0);  // Dark red.
      } else {
        *new_bitmap.getAddr32(bitmap_x, bitmap_y) = SK_ColorTRANSPARENT;
      }
    }
  }

  if (!mismatching_pixels)
    return;

  PaintRecorder recorder;
  recorder.beginRecording();
  auto* canvas = recorder.getRecordingCanvas();
  canvas->drawPicture(std::move(under_invalidation_record_));
  canvas->drawImage(cc::PaintImage::CreateFromBitmap(std::move(new_bitmap)),
                    rect.x(), rect.y());
  under_invalidation_record_ = recorder.finishRecordingAsPicture();
}

}  // namespace blink
