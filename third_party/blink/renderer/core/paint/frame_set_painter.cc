// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/frame_set_painter.h"

#include "third_party/blink/renderer/core/display_lock/display_lock_context.h"
#include "third_party/blink/renderer/core/html/html_frame_set_element.h"
#include "third_party/blink/renderer/core/layout/layout_frame_set.h"
#include "third_party/blink/renderer/core/paint/box_painter.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/scoped_paint_state.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"

namespace blink {

static Color BorderStartEdgeColor() {
  return Color(170, 170, 170);
}

static Color BorderEndEdgeColor() {
  return Color::kBlack;
}

static Color BorderFillColor() {
  return Color(208, 208, 208);
}

void FrameSetPainter::PaintColumnBorder(const PaintInfo& paint_info,
                                        const gfx::Rect& border_rect) {
  if (!paint_info.GetCullRect().Intersects(border_rect))
    return;

  // FIXME: We should do something clever when borders from distinct framesets
  // meet at a join.

  AutoDarkMode auto_dark_mode(PaintAutoDarkMode(
      layout_frame_set_.StyleRef(), DarkModeFilter::ElementRole::kBorder));

  // Fill first.
  GraphicsContext& context = paint_info.context;
  context.FillRect(
      border_rect,
      layout_frame_set_.FrameSet()->HasBorderColor()
          ? layout_frame_set_.ResolveColor(GetCSSPropertyBorderLeftColor())
          : BorderFillColor(),
      auto_dark_mode);

  // Now stroke the edges but only if we have enough room to paint both edges
  // with a little bit of the fill color showing through.
  if (border_rect.width() >= 3) {
    context.FillRect(
        gfx::Rect(border_rect.origin(), gfx::Size(1, border_rect.height())),
        BorderStartEdgeColor(), auto_dark_mode);
    context.FillRect(
        gfx::Rect(gfx::Point(border_rect.right() - 1, border_rect.y()),
                  gfx::Size(1, border_rect.height())),
        BorderEndEdgeColor(), auto_dark_mode);
  }
}

void FrameSetPainter::PaintRowBorder(const PaintInfo& paint_info,
                                     const gfx::Rect& border_rect) {
  // FIXME: We should do something clever when borders from distinct framesets
  // meet at a join.

  AutoDarkMode auto_dark_mode(PaintAutoDarkMode(
      layout_frame_set_.StyleRef(), DarkModeFilter::ElementRole::kBorder));

  // Fill first.
  GraphicsContext& context = paint_info.context;
  context.FillRect(
      border_rect,
      layout_frame_set_.FrameSet()->HasBorderColor()
          ? layout_frame_set_.ResolveColor(GetCSSPropertyBorderLeftColor())
          : BorderFillColor(),
      auto_dark_mode);

  // Now stroke the edges but only if we have enough room to paint both edges
  // with a little bit of the fill color showing through.
  if (border_rect.height() >= 3) {
    context.FillRect(
        gfx::Rect(border_rect.origin(), gfx::Size(border_rect.width(), 1)),
        BorderStartEdgeColor(), auto_dark_mode);
    context.FillRect(
        gfx::Rect(gfx::Point(border_rect.x(), border_rect.bottom() - 1),
                  gfx::Size(border_rect.width(), 1)),
        BorderEndEdgeColor(), auto_dark_mode);
  }
}

static bool ShouldPaintBorderAfter(const LayoutFrameSet::GridAxis& axis,
                                   wtf_size_t index) {
  // Should not paint a border after the last frame along the axis.
  return index + 1 < axis.sizes_.size() && axis.allow_border_[index + 1];
}

void FrameSetPainter::PaintBorders(const PaintInfo& paint_info,
                                   const PhysicalOffset& paint_offset) {
  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, layout_frame_set_, paint_info.phase))
    return;

  BoxDrawingRecorder recorder(paint_info.context, layout_frame_set_,
                              paint_info.phase, paint_offset);

  LayoutUnit border_thickness(
      layout_frame_set_.FrameSet()->Border(layout_frame_set_.StyleRef()));
  if (!border_thickness)
    return;

  LayoutObject* child = layout_frame_set_.FirstChild();
  wtf_size_t rows = layout_frame_set_.Rows().sizes_.size();
  wtf_size_t cols = layout_frame_set_.Columns().sizes_.size();
  LayoutUnit y_pos;
  for (wtf_size_t r = 0; r < rows; r++) {
    LayoutUnit x_pos;
    for (wtf_size_t c = 0; c < cols; c++) {
      x_pos += layout_frame_set_.Columns().sizes_[c];
      if (ShouldPaintBorderAfter(layout_frame_set_.Columns(), c)) {
        PaintColumnBorder(
            paint_info,
            ToPixelSnappedRect(PhysicalRect(
                paint_offset.left + x_pos, paint_offset.top + y_pos,
                border_thickness, layout_frame_set_.Size().Height() - y_pos)));
        x_pos += border_thickness;
      }
      child = child->NextSibling();
      if (!child)
        return;
    }
    y_pos += layout_frame_set_.Rows().sizes_[r];
    if (ShouldPaintBorderAfter(layout_frame_set_.Rows(), r)) {
      PaintRowBorder(paint_info,
                     ToPixelSnappedRect(PhysicalRect(
                         paint_offset.left, paint_offset.top + y_pos,
                         layout_frame_set_.Size().Width(), border_thickness)));
      y_pos += border_thickness;
    }
  }
}

void FrameSetPainter::PaintChildren(const PaintInfo& paint_info) {
  if (paint_info.DescendantPaintingBlocked())
    return;

  // Paint only those children that fit in the grid.
  // Remaining frames are "hidden".
  // See also LayoutFrameSet::positionFrames.
  LayoutObject* child = layout_frame_set_.FirstChild();
  size_t rows = layout_frame_set_.Rows().sizes_.size();
  size_t cols = layout_frame_set_.Columns().sizes_.size();
  for (size_t r = 0; r < rows; r++) {
    for (size_t c = 0; c < cols; c++) {
      // Self-painting layers are painted during the PaintLayer paint recursion,
      // not LayoutObject.
      if (!child->IsBoxModelObject() ||
          !To<LayoutBoxModelObject>(child)->HasSelfPaintingLayer())
        child->Paint(paint_info);
      child = child->NextSibling();
      if (!child)
        return;
    }
  }
}

void FrameSetPainter::Paint(const PaintInfo& paint_info) {
  if (paint_info.phase != PaintPhase::kForeground)
    return;

  if (!layout_frame_set_.FirstChild())
    return;

  if (layout_frame_set_.StyleRef().Visibility() != EVisibility::kVisible)
    return;

  ScopedPaintState paint_state(layout_frame_set_, paint_info);
  const auto& local_paint_info = paint_state.GetPaintInfo();
  PaintChildren(local_paint_info);
  PaintBorders(local_paint_info, paint_state.PaintOffset());
}

}  // namespace blink
