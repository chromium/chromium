// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"

#include "base/logging.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/paint_op_buffer_iterator.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/logging_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkData.h"
#include "ui/gfx/geometry/insets_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace blink {

namespace {

SkBitmap RecordToBitmap(const PaintRecord& record, const gfx::Rect& bounds) {
  SkBitmap bitmap;
  if (!bitmap.tryAllocPixels(
          SkImageInfo::MakeN32Premul(bounds.width(), bounds.height())))
    return bitmap;

  SkiaPaintCanvas canvas(bitmap);
  canvas.clear(SkColors::kTransparent);
  canvas.translate(-bounds.x(), -bounds.y());
  canvas.drawPicture(record);
  return bitmap;
}

bool BitmapsEqual(const PaintRecord& record1,
                  const PaintRecord& record2,
                  const gfx::Rect& bounds) {
  SkBitmap bitmap1 = RecordToBitmap(record1, bounds);
  SkBitmap bitmap2 = RecordToBitmap(record2, bounds);
  if (bitmap1.isNull() || bitmap2.isNull())
    return true;

  int mismatch_count = 0;
  constexpr int kMaxMismatches = 10;
  for (int y = 0; y < bounds.height(); ++y) {
    for (int x = 0; x < bounds.width(); ++x) {
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

bool PaintFlagsMayChangeColorOrMovePixelsExceptShader(
    const cc::PaintFlags& flags) {
  return flags.getStyle() != cc::PaintFlags::kFill_Style || flags.getLooper() ||
         flags.getColorFilter() || flags.getImageFilter() ||
         (flags.getBlendMode() != SkBlendMode::kSrc &&
          flags.getBlendMode() != SkBlendMode::kSrcOver);
}

bool IsDrawAreaAnalysisCandidate(const cc::PaintOp& op) {
  if (!op.IsPaintOpWithFlags()) {
    return false;
  }
  const auto& flags = static_cast<const cc::PaintOpWithFlags&>(op).flags;
  return !PaintFlagsMayChangeColorOrMovePixelsExceptShader(flags) &&
         !flags.getShader();
}

}  // anonymous namespace

bool DrawingDisplayItem::EqualsForUnderInvalidationImpl(
    const DrawingDisplayItem& other) const {
  DCHECK(RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled());

  auto bounds = VisualRect();
  const auto& other_bounds = other.VisualRect();
  if (bounds != other_bounds) {
    return false;
  }

  const auto& record = GetPaintRecord();
  const auto& other_record = other.GetPaintRecord();
  if (record.empty() && other_record.empty()) {
    return true;
  }
  // memcmp() may touch uninitialized gaps in PaintRecord, so skip this check
  // for MSAN.
#if !defined(MEMORY_SANITIZER)
  if (record.buffer().next_op_offset() ==
          other_record.buffer().next_op_offset() &&
      memcmp(&record.GetFirstOp(), &other_record.GetFirstOp(),
             record.buffer().next_op_offset()) == 0) {
    return true;
  }
#endif
  // By checking equality of bitmaps, different records for the same visual
  // result are also treated as equal.
  return BitmapsEqual(record, other_record, bounds);
}

DrawingDisplayItem::BackgroundColorInfo DrawingDisplayItem::BackgroundColor()
    const {
  DCHECK(!IsTombstone());

  if (record_.empty()) {
    return {};
  }

  bool may_be_solid_color = record_.size() == 1;
  for (const cc::PaintOp& op : record_) {
    if (!IsDrawAreaAnalysisCandidate(op)) {
      if (GetType() != DisplayItem::kBoxDecorationBackground &&
          GetType() != DisplayItem::kDocumentBackground &&
          GetType() != DisplayItem::kDocumentRootBackdrop &&
          GetType() != DisplayItem::kScrollCorner) {
        // Only analyze the first op for a display item not of the above types.
        return {};
      }
      continue;
    }
    SkRect item_rect;
    switch (op.GetType()) {
      case cc::PaintOpType::kDrawRect:
        item_rect = static_cast<const cc::DrawRectOp&>(op).rect;
        break;
      case cc::PaintOpType::kDrawIRect:
        item_rect = SkRect::Make(static_cast<const cc::DrawIRectOp&>(op).rect);
        break;
      case cc::PaintOpType::kDrawRRect:
        item_rect = static_cast<const cc::DrawRRectOp&>(op).rrect.rect();
        may_be_solid_color = false;
        break;
      default:
        return {};
    }
    return {static_cast<const cc::PaintOpWithFlags&>(op).flags.getColor4f(),
            item_rect.width() * item_rect.height(),
            may_be_solid_color &&
                item_rect.contains(gfx::RectToSkIRect(VisualRect()))};
  }
  return {};
}

gfx::Rect DrawingDisplayItem::CalculateRectKnownToBeOpaque() const {
  gfx::Rect rect = CalculateRectKnownToBeOpaqueForRecord(record_);
  if (rect.IsEmpty()) {
    SetOpaqueness(Opaqueness::kNone);
  } else if (rect == VisualRect()) {
    SetOpaqueness(Opaqueness::kFull);
  } else {
    DCHECK(VisualRect().Contains(rect));
    DCHECK_EQ(GetOpaqueness(), Opaqueness::kOther);
  }
  return rect;
}

// This is not a PaintRecord method because it's not a general opaqueness
// detection algorithm (which might be more complex and slower), but works well
// and fast for most blink painted results.
gfx::Rect DrawingDisplayItem::CalculateRectKnownToBeOpaqueForRecord(
    const PaintRecord& record) const {
  if (record.empty()) {
    return gfx::Rect();
  }

  // This limit keeps the algorithm fast, while allowing check of enough paint
  // operations for most blink painted results.
  constexpr wtf_size_t kOpCountLimit = 8;
  gfx::Rect opaque_rect;
  wtf_size_t op_count = 0;
  gfx::Rect clip_rect = VisualRect();
  for (const cc::PaintOp& op : record) {
    if (++op_count > kOpCountLimit)
      break;

    // Deal with the common pattern of clipped bleed avoiding images like:
    // kSave, kClipRect, kDraw..., kRestore.
    if (op.GetType() == cc::PaintOpType::kSave) {
      continue;
    }
    if (op.GetType() == cc::PaintOpType::kClipRect) {
      clip_rect.Intersect(gfx::ToEnclosedRect(
          gfx::SkRectToRectF(static_cast<const cc::ClipRectOp&>(op).rect)));
      continue;
    }

    if (!op.IsDrawOp())
      break;

    gfx::Rect op_opaque_rect;
    if (op.GetType() == cc::PaintOpType::kDrawRecord) {
      op_opaque_rect = CalculateRectKnownToBeOpaqueForRecord(
          static_cast<const cc::DrawRecordOp&>(op).record);
    } else {
      if (!op.IsPaintOpWithFlags())
        continue;

      const auto& flags = static_cast<const cc::PaintOpWithFlags&>(op).flags;
      if (PaintFlagsMayChangeColorOrMovePixelsExceptShader(flags) ||
          !flags.getColor4f().isOpaque() ||
          (flags.getShader() && !flags.getShader()->IsOpaque())) {
        continue;
      }

      switch (op.GetType()) {
        case cc::PaintOpType::kDrawRect:
          op_opaque_rect = gfx::ToEnclosedRect(
              gfx::SkRectToRectF(static_cast<const cc::DrawRectOp&>(op).rect));
          break;
        case cc::PaintOpType::kDrawRRect: {
          const SkRRect& rrect = static_cast<const cc::DrawRRectOp&>(op).rrect;
          SkVector top_left = rrect.radii(SkRRect::kUpperLeft_Corner);
          SkVector top_right = rrect.radii(SkRRect::kUpperRight_Corner);
          SkVector bottom_left = rrect.radii(SkRRect::kLowerLeft_Corner);
          SkVector bottom_right = rrect.radii(SkRRect::kLowerRight_Corner);
          // Get a bounding rect that does not intersect with the rounding clip.
          // When a rect has rounded corner with radius r, then the largest rect
          // that can be inscribed inside it has an inset of |((2 - sqrt(2)) /
          // 2) * radius|.
          gfx::RectF contained = gfx::SkRectToRectF(rrect.rect());
          contained.Inset(
              gfx::InsetsF()
                  .set_top(std::max(top_left.y(), top_right.y()) * 0.3f)
                  .set_right(std::max(top_right.x(), bottom_right.x()) * 0.3f)
                  .set_bottom(std::max(bottom_left.y(), bottom_right.y()) *
                              0.3f)
                  .set_left(std::max(top_left.x(), bottom_left.x()) * 0.3f));
          op_opaque_rect = ToEnclosedRect(contained);
          break;
        }
        case cc::PaintOpType::kDrawIRect:
          op_opaque_rect =
              gfx::SkIRectToRect(static_cast<const cc::DrawIRectOp&>(op).rect);
          break;
        case cc::PaintOpType::kDrawImage: {
          const auto& draw_image_op = static_cast<const cc::DrawImageOp&>(op);
          const auto& image = draw_image_op.image;
          if (!image.IsOpaque())
            continue;
          op_opaque_rect = gfx::Rect(draw_image_op.left, draw_image_op.top,
                                     image.width(), image.height());
          break;
        }
        case cc::PaintOpType::kDrawImageRect: {
          const auto& draw_image_rect_op =
              static_cast<const cc::DrawImageRectOp&>(op);
          const auto& image = draw_image_rect_op.image;
          DCHECK(gfx::RectF(image.width(), image.height())
                     .Contains(gfx::SkRectToRectF(draw_image_rect_op.src)));
          if (!image.IsOpaque())
            continue;
          op_opaque_rect =
              gfx::ToEnclosedRect(gfx::SkRectToRectF(draw_image_rect_op.dst));
          break;
        }
        default:
          continue;
      }
    }

    opaque_rect = gfx::MaximumCoveredRect(opaque_rect, op_opaque_rect);
    opaque_rect.Intersect(clip_rect);
    if (opaque_rect == VisualRect())
      break;
  }
  DCHECK(VisualRect().Contains(opaque_rect) || opaque_rect.IsEmpty());
  return opaque_rect;
}

gfx::Rect DrawingDisplayItem::TightenVisualRect(const gfx::Rect& visual_rect,
                                                const PaintRecord& record) {
  DCHECK(ShouldTightenVisualRect(record));

  const cc::PaintOp& op = record.GetFirstOp();
  if (!IsDrawAreaAnalysisCandidate(op)) {
    return visual_rect;
  }

  // TODO(pdr): Consider using |PaintOp::GetBounds| which is a more complete
  // implementation of the logic below.

  gfx::Rect item_rect;
  switch (op.GetType()) {
    case cc::PaintOpType::kDrawRect:
      item_rect = gfx::ToEnclosingRect(
          gfx::SkRectToRectF(static_cast<const cc::DrawRectOp&>(op).rect));
      break;
    case cc::PaintOpType::kDrawIRect:
      item_rect =
          gfx::SkIRectToRect(static_cast<const cc::DrawIRectOp&>(op).rect);
      break;
    case cc::PaintOpType::kDrawRRect:
      item_rect = gfx::ToEnclosingRect(gfx::SkRectToRectF(
          static_cast<const cc::DrawRRectOp&>(op).rrect.rect()));
      break;
    // TODO(pdr): Support image PaintOpTypes such as kDrawImage{rect}.
    // TODO(pdr): Consider checking PaintOpType::kDrawtextblob too.
    default:
      return visual_rect;
  }

  // TODO(pdr): Enable this DCHECK which enforces that the original visual rect
  // was correct and fully contains the recording.
  // DCHECK(visual_rect.Contains(item_rect));
  return item_rect;
}

}  // namespace blink
