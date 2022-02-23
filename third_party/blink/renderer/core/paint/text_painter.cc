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
                        DOMNodeId node_id,
                        const AutoDarkMode& auto_dark_mode) {
  GraphicsContextStateSaver state_saver(graphics_context_, false);
  UpdateGraphicsContext(text_style, state_saver);
  if (combined_text_) {
    graphics_context_.Save();
    combined_text_->TransformToInlineCoordinates(graphics_context_,
                                                 text_frame_rect_);
    PaintInternal<kPaintText>(start_offset, end_offset, length, node_id,
                              auto_dark_mode);
    graphics_context_.Restore();
  } else {
    PaintInternal<kPaintText>(start_offset, end_offset, length, node_id,
                              auto_dark_mode);
  }

  if (!emphasis_mark_.IsEmpty()) {
    if (combined_text_) {
      graphics_context_.ConcatCTM(Rotation(text_frame_rect_, kClockwise));
      PaintEmphasisMarkForCombinedText(
          text_style, combined_text_->OriginalFont(), auto_dark_mode);
      graphics_context_.ConcatCTM(
          Rotation(text_frame_rect_, kCounterclockwise));
    } else {
      if (text_style.emphasis_mark_color != text_style.fill_color)
        graphics_context_.SetFillColor(text_style.emphasis_mark_color);
      PaintInternal<kPaintEmphasisMark>(start_offset, end_offset, length,
                                        node_id, auto_dark_mode);
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
  UpdateGraphicsContext(context, text_style, state_saver);

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

  for (wtf_size_t applied_decoration_index = 0;
       applied_decoration_index < decorations.size();
       ++applied_decoration_index) {
    const AppliedTextDecoration& decoration =
        decorations[applied_decoration_index];
    TextDecorationLine lines = decoration.Lines();
    bool has_underline = EnumHasFlags(lines, TextDecorationLine::kUnderline);
    bool has_overline = EnumHasFlags(lines, TextDecorationLine::kOverline);
    bool is_spelling_error =
        EnumHasFlags(lines, TextDecorationLine::kSpellingError);
    bool is_grammar_error =
        EnumHasFlags(lines, TextDecorationLine::kGrammarError);
    if (flip_underline_and_overline)
      std::swap(has_underline, has_overline);

    decoration_info.SetDecorationIndex(applied_decoration_index);

    float resolved_thickness = decoration_info.ResolvedThickness();
    context.SetStrokeThickness(resolved_thickness);

    if (is_spelling_error || is_grammar_error) {
      DCHECK(!has_underline && !has_overline &&
             !EnumHasFlags(lines, TextDecorationLine::kLineThrough));
      const int paint_underline_offset =
          decoration_offset.ComputeUnderlineOffset(
              underline_position, decoration_info.Style().ComputedFontSize(),
              decoration_info.FontData(), decoration.UnderlineOffset(),
              resolved_thickness);
      decoration_info.SetLineData(is_spelling_error
                                      ? TextDecorationLine::kSpellingError
                                      : TextDecorationLine::kGrammarError,
                                  paint_underline_offset);
      // We ignore "text-decoration-skip-ink: auto" for spelling and grammar
      // error markers.
      AppliedDecorationPainter decoration_painter(context, decoration_info);
      decoration_painter.Paint();
      continue;
    }

    if (has_underline && decoration_info.FontData()) {
      // Don't apply text-underline-offset to overline.
      Length line_offset =
          flip_underline_and_overline ? Length() : decoration.UnderlineOffset();

      const int paint_underline_offset =
          decoration_offset.ComputeUnderlineOffset(
              underline_position, decoration_info.Style().ComputedFontSize(),
              decoration_info.FontData(), line_offset, resolved_thickness);
      decoration_info.SetLineData(TextDecorationLine::kUnderline,
                                  paint_underline_offset);
      PaintDecorationUnderOrOverLine(context, decoration_info,
                                     TextDecorationLine::kUnderline);
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
              decoration_info.FontData(), resolved_thickness, position);
      decoration_info.SetLineData(TextDecorationLine::kOverline,
                                  paint_overline_offset);
      PaintDecorationUnderOrOverLine(context, decoration_info,
                                     TextDecorationLine::kOverline);
    }

    // We could instead build a vector of the TextDecorationLine instances
    // needing line-through but this is a rare case so better to avoid vector
    // overhead.
    *has_line_through_decoration |=
        EnumHasFlags(lines, TextDecorationLine::kLineThrough);
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
  UpdateGraphicsContext(context, text_style, state_saver);

  if (combined_text_)
    context.ConcatCTM(Rotation(text_frame_rect_, kClockwise));

  for (wtf_size_t applied_decoration_index = 0;
       applied_decoration_index < decorations.size();
       ++applied_decoration_index) {
    const AppliedTextDecoration& decoration =
        decorations[applied_decoration_index];
    TextDecorationLine lines = decoration.Lines();
    if (EnumHasFlags(lines, TextDecorationLine::kLineThrough)) {
      decoration_info.SetDecorationIndex(applied_decoration_index);

      float resolved_thickness = decoration_info.ResolvedThickness();
      context.SetStrokeThickness(resolved_thickness);

      // For increased line thickness, the line-through decoration needs to grow
      // in both directions from its origin, subtract half the thickness to keep
      // it centered at the same origin.
      const float line_through_offset =
          2 * decoration_info.Baseline() / 3 - resolved_thickness / 2;
      decoration_info.SetLineData(TextDecorationLine::kLineThrough,
                                  line_through_offset);
      AppliedDecorationPainter decoration_painter(context, decoration_info);
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
                                   DOMNodeId node_id,
                                   const AutoDarkMode& auto_dark_mode) {
  DCHECK(from <= text_run_paint_info.run.length());
  DCHECK(to <= text_run_paint_info.run.length());

  text_run_paint_info.from = from;
  text_run_paint_info.to = to;

  if (step == kPaintEmphasisMark) {
    graphics_context_.DrawEmphasisMarks(
        font_, text_run_paint_info, emphasis_mark_,
        gfx::PointF(text_origin_) + gfx::Vector2dF(0, emphasis_mark_offset_),
        auto_dark_mode);
  } else {
    DCHECK(step == kPaintText);
    graphics_context_.DrawText(font_, text_run_paint_info,
                               gfx::PointF(text_origin_), node_id,
                               auto_dark_mode);
  }
}

template <TextPainter::PaintInternalStep Step>
void TextPainter::PaintInternal(unsigned start_offset,
                                unsigned end_offset,
                                unsigned truncation_point,
                                DOMNodeId node_id,
                                const AutoDarkMode& auto_dark_mode) {
  TextRunPaintInfo text_run_paint_info(run_);
  if (start_offset <= end_offset) {
    PaintInternalRun<Step>(text_run_paint_info, start_offset, end_offset,
                           node_id, auto_dark_mode);
  } else {
    if (end_offset > 0) {
      PaintInternalRun<Step>(text_run_paint_info, ellipsis_offset_, end_offset,
                             node_id, auto_dark_mode);
    }
    if (start_offset < truncation_point) {
      PaintInternalRun<Step>(text_run_paint_info, start_offset,
                             truncation_point, node_id, auto_dark_mode);
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
