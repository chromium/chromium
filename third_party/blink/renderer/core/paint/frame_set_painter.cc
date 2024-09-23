// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/frame_set_painter.h"

#include "third_party/blink/renderer/core/layout/frame_set_layout_data.h"
#include "third_party/blink/renderer/core/paint/box_painter.h"
#include "third_party/blink/renderer/core/paint/box_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"

namespace blink {

namespace {

constexpr Color kBorderStartEdgeColor = Color::FromRGB(170, 170, 170);
constexpr Color kBorderEndEdgeColor = Color::FromRGB(0, 0, 0);
constexpr Color kBorderFillColor = Color::FromRGB(208, 208, 208);

bool ShouldPaintBorderAfter(const Vector<bool>& allow_border,
                            wtf_size_t index) {
  // Should not paint a border after the last frame along the axis.
  return index + 1 < allow_border.size() - 1 && allow_border[index + 1];
}

}  // namespace

void FrameSetPainter::PaintObject(const PaintInfo& paint_info,
                                  const PhysicalOffset& paint_offset) {
  if (paint_info.phase != PaintPhase::kForeground)
    return;

  if (box_fragment_.Children().size() == 0)
    return;

  if (box_fragment_.Style().UsedVisibility() != EVisibility::kVisible) {
    return;
  }

  PaintInfo paint_info_for_descendants = paint_info.ForDescendants();
  PaintChildren(paint_info_for_descendants);

  PaintBorders(paint_info, paint_offset);
}

void FrameSetPainter::PaintChildren(const PaintInfo& paint_info) {
  if (paint_info.DescendantPaintingBlocked())
    return;

  for (const PhysicalFragmentLink& link : box_fragment_.Children()) {
    const PhysicalFragment& child_fragment = *link;
    if (child_fragment.HasSelfPaintingLayer())
      continue;
    if (To<PhysicalBoxFragment>(child_fragment).CanTraverse()) {
      BoxFragmentPainter(To<PhysicalBoxFragment>(child_fragment))
          .Paint(paint_info);
    } else {
      child_fragment.GetLayoutObject()->Paint(paint_info);
    }
  }
}

void FrameSetPainter::PaintBorders(const PaintInfo& paint_info,
                                   const PhysicalOffset& paint_offset) {
  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, display_item_client_, paint_info.phase))
    return;

  DrawingRecorder recorder(
      paint_info.context, display_item_client_, paint_info.phase,
      BoxPainter(*To<LayoutBox>(box_fragment_.GetLayoutObject()))
          .VisualRect(paint_offset));

  const FrameSetLayoutData* layout_data = box_fragment_.GetFrameSetLayoutData();
  const LayoutUnit border_thickness = LayoutUnit(layout_data->border_thickness);
  if (border_thickness <= 0)
    return;

  const ComputedStyle& style = box_fragment_.Style();
  Color border_fill_color =
      layout_data->has_border_color
          ? style.VisitedDependentColor(GetCSSPropertyBorderLeftColor())
          : kBorderFillColor;
  auto auto_dark_mode =
      PaintAutoDarkMode(style, DarkModeFilter::ElementRole::kBackground);
  size_t children_count = box_fragment_.Children().size();
  const Vector<LayoutUnit>& row_sizes = layout_data->row_sizes;
  const Vector<LayoutUnit>& col_sizes = layout_data->col_sizes;
  LayoutUnit y;
  for (wtf_size_t row = 0; row < row_sizes.size(); ++row) {
    LayoutUnit x;
    for (wtf_size_t col = 0; col < col_sizes.size(); ++col) {
      x += col_sizes[col];
      if (ShouldPaintBorderAfter(layout_data->col_allow_border, col)) {
        gfx::Rect rect = ToPixelSnappedRect(
            PhysicalRect(paint_offset.left + x, paint_offset.top + y,
                         border_thickness, box_fragment_.Size().height - y));
        PaintColumnBorder(paint_info, rect, border_fill_color, auto_dark_mode);
        x += border_thickness;
      }
      if (--children_count == 0)
        return;
    }
    y += row_sizes[row];
    if (ShouldPaintBorderAfter(layout_data->row_allow_border, row)) {
      gfx::Rect rect = ToPixelSnappedRect(
          PhysicalRect(paint_offset.left, paint_offset.top + y,
                       box_fragment_.Size().width, border_thickness));
      PaintRowBorder(paint_info, rect, border_fill_color, auto_dark_mode);
      y += border_thickness;
    }
  }
}

void FrameSetPainter::PaintRowBorder(const PaintInfo& paint_info,
                                     const gfx::Rect& border_rect,
                                     const Color& fill_color,
                                     const AutoDarkMode& auto_dark_mode) {
  // Fill first.
  GraphicsContext& context = paint_info.context;
  context.FillRect(border_rect, fill_color, auto_dark_mode);

  // Now stroke the edges but only if we have enough room to paint both edges
  // with a little bit of the fill color showing through.
  if (border_rect.height() < 3)
    return;
  context.FillRect(
      gfx::Rect(border_rect.origin(), gfx::Size(border_rect.width(), 1)),
      kBorderStartEdgeColor, auto_dark_mode);
  context.FillRect(gfx::Rect(border_rect.x(), border_rect.bottom() - 1,
                             border_rect.width(), 1),
                   kBorderEndEdgeColor, auto_dark_mode);
}

void FrameSetPainter::PaintColumnBorder(const PaintInfo& paint_info,
                                        const gfx::Rect& border_rect,
                                        const Color& fill_color,
                                        const AutoDarkMode& auto_dark_mode) {
  if (!paint_info.GetCullRect().Intersects(border_rect))
    return;

  // Fill first.
  GraphicsContext& context = paint_info.context;
  context.FillRect(border_rect, fill_color, auto_dark_mode);

  // Now stroke the edges but only if we have enough room to paint both edges
  // with a little bit of the fill color showing through.
  if (border_rect.width() < 3)
    return;
  context.FillRect(
      gfx::Rect(border_rect.origin(), gfx::Size(1, border_rect.height())),
      kBorderStartEdgeColor, auto_dark_mode);
  context.FillRect(gfx::Rect(border_rect.right() - 1, border_rect.y(), 1,
                             border_rect.height()),
                   kBorderEndEdgeColor, auto_dark_mode);
}

}  // namespace blink
