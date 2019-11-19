// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/ng/ng_inline_box_fragment_painter.h"

#include "third_party/blink/renderer/core/layout/background_bleed_avoidance.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_text_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragment.h"
#include "third_party/blink/renderer/core/paint/background_image_geometry.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_phase.h"
#include "third_party/blink/renderer/core/style/nine_piece_image.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"

namespace blink {

namespace {

template <class Items>
bool HasMultipleItems(const Items items) {
  auto iter = items.begin();
  DCHECK(iter != items.end());
  return iter != items.end() && ++iter != items.end();
}

inline bool HasMultiplePaintFragments(const LayoutObject& layout_object) {
  return HasMultipleItems(NGPaintFragment::InlineFragmentsFor(&layout_object));
}

inline bool HasMultipleFragmentItems(const LayoutObject& layout_object) {
  return HasMultipleItems(NGFragmentItem::ItemsFor(layout_object));
}

}  // namespace

const NGBorderEdges NGInlineBoxFragmentPainter::BorderEdges() const {
  if (border_edges_.has_value())
    return *border_edges_;
  border_edges_ = NGBorderEdges::FromPhysical(PhysicalFragment().BorderEdges(),
                                              style_.GetWritingMode());
  return *border_edges_;
}

void NGInlineBoxFragmentPainter::Paint(const PaintInfo& paint_info,
                                       const PhysicalOffset& paint_offset) {
  const PhysicalOffset adjusted_paint_offset =
      paint_offset + (inline_box_paint_fragment_
                          ? inline_box_paint_fragment_->Offset()
                          : inline_box_item_->Offset());
  if (paint_info.phase == PaintPhase::kForeground)
    PaintBackgroundBorderShadow(paint_info, adjusted_paint_offset);

  NGBoxFragmentPainter box_painter(PhysicalFragment(),
                                   inline_box_paint_fragment_, descendants_);
  bool suppress_box_decoration_background = true;
  box_painter.PaintObject(paint_info, adjusted_paint_offset,
                          suppress_box_decoration_background);
}

void NGInlineBoxFragmentPainterBase::PaintBackgroundBorderShadow(
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset) {
  DCHECK(paint_info.phase == PaintPhase::kForeground);
  if (inline_box_fragment_.Style().Visibility() != EVisibility::kVisible)
    return;

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

  DrawingRecorder recorder(paint_info.context, display_item_client,
                           DisplayItem::kBoxDecorationBackground);

  PhysicalRect frame_rect = inline_box_fragment_.LocalRect();
  PhysicalOffset adjusted_paint_offset = paint_offset;

  PhysicalRect adjusted_frame_rect(adjusted_paint_offset, frame_rect.size);

  DCHECK(inline_box_fragment_.GetLayoutObject());
  const LayoutObject& layout_object = *inline_box_fragment_.GetLayoutObject();
  bool object_has_multiple_boxes =
      inline_box_paint_fragment_ ? HasMultiplePaintFragments(layout_object)
                                 : HasMultipleFragmentItems(layout_object);

  // TODO(eae): Switch to LayoutNG version of BackgroundImageGeometry.
  BackgroundImageGeometry geometry(*static_cast<const LayoutBoxModelObject*>(
      inline_box_fragment_.GetLayoutObject()));
  // TODO(kojii): not applicable for line box
  NGBoxFragmentPainter box_painter(
      To<NGPhysicalBoxFragment>(inline_box_fragment_),
      inline_box_paint_fragment_);
  const NGBorderEdges& border_edges = BorderEdges();
  PaintBoxDecorationBackground(box_painter, paint_info, paint_offset,
                               adjusted_frame_rect, geometry,
                               object_has_multiple_boxes,
                               border_edges.line_left, border_edges.line_right);
}

void NGLineBoxFragmentPainter::PaintBackgroundBorderShadow(
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset) {
  DCHECK_EQ(paint_info.phase, PaintPhase::kForeground);
  DCHECK_EQ(inline_box_fragment_.Type(), NGPhysicalFragment::kFragmentLineBox);
  DCHECK(NeedsPaint(inline_box_fragment_));

  if (line_style_ == style_ ||
      line_style_.Visibility() != EVisibility::kVisible)
    return;

  const DisplayItemClient& display_item_client = GetDisplayItemClient();
  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, display_item_client,
          DisplayItem::kBoxDecorationBackground))
    return;

  DrawingRecorder recorder(paint_info.context, display_item_client,
                           DisplayItem::kBoxDecorationBackground);

  // Compute the content box for the `::first-line` box. It's different from
  // fragment size because the height of line box includes `line-height` while
  // the height of inline box does not. The box "behaves similar to that of an
  // inline-level element".
  // https://drafts.csswg.org/css-pseudo-4/#first-line-styling
  const NGPhysicalLineBoxFragment& line_box = PhysicalFragment();
  const NGLineHeightMetrics line_metrics = line_box.Metrics();
  const NGLineHeightMetrics text_metrics = NGLineHeightMetrics(line_style_);
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

  const LayoutBlockFlow& layout_block_flow =
      *To<LayoutBlockFlow>(block_fragment_.GetLayoutObject());
  BackgroundImageGeometry geometry(layout_block_flow);
  NGBoxFragmentPainter box_painter(block_fragment_, block_paint_fragment_);
  PaintBoxDecorationBackground(
      box_painter, paint_info, paint_offset, rect, geometry,
      /*object_has_multiple_boxes*/ false, /*include_logical_left_edge*/ true,
      /*include_logical_right_edge*/ true);
}

void NGInlineBoxFragmentPainterBase::ComputeFragmentOffsetOnLine(
    TextDirection direction,
    LayoutUnit* offset_on_line,
    LayoutUnit* total_width) const {
  WritingMode writing_mode = inline_box_fragment_.Style().GetWritingMode();
  NGPaintFragment::FragmentRange fragments =
      inline_box_paint_fragment_->InlineFragmentsFor(
          inline_box_fragment_.GetLayoutObject());

  LayoutUnit before;
  LayoutUnit after;
  bool before_self = true;
  for (auto iter = fragments.begin(); iter != fragments.end(); ++iter) {
    if (*iter == inline_box_paint_fragment_) {
      before_self = false;
      continue;
    }
    if (before_self)
      before += NGFragment(writing_mode, iter->PhysicalFragment()).InlineSize();
    else
      after += NGFragment(writing_mode, iter->PhysicalFragment()).InlineSize();
  }

  NGFragment logical_fragment(writing_mode, inline_box_fragment_);
  *total_width = before + after + logical_fragment.InlineSize();

  // We're iterating over the fragments in physical order before so we need to
  // swap before and after for RTL.
  *offset_on_line = direction == TextDirection::kLtr ? before : after;
}

PhysicalRect NGInlineBoxFragmentPainterBase::PaintRectForImageStrip(
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

static PhysicalRect NGClipRectForNinePieceImageStrip(
    const ComputedStyle& style,
    const NGBorderEdges& border_edges,
    const NinePieceImage& image,
    const PhysicalRect& paint_rect) {
  PhysicalRect clip_rect(paint_rect);
  LayoutRectOutsets outsets = style.ImageOutsets(image);
  if (style.IsHorizontalWritingMode()) {
    clip_rect.SetY(paint_rect.Y() - outsets.Top());
    clip_rect.SetHeight(paint_rect.Height() + outsets.Top() + outsets.Bottom());
    if (border_edges.line_left) {
      clip_rect.SetX(paint_rect.X() - outsets.Left());
      clip_rect.SetWidth(paint_rect.Width() + outsets.Left());
    }
    if (border_edges.line_right)
      clip_rect.SetWidth(clip_rect.Width() + outsets.Right());
  } else {
    clip_rect.SetX(paint_rect.X() - outsets.Left());
    clip_rect.SetWidth(paint_rect.Width() + outsets.Left() + outsets.Right());
    if (border_edges.line_left) {
      clip_rect.SetY(paint_rect.Y() - outsets.Top());
      clip_rect.SetHeight(paint_rect.Height() + outsets.Top());
    }
    if (border_edges.line_right)
      clip_rect.SetHeight(clip_rect.Height() + outsets.Bottom());
  }
  return clip_rect;
}

InlineBoxPainterBase::BorderPaintingType
NGInlineBoxFragmentPainterBase::GetBorderPaintType(
    const PhysicalRect& adjusted_frame_rect,
    IntRect& adjusted_clip_rect,
    bool object_has_multiple_boxes) const {
  const ComputedStyle& style = inline_box_fragment_.Style();
  if (!style.HasBorderDecoration())
    return kDontPaintBorders;

  const NinePieceImage& border_image = style.BorderImage();
  StyleImage* border_image_source = border_image.GetImage();
  bool has_border_image =
      border_image_source && border_image_source->CanRender();
  if (has_border_image && !border_image_source->IsLoaded())
    return kDontPaintBorders;

  // The simple case is where we either have no border image or we are the
  // only box for this object.  In those cases only a single call to draw is
  // required.
  if (!has_border_image || !object_has_multiple_boxes) {
    adjusted_clip_rect = PixelSnappedIntRect(adjusted_frame_rect);
    return kPaintBordersWithoutClip;
  }

  // We have a border image that spans multiple lines.
  adjusted_clip_rect = PixelSnappedIntRect(NGClipRectForNinePieceImageStrip(
      style, BorderEdges(), border_image, adjusted_frame_rect));
  return kPaintBordersWithClip;
}

void NGInlineBoxFragmentPainterBase::PaintNormalBoxShadow(
    const PaintInfo& info,
    const ComputedStyle& s,
    const PhysicalRect& paint_rect) {
  const NGBorderEdges& border_edges = BorderEdges();
  BoxPainterBase::PaintNormalBoxShadow(
      info, paint_rect, s, border_edges.line_left, border_edges.line_right);
}

void NGInlineBoxFragmentPainterBase::PaintInsetBoxShadow(
    const PaintInfo& info,
    const ComputedStyle& s,
    const PhysicalRect& paint_rect) {
  const NGBorderEdges& border_edges = BorderEdges();
  BoxPainterBase::PaintInsetBoxShadowWithBorderRect(
      info, paint_rect, s, border_edges.line_left, border_edges.line_right);
}

// Paint all fragments for the |layout_inline|. This function is used only for
// self-painting |LayoutInline|.
void NGInlineBoxFragmentPainter::PaintAllFragments(
    const LayoutInline& layout_inline,
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset) {
  // TODO(kojii): If the block flow is dirty, children of these fragments
  // maybe already deleted. crbug.com/963103
  const LayoutBlockFlow* block_flow =
      layout_inline.RootInlineFormattingContext();
  if (UNLIKELY(block_flow->NeedsLayout()))
    return;

  if (!RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled()) {
    auto fragments = NGPaintFragment::InlineFragmentsFor(&layout_inline);

    // TODO(kojii): The root of this inline formatting context should have a
    // PaintFragment, but it looks like there's a case it doesn't stand.
    // crbug.com/969096
    CHECK(block_flow->PaintFragment() || fragments.IsEmpty());

    for (const NGPaintFragment* fragment : fragments) {
      PhysicalOffset child_offset = paint_offset +
                                    fragment->InlineOffsetToContainerBox() -
                                    fragment->Offset();
      DCHECK(fragment->PhysicalFragment().IsBox());
      NGInlineBoxFragmentPainter(*fragment).Paint(paint_info, child_offset);
    }
    return;
  }

  DCHECK(layout_inline.ShouldCreateBoxFragment());
  NGInlineCursor cursor(*block_flow);
  cursor.MoveTo(layout_inline);
  for (; cursor; cursor.MoveToNextForSameLayoutObject()) {
    const NGFragmentItem* item = cursor.CurrentItem();
    DCHECK(item);
    const NGPhysicalBoxFragment* box_fragment = item->BoxFragment();
    DCHECK(box_fragment);
    NGInlineCursor descendants = cursor.CursorForDescendants();
    NGInlineBoxFragmentPainter(*item, *box_fragment, &descendants)
        .Paint(paint_info, paint_offset);
  }
}

}  // namespace blink
