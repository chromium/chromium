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

#include "third_party/blink/renderer/platform/graphics/logging_canvas.h"

namespace blink {

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

SkColor DrawingDisplayItem::BackgroundColor(float& area) const {
  if (GetType() != DisplayItem::kBoxDecorationBackground &&
      GetType() != DisplayItem::kDocumentBackground &&
      GetType() != DisplayItem::kDocumentRootBackdrop)
    return SK_ColorTRANSPARENT;

  if (!record_)
    return SK_ColorTRANSPARENT;

  for (cc::PaintOpBuffer::Iterator it(record_.get()); it; ++it) {
    const auto* op = *it;
    if (!op->IsPaintOpWithFlags())
      continue;
    const auto& flags = static_cast<const cc::PaintOpWithFlags*>(op)->flags;
    // Skip op with looper or shader which may modify the color.
    if (flags.getLooper() || flags.getShader() ||
        flags.getStyle() != cc::PaintFlags::kFill_Style) {
      continue;
    }
    SkRect item_rect;
    switch (op->GetType()) {
      case cc::PaintOpType::DrawRect:
        item_rect = static_cast<const cc::DrawRectOp*>(op)->rect;
        break;
      case cc::PaintOpType::DrawIRect:
        item_rect = SkRect::Make(static_cast<const cc::DrawIRectOp*>(op)->rect);
        break;
      case cc::PaintOpType::DrawRRect:
        item_rect = static_cast<const cc::DrawRRectOp*>(op)->rrect.rect();
        break;
      default:
        continue;
    }
    area = item_rect.width() * item_rect.height();
    return flags.getColor();
  }
  return SK_ColorTRANSPARENT;
}

// This is not a PaintRecord method because it's not a general opaqueness
// detection algorithm (which might be more complex and slower), but works well
// and fast for most blink painted results.
bool DrawingDisplayItem::CalculateKnownToBeOpaque(
    const PaintRecord* record) const {
  if (!record)
    return false;

  // This limit keeps the algorithm fast, while allowing check of enough paint
  // operations for most blink painted results.
  constexpr wtf_size_t kOpCountLimit = 4;
  wtf_size_t op_count = 0;
  for (cc::PaintOpBuffer::Iterator it(record); it; ++it) {
    if (++op_count > kOpCountLimit)
      return false;

    const auto* op = *it;
    // Deal with the common pattern of clipped bleed avoiding images like:
    // Save, ClipRect, Draw..., Restore.
    if (op->GetType() == cc::PaintOpType::Save)
      continue;
    if (op->GetType() == cc::PaintOpType::ClipRect) {
      const auto* clip_rect_op = static_cast<const cc::ClipRectOp*>(op);
      if (!EnclosedIntRect(clip_rect_op->rect).Contains(VisualRect()))
        return false;
      continue;
    }

    if (!op->IsDrawOp())
      return false;

    if (op->GetType() == cc::PaintOpType::DrawRecord) {
      return CalculateKnownToBeOpaque(
          static_cast<const cc::DrawRecordOp*>(op)->record.get());
    }

    if (!op->IsPaintOpWithFlags())
      continue;

    const auto& flags = static_cast<const cc::PaintOpWithFlags*>(op)->flags;
    if (flags.getStyle() != cc::PaintFlags::kFill_Style || flags.getLooper() ||
        (flags.getBlendMode() != SkBlendMode::kSrc &&
         flags.getBlendMode() != SkBlendMode::kSrcOver) ||
        flags.getMaskFilter() || flags.getColorFilter() ||
        flags.getImageFilter() || flags.getAlpha() != SK_AlphaOPAQUE ||
        (flags.getShader() && !flags.getShader()->IsOpaque()))
      continue;

    IntRect opaque_rect;
    switch (op->GetType()) {
      case cc::PaintOpType::DrawRect:
        opaque_rect =
            EnclosedIntRect(static_cast<const cc::DrawRectOp*>(op)->rect);
        break;
      case cc::PaintOpType::DrawIRect:
        opaque_rect = IntRect(static_cast<const cc::DrawIRectOp*>(op)->rect);
        break;
      case cc::PaintOpType::DrawImage: {
        const auto* draw_image_op = static_cast<const cc::DrawImageOp*>(op);
        const auto& image = draw_image_op->image;
        if (!image.IsOpaque())
          continue;
        opaque_rect = IntRect(draw_image_op->left, draw_image_op->top,
                              image.width(), image.height());
        break;
      }
      case cc::PaintOpType::DrawImageRect: {
        const auto* draw_image_rect_op =
            static_cast<const cc::DrawImageRectOp*>(op);
        const auto& image = draw_image_rect_op->image;
        DCHECK(SkRect::MakeWH(image.width(), image.height())
                   .contains(draw_image_rect_op->src));
        if (!image.IsOpaque())
          continue;
        opaque_rect = EnclosedIntRect(draw_image_rect_op->dst);
        break;
      }
      default:
        continue;
    }

    // We should never paint outside of the visual rect.
    if (opaque_rect.Contains(VisualRect()))
      return true;
  }
  return false;
}

}  // namespace blink
