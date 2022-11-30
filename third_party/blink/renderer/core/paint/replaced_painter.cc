// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/replaced_painter.h"

#include "base/metrics/histogram_macros.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/layout_replaced.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"
#include "third_party/blink/renderer/core/mobile_metrics/mobile_friendliness_checker.h"
#include "third_party/blink/renderer/core/paint/box_painter.h"
#include "third_party/blink/renderer/core/paint/highlight_painting_utils.h"
#include "third_party/blink/renderer/core/paint/object_painter.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/paint/scoped_paint_state.h"
#include "third_party/blink/renderer/core/paint/scrollable_area_painter.h"
#include "third_party/blink/renderer/core/paint/selection_bounds_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item_cache_skipper.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_paint_chunk_properties.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

namespace {

// Adjusts cull rect and paint chunk properties of the input ScopedPaintState
// for ReplacedContentTransform if needed.
class ScopedReplacedContentPaintState : public ScopedPaintState {
 public:
  ScopedReplacedContentPaintState(const ScopedPaintState& input,
                                  const LayoutReplaced& replaced);

 private:
  absl::optional<MobileFriendlinessChecker::IgnoreBeyondViewportScope>
      mf_ignore_scope_;
};

ScopedReplacedContentPaintState::ScopedReplacedContentPaintState(
    const ScopedPaintState& input,
    const LayoutReplaced& replaced)
    : ScopedPaintState(input) {
  if (!fragment_to_paint_)
    return;

  if (input_paint_info_.phase == PaintPhase::kForeground) {
    if (auto* mf_checker =
            MobileFriendlinessChecker::From(replaced.GetDocument())) {
      PhysicalRect content_rect = replaced.ReplacedContentRect();
      content_rect.Move(paint_offset_);
      content_rect.Intersect(PhysicalRect(GetPaintInfo().GetCullRect().Rect()));
      mf_checker->NotifyPaintReplaced(content_rect,
                                      GetPaintInfo()
                                          .context.GetPaintController()
                                          .CurrentPaintChunkProperties()
                                          .Transform());
      mf_ignore_scope_.emplace(*mf_checker);
    }
  }

  const auto* paint_properties = fragment_to_paint_->PaintProperties();
  if (!paint_properties)
    return;

  auto new_properties = input_paint_info_.context.GetPaintController()
                            .CurrentPaintChunkProperties();
  bool property_changed = false;

  const auto* content_transform = paint_properties->ReplacedContentTransform();
  if (content_transform) {
    new_properties.SetTransform(*content_transform);
    adjusted_paint_info_.emplace(input_paint_info_);
    adjusted_paint_info_->TransformCullRect(*content_transform);
    property_changed = true;
  }

  if (const auto* clip = paint_properties->OverflowClip()) {
    new_properties.SetClip(*clip);
    property_changed = true;
  }

  if (property_changed) {
    chunk_properties_.emplace(input_paint_info_.context.GetPaintController(),
                              new_properties, replaced,
                              input_paint_info_.DisplayItemTypeForClipping());
  }
}

}  // anonymous namespace

bool ReplacedPainter::ShouldPaintBoxDecorationBackground(
    const PaintInfo& paint_info) {
  // LayoutFrameSet paints everything in the foreground phase.
  if (layout_replaced_.IsLayoutEmbeddedContent() &&
      layout_replaced_.Parent()->IsFrameSetIncludingNG())
    return paint_info.phase == PaintPhase::kForeground;
  return ShouldPaintSelfBlockBackground(paint_info.phase);
}

void ReplacedPainter::Paint(const PaintInfo& paint_info) {
  // TODO(crbug.com/797779): For now embedded contents don't know whether
  // they are painted in a fragmented context and may do something bad in a
  // fragmented context, e.g. creating subsequences. Skip cache to avoid that.
  // This will be unnecessary when the contents are fragment aware.
  absl::optional<DisplayItemCacheSkipper> cache_skipper;
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
  PhysicalRect border_rect(paint_offset, layout_replaced_.Size());

  if (ShouldPaintBoxDecorationBackground(local_paint_info)) {
    bool should_paint_background = false;
    if (layout_replaced_.StyleRef().Visibility() == EVisibility::kVisible) {
      if (layout_replaced_.HasBoxDecorationBackground()) {
        should_paint_background = true;
      } else if (layout_replaced_.HasEffectiveAllowedTouchAction() ||
                 layout_replaced_.InsideBlockingWheelEventHandler()) {
        should_paint_background = true;
      } else {
        Element* element = DynamicTo<Element>(layout_replaced_.GetNode());
        if (element && element->GetRegionCaptureCropId()) {
          should_paint_background = true;
        }
      }
    }
    if (should_paint_background) {
      if (layout_replaced_.DrawsBackgroundOntoContentLayer()) {
        // If the background paints into the content layer, we can skip painting
        // the background but still need to paint the hit test rects.
        BoxPainter(layout_replaced_)
            .RecordHitTestData(local_paint_info, border_rect, layout_replaced_);
        BoxPainter(layout_replaced_)
            .RecordRegionCaptureData(local_paint_info, border_rect,
                                     layout_replaced_);
        return;
      }

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
      local_paint_info.phase != PaintPhase::kSelectionDragImage &&
      !layout_replaced_.CanHaveChildren())
    return;

  if (local_paint_info.phase == PaintPhase::kSelectionDragImage &&
      !layout_replaced_.IsSelected())
    return;

  bool has_clip =
      layout_replaced_.FirstFragment().PaintProperties() &&
      layout_replaced_.FirstFragment().PaintProperties()->OverflowClip();
  if (!has_clip || !layout_replaced_.PhysicalContentBoxRect().IsEmpty()) {
    ScopedReplacedContentPaintState content_paint_state(paint_state,
                                                        layout_replaced_);
    layout_replaced_.PaintReplaced(content_paint_state.GetPaintInfo(),
                                   content_paint_state.PaintOffset());
    MeasureOverflowMetrics();
  }

  if (layout_replaced_.StyleRef().Visibility() == EVisibility::kVisible &&
      layout_replaced_.CanResize()) {
    auto* scrollable_area = layout_replaced_.GetScrollableArea();
    DCHECK(scrollable_area);
    if (!scrollable_area->HasLayerForScrollCorner()) {
      ScrollableAreaPainter(*scrollable_area)
          .PaintResizer(local_paint_info.context,
                        ToRoundedVector2d(paint_offset),
                        local_paint_info.GetCullRect());
    }
    // Otherwise the resizer will be painted by the scroll corner layer.
  }

  // The selection tint never gets clipped by border-radius rounding, since we
  // want it to run right up to the edges of surrounding content.
  bool draw_selection_tint =
      local_paint_info.phase == PaintPhase::kForeground &&
      layout_replaced_.IsSelected() && layout_replaced_.CanBeSelectionLeaf() &&
      !layout_replaced_.GetDocument().Printing();
  if (!draw_selection_tint)
    return;

  absl::optional<SelectionBoundsRecorder> selection_recorder;
  const FrameSelection& frame_selection =
      layout_replaced_.GetFrame()->Selection();
  SelectionState selection_state = layout_replaced_.GetSelectionState();
  if (SelectionBoundsRecorder::ShouldRecordSelection(frame_selection,
                                                     selection_state)) {
    PhysicalRect selection_rect = layout_replaced_.LocalSelectionVisualRect();
    selection_rect.Move(paint_offset);
    const ComputedStyle& style = layout_replaced_.StyleRef();
    selection_recorder.emplace(selection_state, selection_rect,
                               local_paint_info.context.GetPaintController(),
                               style.Direction(), style.GetWritingMode(),
                               layout_replaced_);
  }

  if (!DrawingRecorder::UseCachedDrawingIfPossible(
          local_paint_info.context, layout_replaced_,
          DisplayItem::kSelectionTint)) {
    PhysicalRect selection_painting_rect =
        layout_replaced_.LocalSelectionVisualRect();
    selection_painting_rect.Move(paint_offset);
    gfx::Rect selection_painting_int_rect =
        ToPixelSnappedRect(selection_painting_rect);

    DrawingRecorder recorder(local_paint_info.context, layout_replaced_,
                             DisplayItem::kSelectionTint,
                             selection_painting_int_rect);
    Color selection_bg = HighlightPaintingUtils::HighlightBackgroundColor(
        layout_replaced_.GetDocument(), layout_replaced_.StyleRef(),
        layout_replaced_.GetNode(), absl::nullopt, kPseudoIdSelection);
    local_paint_info.context.FillRect(
        selection_painting_int_rect, selection_bg,
        PaintAutoDarkMode(layout_replaced_.StyleRef(),
                          DarkModeFilter::ElementRole::kBackground));
  }
}

bool ReplacedPainter::ShouldPaint(const ScopedPaintState& paint_state) const {
  const auto& paint_info = paint_state.GetPaintInfo();
  if (paint_info.phase != PaintPhase::kForeground &&
      paint_info.phase != PaintPhase::kForcedColorsModeBackplate &&
      !ShouldPaintSelfOutline(paint_info.phase) &&
      paint_info.phase != PaintPhase::kSelectionDragImage &&
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

  PhysicalRect local_rect = layout_replaced_.PhysicalVisualOverflowRect();
  local_rect.Unite(layout_replaced_.LocalSelectionVisualRect());
  if (!paint_state.LocalRectIntersectsCullRect(local_rect))
    return false;

  return true;
}

void ReplacedPainter::MeasureOverflowMetrics() const {
  if (!layout_replaced_.BelongsToElementChangingOverflowBehaviour() ||
      layout_replaced_.ClipsToContentBox() ||
      !layout_replaced_.HasVisualOverflow()) {
    return;
  }

  auto overflow_size = layout_replaced_.PhysicalVisualOverflowRect().size;
  auto overflow_area = overflow_size.width * overflow_size.height;

  auto content_size = layout_replaced_.Size();
  auto content_area = content_size.Width() * content_size.Height();

  DCHECK_GE(overflow_area, content_area);
  if (overflow_area == content_area)
    return;

  const float device_pixel_ratio =
      layout_replaced_.GetDocument().DevicePixelRatio();
  const int overflow_outside_content_rect =
      (overflow_area - content_area).ToInt() / pow(device_pixel_ratio, 2);
  UMA_HISTOGRAM_COUNTS_100000(
      "Blink.Overflow.ReplacedElementAreaOutsideContentRect",
      overflow_outside_content_rect);

  UseCounter::Count(layout_replaced_.GetDocument(),
                    WebFeature::kReplacedElementPaintedWithOverflow);
  constexpr int kMaxContentBreakageHeuristic = 5000;
  if (overflow_outside_content_rect > kMaxContentBreakageHeuristic) {
    UseCounter::Count(layout_replaced_.GetDocument(),
                      WebFeature::kReplacedElementPaintedWithLargeOverflow);
  }
}

}  // namespace blink
