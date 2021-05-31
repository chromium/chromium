// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/text_painter.h"

#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_api_shim.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_item.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_text_combine.h"
#include "third_party/blink/renderer/core/layout/text_decoration_offset_base.h"
#include "third_party/blink/renderer/core/paint/applied_decoration_painter.h"
#include "third_party/blink/renderer/core/paint/box_painter.h"
#include "third_party/blink/renderer/core/paint/highlight_painting_utils.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/shadow_list.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/text_run_paint_info.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/text/text_run.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"

namespace blink {

void TextPainter::Paint(unsigned start_offset,
                        unsigned end_offset,
                        unsigned length,
                        const TextPaintStyle& text_style,
                        DOMNodeId node_id) {
  GraphicsContextStateSaver state_saver(graphics_context_, false);
  UpdateGraphicsContext(text_style, state_saver);
  if (combined_text_) {
    graphics_context_.Save();
    combined_text_->TransformToInlineCoordinates(graphics_context_,
                                                 text_frame_rect_);
    PaintInternal<kPaintText>(start_offset, end_offset, length, node_id);
    graphics_context_.Restore();
  } else {
    PaintInternal<kPaintText>(start_offset, end_offset, length, node_id);
  }

  if (!emphasis_mark_.IsEmpty()) {
    if (combined_text_) {
      graphics_context_.ConcatCTM(Rotation(text_frame_rect_, kClockwise));
      PaintEmphasisMarkForCombinedText(text_style,
                                       combined_text_->OriginalFont());
      graphics_context_.ConcatCTM(
          Rotation(text_frame_rect_, kCounterclockwise));
    } else {
      if (text_style.emphasis_mark_color != text_style.fill_color)
        graphics_context_.SetFillColor(text_style.emphasis_mark_color);
      PaintInternal<kPaintEmphasisMark>(start_offset, end_offset, length,
                                        node_id);
    }
  }
}

void TextPainter::PaintDecorationsExceptLineThrough(
    const TextDecorationOffsetBase& decoration_offset,
    TextDecorationInfo& decoration_info,
    const PaintInfo& paint_info,
    const Vector<AppliedTextDecoration>& decorations,
    const TextPaintStyle& text_style,
    bool* has_line_through_decoration) {
  GraphicsContext& context = paint_info.context;
  GraphicsContextStateSaver state_saver(context);
  UpdateGraphicsContext(context, text_style, horizontal_, state_saver);

  if (combined_text_)
    context.ConcatCTM(Rotation(text_frame_rect_, kClockwise));

  // text-underline-position may flip underline and overline.
  ResolvedUnderlinePosition underline_position =
      decoration_info.UnderlinePosition();
  bool flip_underline_and_overline = false;
  if (underline_position == ResolvedUnderlinePosition::kOver) {
    flip_underline_and_overline = true;
    underline_position = ResolvedUnderlinePosition::kUnder;
  }

  for (size_t applied_decoration_index = 0;
       applied_decoration_index < decorations.size();
       ++applied_decoration_index) {
    const AppliedTextDecoration& decoration =
        decorations[applied_decoration_index];
    TextDecoration lines = decoration.Lines();
    bool has_underline = EnumHasFlags(lines, TextDecoration::kUnderline);
    bool has_overline = EnumHasFlags(lines, TextDecoration::kOverline);
    if (flip_underline_and_overline)
      std::swap(has_underline, has_overline);

    decoration_info.SetDecorationIndex(applied_decoration_index);

    float resolved_thickness = decoration_info.ResolvedThickness();
    context.SetStrokeThickness(resolved_thickness);

    if (has_underline && decoration_info.FontData()) {
      // Don't apply text-underline-offset to overline.
      Length line_offset =
          flip_underline_and_overline ? Length() : decoration.UnderlineOffset();

      const int paint_underline_offset =
          decoration_offset.ComputeUnderlineOffset(
              underline_position, decoration_info.Style().ComputedFontSize(),
              decoration_info.FontData()->GetFontMetrics(), line_offset,
              resolved_thickness);
      decoration_info.SetPerLineData(
          TextDecoration::kUnderline, paint_underline_offset,
          TextDecorationInfo::DoubleOffsetFromThickness(resolved_thickness), 1);
      PaintDecorationUnderOrOverLine(context, decoration_info,
                                     TextDecoration::kUnderline);
    }

    if (has_overline && decoration_info.FontData()) {
      // Don't apply text-underline-offset to overline.
      Length line_offset =
          flip_underline_and_overline ? decoration.UnderlineOffset() : Length();

      FontVerticalPositionType position =
          flip_underline_and_overline ? FontVerticalPositionType::TopOfEmHeight
                                      : FontVerticalPositionType::TextTop;
      const int paint_overline_offset =
          decoration_offset.ComputeUnderlineOffsetForUnder(
              line_offset, decoration_info.Style().ComputedFontSize(),
              resolved_thickness, position);
      decoration_info.SetPerLineData(
          TextDecoration::kOverline, paint_overline_offset,
          -TextDecorationInfo::DoubleOffsetFromThickness(resolved_thickness),
          1);
      PaintDecorationUnderOrOverLine(context, decoration_info,
                                     TextDecoration::kOverline);
    }

    // We could instead build a vector of the TextDecoration instances needing
    // line-through but this is a rare case so better to avoid vector overhead.
    *has_line_through_decoration |=
        EnumHasFlags(lines, TextDecoration::kLineThrough);
  }

  // Restore rotation as needed.
  if (combined_text_)
    context.ConcatCTM(Rotation(text_frame_rect_, kCounterclockwise));
}

void TextPainter::PaintDecorationsOnlyLineThrough(
    TextDecorationInfo& decoration_info,
    const PaintInfo& paint_info,
    const Vector<AppliedTextDecoration>& decorations,
    const TextPaintStyle& text_style) {
  GraphicsContext& context = paint_info.context;
  GraphicsContextStateSaver state_saver(context);
  UpdateGraphicsContext(context, text_style, horizontal_, state_saver);

  if (combined_text_)
    context.ConcatCTM(Rotation(text_frame_rect_, kClockwise));

  for (size_t applied_decoration_index = 0;
       applied_decoration_index < decorations.size();
       ++applied_decoration_index) {
    const AppliedTextDecoration& decoration =
        decorations[applied_decoration_index];
    TextDecoration lines = decoration.Lines();
    if (EnumHasFlags(lines, TextDecoration::kLineThrough)) {
      decoration_info.SetDecorationIndex(applied_decoration_index);

      float resolved_thickness = decoration_info.ResolvedThickness();
      context.SetStrokeThickness(resolved_thickness);

      // For increased line thickness, the line-through decoration needs to grow
      // in both directions from its origin, subtract half the thickness to keep
      // it centered at the same origin.
      const float line_through_offset =
          2 * decoration_info.Baseline() / 3 - resolved_thickness / 2;
      // Floor double_offset in order to avoid double-line gap to appear
      // of different size depending on position where the double line
      // is drawn because of rounding downstream in
      // GraphicsContext::DrawLineForText.
      decoration_info.SetPerLineData(
          TextDecoration::kLineThrough, line_through_offset,
          floorf(TextDecorationInfo::DoubleOffsetFromThickness(
              resolved_thickness)),
          0);
      AppliedDecorationPainter decoration_painter(context, decoration_info,
                                                  TextDecoration::kLineThrough);
      // No skip: ink for line-through,
      // compare https://github.com/w3c/csswg-drafts/issues/711
      decoration_painter.Paint();
    }
  }

  // Restore rotation as needed.
  if (combined_text_)
    context.ConcatCTM(Rotation(text_frame_rect_, kCounterclockwise));
}

template <TextPainter::PaintInternalStep step>
void TextPainter::PaintInternalRun(TextRunPaintInfo& text_run_paint_info,
                                   unsigned from,
                                   unsigned to,
                                   DOMNodeId node_id) {
  DCHECK(from <= text_run_paint_info.run.length());
  DCHECK(to <= text_run_paint_info.run.length());

  text_run_paint_info.from = from;
  text_run_paint_info.to = to;

  if (step == kPaintEmphasisMark) {
    graphics_context_.DrawEmphasisMarks(
        font_, text_run_paint_info, emphasis_mark_,
        FloatPoint(text_origin_) + IntSize(0, emphasis_mark_offset_));
  } else {
    DCHECK(step == kPaintText);
    graphics_context_.DrawText(font_, text_run_paint_info,
                               FloatPoint(text_origin_), node_id);
  }
}

template <TextPainter::PaintInternalStep Step>
void TextPainter::PaintInternal(unsigned start_offset,
                                unsigned end_offset,
                                unsigned truncation_point,
                                DOMNodeId node_id) {
  TextRunPaintInfo text_run_paint_info(run_);
  if (start_offset <= end_offset) {
    PaintInternalRun<Step>(text_run_paint_info, start_offset, end_offset,
                           node_id);
  } else {
    if (end_offset > 0) {
      PaintInternalRun<Step>(text_run_paint_info, ellipsis_offset_, end_offset,
                             node_id);
    }
    if (start_offset < truncation_point) {
      PaintInternalRun<Step>(text_run_paint_info, start_offset,
                             truncation_point, node_id);
    }
  }
}

void TextPainter::ClipDecorationsStripe(float upper,
                                        float stripe_width,
                                        float dilation) {
  TextRunPaintInfo text_run_paint_info(run_);
  if (!run_.length())
    return;

  Vector<Font::TextIntercept> text_intercepts;
  font_.GetTextIntercepts(
      text_run_paint_info, graphics_context_.DeviceScaleFactor(),
      graphics_context_.FillFlags(),
      std::make_tuple(upper, upper + stripe_width), text_intercepts);

  DecorationsStripeIntercepts(upper, stripe_width, dilation, text_intercepts);
}

}  // namespace blink
