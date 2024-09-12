// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/inline_box_fragment_painter.h"

#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/layout/background_bleed_avoidance.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/logical_fragment.h"
#include "third_party/blink/renderer/core/paint/box_background_paint_context.h"
#include "third_party/blink/renderer/core/paint/nine_piece_image_painter.h"
#include "third_party/blink/renderer/core/paint/object_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_phase.h"
#include "third_party/blink/renderer/core/paint/scoped_paint_state.h"
#include "third_party/blink/renderer/core/paint/scoped_svg_paint_state.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/url_metadata_utils.h"
#include "third_party/blink/renderer/core/style/nine_piece_image.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_display_item_fragment.h"

namespace blink {

namespace {

template <class Items>
bool HasMultipleItems(const Items items) {
  auto iter = items.begin();
  DCHECK(iter != items.end());
  return iter != items.end() && ++iter != items.end();
}

inline bool MayHaveMultipleFragmentItems(const FragmentItem& item,
                                         const LayoutObject& layout_object) {
  if (!item.IsFirstForNode() || !item.IsLastForNode()) {
    return true;
  }
  // TODO(crbug.com/40122434): InlineCursor is currently unable to deal with
  // objects split into multiple fragmentainers (e.g. columns). Just return true
  // if it's possible that this object participates in a fragmentation context.
  // This will give false positives, but that should be harmless, given the way
  // the return value is used by the caller.
  if (layout_object.IsInsideFlowThread()) [[unlikely]] {
    return true;
  }
  return false;
}

}  // namespace

PhysicalBoxSides InlineBoxFragmentPainter::SidesToInclude() const {
  return BoxFragment().SidesToInclude();
}

void InlineBoxFragmentPainter::Paint(const PaintInfo& paint_info,
                                     const PhysicalOffset& paint_offset) {
  ScopedDisplayItemFragment display_item_fragment(
      paint_info.context, inline_box_item_.FragmentId());
  const LayoutObject& layout_object = *inline_box_fragment_.GetLayoutObject();
  std::optional<ScopedSVGPaintState> svg_paint_state;
  const PhysicalOffset adjusted_paint_offset =
      paint_offset + inline_box_item_.OffsetInContainerFragment();

  if (!layout_object.IsSVGInline()) {
    if (paint_info.phase == PaintPhase::kMask) {
      PaintMask(paint_info, adjusted_paint_offset);
      return;
    }
    if (paint_info.phase == PaintPhase::kForeground) {
      PaintBackgroundBorderShadow(paint_info, adjusted_paint_offset);
    }
  } else {
    svg_paint_state.emplace(layout_object, paint_info);
  }
  const bool suppress_box_decoration_background = true;
  DCHECK(inline_context_);
  InlinePaintContext::ScopedInlineItem scoped_item(inline_box_item_,
                                                   inline_context_);
  DCHECK(inline_box_cursor_);
  BoxFragmentPainter box_painter(*inline_box_cursor_, inline_box_item_,
                                 BoxFragment(), inline_context_);
  box_painter.PaintObject(paint_info, adjusted_paint_offset,
                          suppress_box_decoration_background);
}

void InlineBoxFragmentPainter::PaintMask(const PaintInfo& paint_info,
                                         const PhysicalOffset& paint_offset) {
  DCHECK_EQ(PaintPhase::kMask, paint_info.phase);
  if (!style_.HasMask() || style_.UsedVisibility() != EVisibility::kVisible) {
    return;
  }

  const DisplayItemClient& display_item_client = GetDisplayItemClient();
  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, display_item_client, paint_info.phase)) {
    return;
  }

  DrawingRecorder recorder(paint_info.context, display_item_client,
                           paint_info.phase, VisualRect(paint_offset));
  PhysicalRect adjusted_frame_rect(paint_offset,
                                   inline_box_fragment_.LocalRect().size);

  const LayoutObject& layout_object = *inline_box_fragment_.GetLayoutObject();
  bool object_may_have_multiple_boxes =
      MayHaveMultipleFragmentItems(inline_box_item_, layout_object);

  DCHECK(inline_box_cursor_);
  BoxFragmentPainter box_painter(*inline_box_cursor_, inline_box_item_,
                                 BoxFragment(), inline_context_);

  BoxBackgroundPaintContext bg_paint_context(
      static_cast<const LayoutBoxModelObject&>(layout_object));
  PaintFillLayers(box_painter, paint_info, Color::kTransparent,
                  style_.MaskLayers(), adjusted_frame_rect, bg_paint_context,
                  object_may_have_multiple_boxes);

  gfx::Rect adjusted_clip_rect;
  SlicePaintingType border_painting_type =
      GetSlicePaintType(style_.MaskBoxImage(), adjusted_frame_rect,
                        adjusted_clip_rect, object_may_have_multiple_boxes);
  if (border_painting_type == kDontPaint) {
    return;
  }
  GraphicsContextStateSaver state_saver(paint_info.context, false);
  PhysicalRect adjusted_paint_rect;
  if (border_painting_type == kPaintWithClip) {
    state_saver.Save();
    paint_info.context.Clip(adjusted_clip_rect);
    adjusted_paint_rect =
        PaintRectForImageStrip(adjusted_frame_rect, style_.Direction());
  } else {
    adjusted_paint_rect = adjusted_frame_rect;
  }
  NinePieceImagePainter::Paint(paint_info.context, image_observer_, *document_,
                               node_, adjusted_paint_rect, style_,
                               style_.MaskBoxImage(), SidesToInclude());
}

void InlineBoxFragmentPainterBase::PaintBackgroundBorderShadow(
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset) {
  DCHECK(paint_info.phase == PaintPhase::kForeground);
  if (inline_box_fragment_.Style().UsedVisibility() != EVisibility::kVisible ||
      inline_box_fragment_.IsOpaque()) {
    return;
  }

  // You can use p::first-line to specify a background. If so, the direct child
  // inline boxes of line boxes may actually have to paint a background.
  // TODO(layout-dev): Cache HasBoxDecorationBackground on the fragment like
  // we do for LayoutObject. Querying Style each time is too costly.
  bool should_paint_box_decoration_background =
      inline_box_fragment_.GetLayoutObject()->HasBoxDecorationBackground() ||
      inline_box_fragment_.UsesFirstLineStyle();

  if (!should_paint_box_decoration_background)
    return;

  const DisplayItemClient& display_item_client = GetDisplayItemClient();
  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, display_item_client,
          DisplayItem::kBoxDecorationBackground))
    return;

  PhysicalRect frame_rect = inline_box_fragment_.LocalRect();
  PhysicalRect adjusted_frame_rect(paint_offset, frame_rect.size);

  DrawingRecorder recorder(paint_info.context, display_item_client,
                           DisplayItem::kBoxDecorationBackground,
                           VisualRect(paint_offset));

  DCHECK(inline_box_fragment_.GetLayoutObject());
  const LayoutObject& layout_object = *inline_box_fragment_.GetLayoutObject();
  bool object_may_have_multiple_boxes =
      MayHaveMultipleFragmentItems(inline_box_item_, layout_object);

  DCHECK(inline_box_cursor_);
  DCHECK(inline_context_);
  BoxFragmentPainter box_painter(*inline_box_cursor_, inline_box_item_,
                                 To<PhysicalBoxFragment>(inline_box_fragment_),
                                 inline_context_);
  // TODO(eae): Switch to LayoutNG version of BoxBackgroundPaintContext.
  BoxBackgroundPaintContext bg_paint_context(
      *static_cast<const LayoutBoxModelObject*>(
          inline_box_fragment_.GetLayoutObject()));
  PaintBoxDecorationBackground(
      box_painter, paint_info, paint_offset, adjusted_frame_rect,
      bg_paint_context, object_may_have_multiple_boxes, SidesToInclude());
}

gfx::Rect InlineBoxFragmentPainterBase::VisualRect(
    const PhysicalOffset& paint_offset) {
  PhysicalRect overflow_rect = inline_box_item_.SelfInkOverflowRect();
  overflow_rect.Move(paint_offset);
  return ToEnclosingRect(overflow_rect);
}

void LineBoxFragmentPainter::PaintBackgroundBorderShadow(
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset) {
  DCHECK_EQ(paint_info.phase, PaintPhase::kForeground);
  DCHECK_EQ(inline_box_fragment_.Type(), PhysicalFragment::kFragmentLineBox);
  DCHECK(NeedsPaint(inline_box_fragment_));
  // |FragmentItem| uses the fragment id when painting the background of
  // line boxes. Please see |FragmentItem::kInitialLineFragmentId|.
  DCHECK_NE(paint_info.context.GetPaintController().CurrentFragment(), 0u);

  if (line_style_ == style_ ||
      line_style_.UsedVisibility() != EVisibility::kVisible) {
    return;
  }

  const DisplayItemClient& display_item_client = GetDisplayItemClient();
  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, display_item_client,
          DisplayItem::kBoxDecorationBackground))
    return;

  // Compute the content box for the `::first-line` box. It's different from
  // fragment size because the height of line box includes `line-height` while
  // the height of inline box does not. The box "behaves similar to that of an
  // inline-level element".
  // https://drafts.csswg.org/css-pseudo-4/#first-line-styling
  const PhysicalLineBoxFragment& line_box = LineBoxFragment();
  const FontHeight line_metrics = line_box.Metrics();
  const FontHeight text_metrics = line_style_.GetFontHeight();
  const WritingMode writing_mode = line_style_.GetWritingMode();
  PhysicalRect rect;
  if (IsHorizontalWritingMode(writing_mode)) {
    rect.offset.top = line_metrics.ascent - text_metrics.ascent;
    rect.size = {line_box.Size().width, text_metrics.LineHeight()};
  } else {
    rect.offset.left =
        line_box.Size().width - line_metrics.ascent - text_metrics.descent;
    rect.size = {text_metrics.LineHeight(), line_box.Size().height};
  }
  rect.offset += paint_offset;

  DrawingRecorder recorder(paint_info.context, display_item_client,
                           DisplayItem::kBoxDecorationBackground,
                           VisualRect(paint_offset));

  const LayoutBlockFlow& layout_block_flow =
      *To<LayoutBlockFlow>(block_fragment_.GetLayoutObject());
  BoxFragmentPainter box_painter(block_fragment_);
  BoxBackgroundPaintContext bg_paint_context(layout_block_flow);
  PaintBoxDecorationBackground(
      box_painter, paint_info, paint_offset, rect, bg_paint_context,
      /*object_has_multiple_boxes*/ false, PhysicalBoxSides());
}

void InlineBoxFragmentPainterBase::ComputeFragmentOffsetOnLine(
    TextDirection direction,
    LayoutUnit* offset_on_line,
    LayoutUnit* total_width) const {
  WritingDirectionMode writing_direction =
      inline_box_fragment_.Style().GetWritingDirection();
  InlineCursor cursor;
  DCHECK(inline_box_fragment_.GetLayoutObject());
  cursor.MoveTo(*inline_box_fragment_.GetLayoutObject());

  LayoutUnit before;
  LayoutUnit after;
  bool before_self = true;
  for (; cursor; cursor.MoveToNextForSameLayoutObject()) {
    DCHECK(cursor.CurrentItem());
    if (cursor.CurrentItem() == &inline_box_item_) {
      before_self = false;
      continue;
    }
    const PhysicalBoxFragment* box_fragment = cursor.Current().BoxFragment();
    DCHECK(box_fragment);
    if (before_self)
      before += LogicalFragment(writing_direction, *box_fragment).InlineSize();
    else
      after += LogicalFragment(writing_direction, *box_fragment).InlineSize();
  }

  *total_width =
      before + after +
      LogicalFragment(writing_direction, inline_box_fragment_).InlineSize();

  // We're iterating over the fragments in physical order before so we need to
  // swap before and after for RTL.
  *offset_on_line = direction == TextDirection::kLtr ? before : after;
}

PhysicalRect InlineBoxFragmentPainterBase::PaintRectForImageStrip(
    const PhysicalRect& paint_rect,
    TextDirection direction) const {
  // We have a fill/border/mask image that spans multiple lines.
  // We need to adjust the offset by the width of all previous lines.
  // Think of background painting on inlines as though you had one long line, a
  // single continuous strip. Even though that strip has been broken up across
  // multiple lines, you still paint it as though you had one single line. This
  // means each line has to pick up the background where the previous line left
  // off.
  LayoutUnit offset_on_line;
  LayoutUnit total_width;
  ComputeFragmentOffsetOnLine(direction, &offset_on_line, &total_width);

  if (inline_box_fragment_.Style().IsHorizontalWritingMode()) {
    return PhysicalRect(paint_rect.X() - offset_on_line, paint_rect.Y(),
                        total_width, paint_rect.Height());
  }
  return PhysicalRect(paint_rect.X(), paint_rect.Y() - offset_on_line,
                      paint_rect.Width(), total_width);
}

PhysicalRect InlineBoxFragmentPainterBase::ClipRectForNinePieceImageStrip(
    const ComputedStyle& style,
    PhysicalBoxSides sides_to_include,
    const NinePieceImage& image,
    const PhysicalRect& paint_rect) {
  PhysicalRect clip_rect(paint_rect);
  PhysicalBoxStrut outsets = style.ImageOutsets(image);
  if (sides_to_include.left) {
    clip_rect.SetX(paint_rect.X() - outsets.left);
    clip_rect.SetWidth(paint_rect.Width() + outsets.left);
  }
  if (sides_to_include.right) {
    clip_rect.SetWidth(clip_rect.Width() + outsets.right);
  }
  if (sides_to_include.top) {
    clip_rect.SetY(paint_rect.Y() - outsets.top);
    clip_rect.SetHeight(paint_rect.Height() + outsets.top);
  }
  if (sides_to_include.bottom) {
    clip_rect.SetHeight(clip_rect.Height() + outsets.bottom);
  }
  return clip_rect;
}

InlineBoxFragmentPainterBase::SlicePaintingType
InlineBoxFragmentPainterBase::GetBorderPaintType(
    const PhysicalRect& adjusted_frame_rect,
    gfx::Rect& adjusted_clip_rect,
    bool object_has_multiple_boxes) const {
  const ComputedStyle& style = inline_box_fragment_.Style();
  if (!style.HasBorderDecoration()) {
    return kDontPaint;
  }
  return GetSlicePaintType(style.BorderImage(), adjusted_frame_rect,
                           adjusted_clip_rect, object_has_multiple_boxes);
}

InlineBoxFragmentPainterBase::SlicePaintingType
InlineBoxFragmentPainterBase::GetSlicePaintType(
    const NinePieceImage& nine_piece_image,
    const PhysicalRect& adjusted_frame_rect,
    gfx::Rect& adjusted_clip_rect,
    bool object_has_multiple_boxes) const {
  StyleImage* nine_piece_image_source = nine_piece_image.GetImage();
  bool has_nine_piece_image =
      nine_piece_image_source && nine_piece_image_source->CanRender();
  if (has_nine_piece_image && !nine_piece_image_source->IsLoaded()) {
    return kDontPaint;
  }

  // The simple case is where we either have no border image or we are the
  // only box for this object.  In those cases only a single call to draw is
  // required.
  const ComputedStyle& style = inline_box_fragment_.Style();
  if (!has_nine_piece_image || !object_has_multiple_boxes ||
      style.BoxDecorationBreak() == EBoxDecorationBreak::kClone) {
    adjusted_clip_rect = ToPixelSnappedRect(adjusted_frame_rect);
    return kPaintWithoutClip;
  }

  // We have a border image that spans multiple lines.
  adjusted_clip_rect = ToPixelSnappedRect(ClipRectForNinePieceImageStrip(
      style, SidesToInclude(), nine_piece_image, adjusted_frame_rect));
  return kPaintWithClip;
}

void InlineBoxFragmentPainterBase::PaintNormalBoxShadow(
    const PaintInfo& info,
    const ComputedStyle& s,
    const PhysicalRect& paint_rect) {
  BoxPainterBase::PaintNormalBoxShadow(info, paint_rect, s, SidesToInclude());
}

void InlineBoxFragmentPainterBase::PaintInsetBoxShadow(
    const PaintInfo& info,
    const ComputedStyle& s,
    const PhysicalRect& paint_rect) {
  BoxPainterBase::PaintInsetBoxShadowWithBorderRect(info, paint_rect, s,
                                                    SidesToInclude());
}

void InlineBoxFragmentPainterBase::PaintBoxDecorationBackground(
    BoxPainterBase& box_painter,
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset,
    const PhysicalRect& adjusted_frame_rect,
    const BoxBackgroundPaintContext& bg_paint_context,
    bool object_has_multiple_boxes,
    PhysicalBoxSides sides_to_include) {
  // Shadow comes first and is behind the background and border.
  PaintNormalBoxShadow(paint_info, line_style_, adjusted_frame_rect);

  Color background_color =
      line_style_.VisitedDependentColor(GetCSSPropertyBackgroundColor());
  PaintFillLayers(box_painter, paint_info, background_color,
                  line_style_.BackgroundLayers(), adjusted_frame_rect,
                  bg_paint_context, object_has_multiple_boxes);

  PaintInsetBoxShadow(paint_info, line_style_, adjusted_frame_rect);

  gfx::Rect adjusted_clip_rect;
  SlicePaintingType border_painting_type = GetBorderPaintType(
      adjusted_frame_rect, adjusted_clip_rect, object_has_multiple_boxes);
  switch (border_painting_type) {
    case kDontPaint:
      break;
    case kPaintWithoutClip:
      BoxPainterBase::PaintBorder(image_observer_, *document_, node_,
                                  paint_info, adjusted_frame_rect, line_style_,
                                  kBackgroundBleedNone, sides_to_include);
      break;
    case kPaintWithClip:
      // FIXME: What the heck do we do with RTL here? The math we're using is
      // obviously not right, but it isn't even clear how this should work at
      // all.
      PhysicalRect image_strip_paint_rect =
          PaintRectForImageStrip(adjusted_frame_rect, TextDirection::kLtr);
      GraphicsContextStateSaver state_saver(paint_info.context);
      paint_info.context.Clip(adjusted_clip_rect);
      BoxPainterBase::PaintBorder(image_observer_, *document_, node_,
                                  paint_info, image_strip_paint_rect,
                                  line_style_);
      break;
  }
}

void InlineBoxFragmentPainterBase::PaintFillLayers(
    BoxPainterBase& box_painter,
    const PaintInfo& info,
    const Color& c,
    const FillLayer& layer,
    const PhysicalRect& rect,
    const BoxBackgroundPaintContext& bg_paint_context,
    bool object_has_multiple_boxes) {
  // FIXME: This should be a for loop or similar. It's a little non-trivial to
  // do so, however, since the layers need to be painted in reverse order.
  if (layer.Next()) {
    PaintFillLayers(box_painter, info, c, *layer.Next(), rect, bg_paint_context,
                    object_has_multiple_boxes);
  }
  PaintFillLayer(box_painter, info, c, layer, rect, bg_paint_context,
                 object_has_multiple_boxes);
}

void InlineBoxFragmentPainterBase::PaintFillLayer(
    BoxPainterBase& box_painter,
    const PaintInfo& paint_info,
    const Color& c,
    const FillLayer& fill_layer,
    const PhysicalRect& paint_rect,
    const BoxBackgroundPaintContext& bg_paint_context,
    bool object_has_multiple_boxes) {
  StyleImage* img = fill_layer.GetImage();
  bool has_fill_image = img && img->CanRender();

  if (!object_has_multiple_boxes ||
      (!has_fill_image && !style_.HasBorderRadius())) {
    box_painter.PaintFillLayer(paint_info, c, fill_layer, paint_rect,
                               kBackgroundBleedNone, bg_paint_context, false);
    return;
  }

  // Handle fill images that clone or spans multiple lines.
  bool multi_line = object_has_multiple_boxes &&
                    style_.BoxDecorationBreak() != EBoxDecorationBreak::kClone;
  PhysicalRect rect =
      multi_line ? PaintRectForImageStrip(paint_rect, style_.Direction())
                 : paint_rect;
  GraphicsContextStateSaver state_saver(paint_info.context);
  paint_info.context.Clip(ToPixelSnappedRect(paint_rect));
  box_painter.PaintFillLayer(paint_info, c, fill_layer, rect,
                             kBackgroundBleedNone, bg_paint_context, multi_line,
                             paint_rect.size);
}

// Paint all fragments for the |layout_inline|. This function is used only for
// self-painting |LayoutInline|.
void InlineBoxFragmentPainter::PaintAllFragments(
    const LayoutInline& layout_inline,
    const FragmentData& fragment_data,
    wtf_size_t fragment_data_idx,
    const PaintInfo& paint_info) {
  // TODO(kojii): If the block flow is dirty, children of these fragments
  // maybe already deleted. crbug.com/963103
  const LayoutBlockFlow* block_flow = layout_inline.FragmentItemsContainer();
  if (block_flow->NeedsLayout()) [[unlikely]] {
    return;
  }

  ScopedPaintState paint_state(layout_inline, paint_info, &fragment_data);
  PhysicalOffset paint_offset = paint_state.PaintOffset();
  const PaintInfo& local_paint_info = paint_state.GetPaintInfo();

  if (local_paint_info.phase == PaintPhase::kForeground &&
      local_paint_info.ShouldAddUrlMetadata()) {
    ObjectPainter(layout_inline)
        .AddURLRectIfNeeded(local_paint_info, paint_offset);
  }

  ScopedPaintTimingDetectorBlockPaintHook
      scoped_paint_timing_detector_block_paint_hook;
  if (paint_info.phase == PaintPhase::kForeground) {
    scoped_paint_timing_detector_block_paint_hook.EmplaceIfNeeded(
        layout_inline,
        paint_info.context.GetPaintController().CurrentPaintChunkProperties());
  }

  if (paint_info.phase == PaintPhase::kForeground &&
      paint_info.ShouldAddUrlMetadata()) {
    // URLRects for descendants are normally added via BoxFragmentPainter::
    // PaintLineBoxes(), but relatively positioned (self-painting) inlines
    // are omitted. Do it now.
    AddURLRectsForInlineChildrenRecursively(layout_inline, paint_info,
                                            paint_offset);
  }

  InlinePaintContext inline_context;
  InlineCursor first_container_cursor(*block_flow);
  first_container_cursor.MoveTo(layout_inline);

  wtf_size_t container_fragment_idx =
      first_container_cursor.ContainerFragmentIndex() + fragment_data_idx;
  const PhysicalBoxFragment* container_fragment =
      block_flow->GetPhysicalFragment(container_fragment_idx);

  InlineCursor cursor(*container_fragment);
  cursor.MoveTo(layout_inline);
  for (; cursor; cursor.MoveToNextForSameLayoutObject()) {
    InlinePaintContext::ScopedInlineBoxAncestors scoped_items(cursor,
                                                              &inline_context);
    const FragmentItem* item = cursor.CurrentItem();
    DCHECK(item);
    const PhysicalBoxFragment* box_fragment = item->BoxFragment();
    DCHECK(box_fragment);
    InlineBoxFragmentPainter(cursor, *item, *box_fragment, &inline_context)
        .Paint(paint_info, paint_offset);
  }
}

#if DCHECK_IS_ON()
void InlineBoxFragmentPainter::CheckValid() const {
  DCHECK(inline_box_cursor_);
  DCHECK_EQ(inline_box_cursor_->Current().Item(), &inline_box_item_);

  DCHECK(inline_box_fragment_.IsInlineBox());
}
#endif

}  // namespace blink
