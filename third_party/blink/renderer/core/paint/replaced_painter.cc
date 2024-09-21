// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/replaced_painter.h"

#include <optional>

#include "base/metrics/histogram_macros.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/highlight/highlight_style_utils.h"
#include "third_party/blink/renderer/core/layout/layout_replaced.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"
#include "third_party/blink/renderer/core/mobile_metrics/mobile_friendliness_checker.h"
#include "third_party/blink/renderer/core/paint/box_background_paint_context.h"
#include "third_party/blink/renderer/core/paint/box_decoration_data.h"
#include "third_party/blink/renderer/core/paint/box_model_object_painter.h"
#include "third_party/blink/renderer/core/paint/box_painter.h"
#include "third_party/blink/renderer/core/paint/box_painter_base.h"
#include "third_party/blink/renderer/core/paint/object_painter.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/paint/rounded_border_geometry.h"
#include "third_party/blink/renderer/core/paint/scoped_paint_state.h"
#include "third_party/blink/renderer/core/paint/scrollable_area_painter.h"
#include "third_party/blink/renderer/core/paint/selection_bounds_recorder.h"
#include "third_party/blink/renderer/core/paint/theme_painter.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
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
  std::optional<MobileFriendlinessChecker::IgnoreBeyondViewportScope>
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
      layout_replaced_.Parent()->IsFrameSet()) {
    return paint_info.phase == PaintPhase::kForeground;
  }
  return ShouldPaintSelfBlockBackground(paint_info.phase);
}

void ReplacedPainter::Paint(const PaintInfo& paint_info) {
  ScopedPaintState paint_state(layout_replaced_, paint_info);
  if (!ShouldPaint(paint_state))
    return;

  const auto& local_paint_info = paint_state.GetPaintInfo();
  auto paint_offset = paint_state.PaintOffset();
  PhysicalRect border_rect(paint_offset, layout_replaced_.Size());

  if (ShouldPaintBoxDecorationBackground(local_paint_info)) {
    bool should_paint_background = false;
    if (RuntimeEnabledFeatures::HitTestOpaquenessEnabled() &&
        // TODO(crbug.com/1477914): Without this condition, scaled canvas
        // would become pixelated on Linux.
        !layout_replaced_.IsCanvas()) {
      should_paint_background = true;
    } else if (layout_replaced_.HasBoxDecorationBackground()) {
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
    if (should_paint_background) {
      PaintBoxDecorationBackground(local_paint_info, paint_offset);
    }

    // We're done. We don't bother painting any children.
    if (layout_replaced_.DrawsBackgroundOntoContentLayer() ||
        local_paint_info.phase == PaintPhase::kSelfBlockBackgroundOnly) {
      return;
    }
  }

  if (local_paint_info.phase == PaintPhase::kMask) {
    PaintMask(local_paint_info, paint_offset);
    return;
  }

  if (ShouldPaintSelfOutline(local_paint_info.phase)) {
    ObjectPainter(layout_replaced_)
        .PaintOutline(local_paint_info, paint_offset);
    return;
  }

  if (local_paint_info.phase != PaintPhase::kForeground &&
      local_paint_info.phase != PaintPhase::kSelectionDragImage &&
      (!layout_replaced_.CanHaveChildren() || layout_replaced_.IsCanvas())) {
    return;
  }

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

  if (layout_replaced_.StyleRef().UsedVisibility() == EVisibility::kVisible &&
      layout_replaced_.CanResize()) {
    auto* scrollable_area = layout_replaced_.GetScrollableArea();
    DCHECK(scrollable_area);
    if (!scrollable_area->HasLayerForScrollCorner()) {
      ScrollableAreaPainter(*scrollable_area)
          .PaintResizer(local_paint_info.context, paint_offset,
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

  std::optional<SelectionBoundsRecorder> selection_recorder;
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
    Color selection_bg = HighlightStyleUtils::HighlightBackgroundColor(
        layout_replaced_.GetDocument(), layout_replaced_.StyleRef(),
        layout_replaced_.GetNode(), std::nullopt, kPseudoIdSelection,
        SearchTextIsCurrent::kNo);
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
      layout_replaced_.StyleRef().UsedVisibility() != EVisibility::kVisible) {
    return false;
  }

  PhysicalRect local_rect = layout_replaced_.VisualOverflowRect();
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

  auto overflow_size = layout_replaced_.VisualOverflowRect().size;
  auto overflow_area = overflow_size.width * overflow_size.height;

  auto content_size = layout_replaced_.Size();
  auto content_area = content_size.width * content_size.height;

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

void ReplacedPainter::PaintBoxDecorationBackground(
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset) {
  const ComputedStyle& style = layout_replaced_.StyleRef();
  if (style.UsedVisibility() != EVisibility::kVisible) {
    return;
  }

  PhysicalRect paint_rect;
  const DisplayItemClient* background_client = nullptr;
  std::optional<ScopedBoxContentsPaintState> contents_paint_state;
  bool painting_background_in_contents_space =
      paint_info.IsPaintingBackgroundInContentsSpace();
  gfx::Rect visual_rect;
  if (painting_background_in_contents_space) {
    // For the case where we are painting the background in the contents space,
    // we need to include the entire overflow rect.
    paint_rect = layout_replaced_.ScrollableOverflowRect();
    contents_paint_state.emplace(paint_info, paint_offset, layout_replaced_,
                                 paint_info.FragmentDataOverride());
    paint_rect.Move(contents_paint_state->PaintOffset());

    // The background painting code assumes that the borders are part of the
    // paint_rect so we expand the paint_rect by the border size when painting
    // the background into the scrolling contents layer.
    paint_rect.Expand(layout_replaced_.BorderOutsets());

    background_client = &layout_replaced_.GetScrollableArea()
                             ->GetScrollingBackgroundDisplayItemClient();
    visual_rect =
        layout_replaced_.GetScrollableArea()->ScrollingBackgroundVisualRect(
            paint_offset);
  } else {
    paint_rect = layout_replaced_.PhysicalBorderBoxRect();
    paint_rect.Move(paint_offset);
    background_client = &layout_replaced_;
    visual_rect = BoxPainter(layout_replaced_).VisualRect(paint_offset);
  }

  if (layout_replaced_.HasBoxDecorationBackground() &&
      !layout_replaced_.DrawsBackgroundOntoContentLayer()) {
    PaintBoxDecorationBackgroundWithRect(
        contents_paint_state ? contents_paint_state->GetPaintInfo()
                             : paint_info,
        visual_rect, paint_rect, *background_client);
  }

  ObjectPainter(layout_replaced_)
      .RecordHitTestData(paint_info, ToPixelSnappedRect(paint_rect),
                         *background_client);
  BoxPainter(layout_replaced_)
      .RecordRegionCaptureData(paint_info, paint_rect, *background_client);

  // Record the scroll hit test after the non-scrolling background so
  // background squashing is not affected. Hit test order would be equivalent
  // if this were immediately before the non-scrolling background.
  if (!painting_background_in_contents_space) {
    BoxPainter(layout_replaced_)
        .RecordScrollHitTestData(paint_info, *background_client,
                                 paint_info.FragmentDataOverride());
  }
}

void ReplacedPainter::PaintBoxDecorationBackgroundWithRect(
    const PaintInfo& paint_info,
    const gfx::Rect& visual_rect,
    const PhysicalRect& paint_rect,
    const DisplayItemClient& background_client) {
  const ComputedStyle& style = layout_replaced_.StyleRef();

  std::optional<DisplayItemCacheSkipper> cache_skipper;
  if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled() &&
      BoxPainterBase::ShouldSkipPaintUnderInvalidationChecking(
          layout_replaced_)) {
    cache_skipper.emplace(paint_info.context);
  }

  BoxDecorationData box_decoration_data(paint_info, layout_replaced_);
  if (!box_decoration_data.ShouldPaint()) {
    return;
  }

  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, background_client,
          DisplayItem::kBoxDecorationBackground)) {
    return;
  }

  DrawingRecorder recorder(paint_info.context, background_client,
                           DisplayItem::kBoxDecorationBackground, visual_rect);
  GraphicsContextStateSaver state_saver(paint_info.context, false);

  bool needs_end_layer = false;
  // FIXME: Should eventually give the theme control over whether the box
  // shadow should paint, since controls could have custom shadows of their
  // own.
  if (box_decoration_data.ShouldPaintShadow()) {
    BoxPainterBase::PaintNormalBoxShadow(
        paint_info, paint_rect, style, PhysicalBoxSides(),
        !box_decoration_data.ShouldPaintBackground());
  }

  if (BleedAvoidanceIsClipping(
          box_decoration_data.GetBackgroundBleedAvoidance())) {
    state_saver.Save();
    FloatRoundedRect border =
        RoundedBorderGeometry::PixelSnappedRoundedBorder(style, paint_rect);
    paint_info.context.ClipRoundedRect(border);

    if (box_decoration_data.GetBackgroundBleedAvoidance() ==
        kBackgroundBleedClipLayer) {
      paint_info.context.BeginLayer();
      needs_end_layer = true;
    }
  }

  // If we have a native theme appearance, paint that before painting our
  // background.  The theme will tell us whether or not we should also paint the
  // CSS background.
  gfx::Rect snapped_paint_rect = ToPixelSnappedRect(paint_rect);
  ThemePainter& theme_painter = LayoutTheme::GetTheme().Painter();
  bool theme_painted =
      box_decoration_data.HasAppearance() &&
      !theme_painter.Paint(layout_replaced_, paint_info, snapped_paint_rect);
  if (!theme_painted) {
    if (box_decoration_data.ShouldPaintBackground()) {
      PaintBackground(paint_info, paint_rect,
                      box_decoration_data.BackgroundColor(),
                      box_decoration_data.GetBackgroundBleedAvoidance());
    }
    if (box_decoration_data.HasAppearance()) {
      theme_painter.PaintDecorations(layout_replaced_.GetNode(),
                                     layout_replaced_.GetDocument(), style,
                                     paint_info, snapped_paint_rect);
    }
  }

  if (box_decoration_data.ShouldPaintShadow()) {
    BoxPainterBase::PaintInsetBoxShadowWithBorderRect(paint_info, paint_rect,
                                                      style);
  }

  // The theme will tell us whether or not we should also paint the CSS
  // border.
  if (box_decoration_data.ShouldPaintBorder()) {
    if (!theme_painted) {
      theme_painted =
          box_decoration_data.HasAppearance() &&
          !theme_painter.PaintBorderOnly(layout_replaced_.GetNode(), style,
                                         paint_info, snapped_paint_rect);
    }
    if (!theme_painted) {
      BoxPainterBase::PaintBorder(
          layout_replaced_, layout_replaced_.GetDocument(),
          layout_replaced_.GeneratingNode(), paint_info, paint_rect, style,
          box_decoration_data.GetBackgroundBleedAvoidance());
    }
  }

  if (needs_end_layer) {
    paint_info.context.EndLayer();
  }
}

void ReplacedPainter::PaintBackground(
    const PaintInfo& paint_info,
    const PhysicalRect& paint_rect,
    const Color& background_color,
    BackgroundBleedAvoidance bleed_avoidance) {
  if (layout_replaced_.BackgroundTransfersToView()) {
    return;
  }
  if (layout_replaced_.BackgroundIsKnownToBeObscured()) {
    return;
  }
  BoxModelObjectPainter box_model_painter(layout_replaced_);
  BoxBackgroundPaintContext bg_paint_context(layout_replaced_);
  box_model_painter.PaintFillLayers(
      paint_info, background_color,
      layout_replaced_.StyleRef().BackgroundLayers(), paint_rect,
      bg_paint_context, bleed_avoidance);
}

void ReplacedPainter::PaintMask(const PaintInfo& paint_info,
                                const PhysicalOffset& paint_offset) {
  DCHECK_EQ(PaintPhase::kMask, paint_info.phase);

  if (!layout_replaced_.HasMask() ||
      layout_replaced_.StyleRef().UsedVisibility() != EVisibility::kVisible) {
    return;
  }

  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, layout_replaced_, paint_info.phase)) {
    return;
  }

  PhysicalRect paint_rect(paint_offset, layout_replaced_.Size());
  BoxDrawingRecorder recorder(paint_info.context, layout_replaced_,
                              paint_info.phase, paint_offset);
  PaintMaskImages(paint_info, paint_rect);
}

void ReplacedPainter::PaintMaskImages(const PaintInfo& paint_info,
                                      const PhysicalRect& paint_rect) {
  // For mask images legacy layout painting handles multi-line boxes by giving
  // the full width of the element, not the current line box, thereby clipping
  // the offending edges.
  BoxModelObjectPainter painter(layout_replaced_);
  BoxBackgroundPaintContext bg_paint_context(layout_replaced_);
  painter.PaintMaskImages(paint_info, paint_rect, layout_replaced_,
                          bg_paint_context, PhysicalBoxSides());
}

}  // namespace blink
