// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/replaced_painter.h"

#include "base/optional.h"
#include "third_party/blink/renderer/core/layout/layout_replaced.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"
#include "third_party/blink/renderer/core/paint/box_painter.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/object_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/scoped_paint_state.h"
#include "third_party/blink/renderer/core/paint/scrollable_area_painter.h"
#include "third_party/blink/renderer/core/paint/selection_painting_utils.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item_cache_skipper.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_paint_chunk_properties.h"

namespace blink {

namespace {

// Adjusts cull rect and paint chunk properties of the input ScopedPaintState
// for ReplacedContentTransform if needed.
class ScopedReplacedContentPaintState : public ScopedPaintState {
 public:
  ScopedReplacedContentPaintState(const ScopedPaintState& input,
                                  const LayoutReplaced& replaced);
};

ScopedReplacedContentPaintState::ScopedReplacedContentPaintState(
    const ScopedPaintState& input,
    const LayoutReplaced& replaced)
    : ScopedPaintState(input) {
  if (!fragment_to_paint_)
    return;

  const auto* paint_properties = fragment_to_paint_->PaintProperties();
  if (!paint_properties)
    return;

  PropertyTreeState new_properties =
      input_paint_info_.context.GetPaintController()
          .CurrentPaintChunkProperties();
  bool property_changed = false;

  const auto* content_transform = paint_properties->ReplacedContentTransform();
  if (content_transform && replaced.IsSVGRoot()) {
    new_properties.SetTransform(content_transform);
    adjusted_paint_info_.emplace(input_paint_info_);
    DCHECK(content_transform->Matrix().IsAffine());
    adjusted_paint_info_->UpdateCullRect(
        content_transform->Matrix().ToAffineTransform());
    property_changed = true;
  }

  bool painter_implements_content_box_clip = replaced.IsLayoutImage();
  if (paint_properties->OverflowClip() &&
      (!painter_implements_content_box_clip ||
       replaced.StyleRef().HasBorderRadius())) {
    new_properties.SetClip(paint_properties->OverflowClip());
    property_changed = true;
  }

  if (property_changed) {
    chunk_properties_.emplace(input_paint_info_.context.GetPaintController(),
                              new_properties, replaced,
                              input_paint_info_.DisplayItemTypeForClipping());
  }
}

}  // anonymous namespace

void ReplacedPainter::Paint(const PaintInfo& paint_info) {
  // TODO(crbug.com/797779): For now embedded contents don't know whether
  // they are painted in a fragmented context and may do something bad in a
  // fragmented context, e.g. creating subsequences. Skip cache to avoid that.
  // This will be unnecessary when the contents are fragment aware.
  base::Optional<DisplayItemCacheSkipper> cache_skipper;
  if (layout_replaced_.IsLayoutEmbeddedContent()) {
    DCHECK(layout_replaced_.HasLayer());
    if (layout_replaced_.Layer()->EnclosingPaginationLayer())
      cache_skipper.emplace(paint_info.context);
  }

  ScopedPaintState paint_state(layout_replaced_, paint_info);
  if (!ShouldPaint(paint_state))
    return;

  const auto& local_paint_info = paint_state.GetPaintInfo();
  auto paint_offset = paint_state.PaintOffset();
  LayoutRect border_rect(paint_offset, layout_replaced_.Size());

  if (ShouldPaintSelfBlockBackground(local_paint_info.phase)) {
    if (layout_replaced_.StyleRef().Visibility() == EVisibility::kVisible &&
        layout_replaced_.HasBoxDecorationBackground()) {
      if (layout_replaced_.HasLayer() &&
          layout_replaced_.Layer()->GetCompositingState() ==
              kPaintsIntoOwnBacking &&
          layout_replaced_.Layer()
              ->GetCompositedLayerMapping()
              ->DrawsBackgroundOntoContentLayer())
        return;

      BoxPainter(layout_replaced_)
          .PaintBoxDecorationBackground(local_paint_info, paint_offset);
    }
    // We're done. We don't bother painting any children.
    if (local_paint_info.phase == PaintPhase::kSelfBlockBackgroundOnly)
      return;
  }

  if (local_paint_info.phase == PaintPhase::kMask) {
    BoxPainter(layout_replaced_).PaintMask(local_paint_info, paint_offset);
    return;
  }

  if (ShouldPaintSelfOutline(local_paint_info.phase)) {
    ObjectPainter(layout_replaced_)
        .PaintOutline(local_paint_info, paint_offset);
    return;
  }

  if (local_paint_info.phase != PaintPhase::kForeground &&
      local_paint_info.phase != PaintPhase::kSelection &&
      !layout_replaced_.CanHaveChildren())
    return;

  if (local_paint_info.phase == PaintPhase::kSelection &&
      !layout_replaced_.IsSelected())
    return;

  bool skip_clip = layout_replaced_.IsSVGRoot() &&
                   !ToLayoutSVGRoot(layout_replaced_).ShouldApplyViewportClip();
  if (skip_clip || !layout_replaced_.PhysicalContentBoxRect().IsEmpty()) {
    ScopedReplacedContentPaintState content_paint_state(paint_state,
                                                        layout_replaced_);
    if (RuntimeEnabledFeatures::PaintTouchActionRectsEnabled()) {
      RecordHitTestData(content_paint_state.GetPaintInfo(),
                        content_paint_state.PaintOffset());
    }
    layout_replaced_.PaintReplaced(content_paint_state.GetPaintInfo(),
                                   content_paint_state.PaintOffset());
  }

  if (layout_replaced_.CanResize()) {
    ScrollableAreaPainter(*layout_replaced_.Layer()->GetScrollableArea())
        .PaintResizer(local_paint_info.context, RoundedIntPoint(paint_offset),
                      local_paint_info.GetCullRect());
  }

  // The selection tint never gets clipped by border-radius rounding, since we
  // want it to run right up to the edges of surrounding content.
  bool draw_selection_tint =
      local_paint_info.phase == PaintPhase::kForeground &&
      layout_replaced_.IsSelected() && layout_replaced_.CanBeSelectionLeaf() &&
      !local_paint_info.IsPrinting();
  if (draw_selection_tint && !DrawingRecorder::UseCachedDrawingIfPossible(
                                 local_paint_info.context, layout_replaced_,
                                 DisplayItem::kSelectionTint)) {
    LayoutRect selection_painting_rect = layout_replaced_.LocalSelectionRect();
    selection_painting_rect.MoveBy(paint_offset);
    IntRect selection_painting_int_rect =
        PixelSnappedIntRect(selection_painting_rect);

    DrawingRecorder recorder(local_paint_info.context, layout_replaced_,
                             DisplayItem::kSelectionTint);
    Color selection_bg = SelectionPaintingUtils::SelectionBackgroundColor(
        layout_replaced_.GetDocument(), layout_replaced_.StyleRef(),
        layout_replaced_.GetNode());
    local_paint_info.context.FillRect(selection_painting_int_rect,
                                      selection_bg);
  }
}

void ReplacedPainter::RecordHitTestData(const PaintInfo& paint_info,
                                        const LayoutPoint& paint_offset) {
  // Hit test display items are only needed for compositing. This flag is used
  // for for printing and drag images which do not need hit testing.
  if (paint_info.GetGlobalPaintFlags() & kGlobalPaintFlattenCompositingLayers)
    return;

  if (paint_info.phase != PaintPhase::kForeground)
    return;

  // If an object is not visible, it does not participate in hit testing.
  if (layout_replaced_.StyleRef().Visibility() != EVisibility::kVisible)
    return;

  auto touch_action = layout_replaced_.EffectiveWhitelistedTouchAction();
  if (touch_action == TouchAction::kTouchActionAuto)
    return;

  auto rect = layout_replaced_.VisualOverflowRect();
  rect.MoveBy(paint_offset);
  HitTestData::RecordHitTestRect(paint_info.context, layout_replaced_,
                                 HitTestRect(rect, touch_action));
}

bool ReplacedPainter::ShouldPaint(const ScopedPaintState& paint_state) const {
  const auto& paint_info = paint_state.GetPaintInfo();
  if (paint_info.phase != PaintPhase::kForeground &&
      !ShouldPaintSelfOutline(paint_info.phase) &&
      paint_info.phase != PaintPhase::kSelection &&
      paint_info.phase != PaintPhase::kMask &&
      !ShouldPaintSelfBlockBackground(paint_info.phase))
    return false;

  if (layout_replaced_.IsTruncated())
    return false;

  // If we're invisible or haven't received a layout yet, just bail.
  // But if it's an SVG root, there can be children, so we'll check visibility
  // later.
  if (!layout_replaced_.IsSVGRoot() &&
      layout_replaced_.StyleRef().Visibility() != EVisibility::kVisible)
    return false;

  LayoutRect local_rect(layout_replaced_.VisualOverflowRect());
  local_rect.Unite(layout_replaced_.LocalSelectionRect());
  layout_replaced_.FlipForWritingMode(local_rect);
  if (!paint_state.LocalRectIntersectsCullRect(local_rect))
    return false;

  return true;
}

}  // namespace blink
