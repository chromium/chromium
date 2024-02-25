// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/mathml_painter.h"

#include "third_party/blink/renderer/core/layout/mathml/math_layout_utils.h"
#include "third_party/blink/renderer/core/mathml/mathml_radical_element.h"
#include "third_party/blink/renderer/core/paint/box_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/platform/fonts/text_fragment_paint_info.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"

namespace blink {

void MathMLPainter::PaintBar(const PaintInfo& info,
                             const PhysicalRect& bar_rect) {
  gfx::Rect snapped_bar_rect = ToPixelSnappedRect(bar_rect);
  if (snapped_bar_rect.IsEmpty()) {
    return;
  }
  // The (vertical) origin of `snapped_bar_rect` is now at the mid-point of the
  // bar. Shift up by half the height to produce the corresponding rectangle.
  snapped_bar_rect -= gfx::Vector2d(0, snapped_bar_rect.height() / 2);
  const ComputedStyle& style = box_fragment_.Style();
  info.context.FillRect(
      snapped_bar_rect, style.VisitedDependentColor(GetCSSPropertyColor()),
      PaintAutoDarkMode(style, DarkModeFilter::ElementRole::kForeground));
}

void MathMLPainter::PaintStretchyOrLargeOperator(const PaintInfo& info,
                                                 PhysicalOffset paint_offset) {
  const ComputedStyle& style = box_fragment_.Style();
  const MathMLPaintInfo& parameters = box_fragment_.GetMathMLPaintInfo();
  UChar operator_character = parameters.operator_character;
  TextFragmentPaintInfo text_fragment_paint_info = {
      StringView(&operator_character, 1), 0, 1,
      parameters.operator_shape_result_view.Get()};
  GraphicsContextStateSaver state_saver(info.context);
  info.context.SetFillColor(style.VisitedDependentColor(GetCSSPropertyColor()));
  AutoDarkMode auto_dark_mode(
      PaintAutoDarkMode(style, DarkModeFilter::ElementRole::kForeground));
  info.context.DrawText(style.GetFont(), text_fragment_paint_info,
                        gfx::PointF(paint_offset), kInvalidDOMNodeId,
                        auto_dark_mode);
}

void MathMLPainter::PaintFractionBar(const PaintInfo& info,
                                     PhysicalOffset paint_offset) {
  DCHECK(box_fragment_.Style().IsHorizontalWritingMode());
  const ComputedStyle& style = box_fragment_.Style();
  LayoutUnit line_thickness = FractionLineThickness(style);
  if (!line_thickness)
    return;
  LayoutUnit axis_height = MathAxisHeight(style);
  if (auto baseline = box_fragment_.FirstBaseline()) {
    auto borders = box_fragment_.Borders();
    auto padding = box_fragment_.Padding();
    PhysicalRect bar_rect = {
        borders.left + padding.left, *baseline - axis_height,
        box_fragment_.Size().width - borders.HorizontalSum() -
            padding.HorizontalSum(),
        line_thickness};
    bar_rect.Move(paint_offset);
    PaintBar(info, bar_rect);
  }
}

void MathMLPainter::PaintOperator(const PaintInfo& info,
                                  PhysicalOffset paint_offset) {
  const ComputedStyle& style = box_fragment_.Style();
  const MathMLPaintInfo& parameters = box_fragment_.GetMathMLPaintInfo();
  LogicalOffset offset(LayoutUnit(), parameters.operator_ascent);
  PhysicalOffset physical_offset = offset.ConvertToPhysical(
      style.GetWritingDirection(),
      PhysicalSize(box_fragment_.Size().width, box_fragment_.Size().height),
      PhysicalSize(parameters.operator_inline_size,
                   parameters.operator_ascent + parameters.operator_descent));
  auto borders = box_fragment_.Borders();
  auto padding = box_fragment_.Padding();
  physical_offset.left += borders.left + padding.left;
  physical_offset.top += borders.top + padding.top;

  // TODO(http://crbug.com/1124301): MathOperatorLayoutAlgorithm::Layout
  // passes the operator's inline size but this does not match the width of the
  // box fragment, which relies on the min-max sizes instead. Shift the paint
  // offset to work around that issue, splitting the size error symmetrically.
  DCHECK(box_fragment_.Style().IsHorizontalWritingMode());
  physical_offset.left +=
      (box_fragment_.Size().width - borders.HorizontalSum() -
       padding.HorizontalSum() - parameters.operator_inline_size) /
      2;

  PaintStretchyOrLargeOperator(info, paint_offset + physical_offset);
}

void MathMLPainter::PaintRadicalSymbol(const PaintInfo& info,
                                       PhysicalOffset paint_offset) {
  LayoutUnit base_child_width;
  LayoutUnit base_child_ascent;
  if (box_fragment_.Children().size() > 0) {
    const auto& base_child =
        To<PhysicalBoxFragment>(*box_fragment_.Children()[0]);
    base_child_width = base_child.Size().width;
    base_child_ascent =
        base_child.FirstBaseline().value_or(base_child.Size().height);
  }

  const MathMLPaintInfo& parameters = box_fragment_.GetMathMLPaintInfo();
  DCHECK(box_fragment_.Style().IsHorizontalWritingMode());

  // Paint the vertical symbol.
  const ComputedStyle& style = box_fragment_.Style();
  bool has_index =
      To<MathMLRadicalElement>(box_fragment_.GetNode())->HasIndex();
  auto vertical = GetRadicalVerticalParameters(style, has_index);

  auto radical_base_ascent =
      base_child_ascent + parameters.radical_base_margins.inline_start;
  LayoutUnit block_offset =
      box_fragment_.FirstBaseline().value_or(box_fragment_.Size().height) -
      vertical.vertical_gap - radical_base_ascent;

  auto borders = box_fragment_.Borders();
  auto padding = box_fragment_.Padding();
  LayoutUnit inline_offset = borders.left + padding.left;
  inline_offset += *parameters.radical_operator_inline_offset;

  LogicalOffset radical_symbol_offset(
      inline_offset, block_offset + parameters.operator_ascent);
  auto radical_symbol_physical_offset = radical_symbol_offset.ConvertToPhysical(
      style.GetWritingDirection(),
      PhysicalSize(box_fragment_.Size().width, box_fragment_.Size().height),
      PhysicalSize(parameters.operator_ascent,
                   parameters.operator_ascent + parameters.operator_descent));
  PaintStretchyOrLargeOperator(info,
                               paint_offset + radical_symbol_physical_offset);

  // Paint the horizontal overbar.
  LayoutUnit rule_thickness = vertical.rule_thickness;
  if (!rule_thickness)
    return;
  LayoutUnit base_width =
      base_child_width + parameters.radical_base_margins.InlineSum();
  LogicalOffset bar_offset =
      LogicalOffset(inline_offset, block_offset) +
      LogicalSize(parameters.operator_inline_size, LayoutUnit());
  auto bar_physical_offset = bar_offset.ConvertToPhysical(
      style.GetWritingDirection(), box_fragment_.Size(),
      PhysicalSize(base_width, rule_thickness));
  PhysicalRect bar_rect = {bar_physical_offset.left, bar_physical_offset.top,
                           base_width, rule_thickness};
  bar_rect.Move(paint_offset);
  PaintBar(info, bar_rect);
}

void MathMLPainter::Paint(const PaintInfo& info, PhysicalOffset paint_offset) {
  const DisplayItemClient& display_item_client =
      *box_fragment_.GetLayoutObject();
  if (DrawingRecorder::UseCachedDrawingIfPossible(
          info.context, display_item_client, info.phase))
    return;
  DrawingRecorder recorder(
      info.context, display_item_client, info.phase,
      BoxFragmentPainter(box_fragment_).VisualRect(paint_offset));

  // Fraction
  if (box_fragment_.IsMathMLFraction()) {
    PaintFractionBar(info, paint_offset);
    return;
  }

  // Radical symbol
  if (box_fragment_.GetMathMLPaintInfo().IsRadicalOperator()) {
    PaintRadicalSymbol(info, paint_offset);
    return;
  }

  // Operator
  PaintOperator(info, paint_offset);
}

}  // namespace blink
