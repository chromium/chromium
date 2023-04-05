// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/text_painter.h"

#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
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
  PaintInternal<kPaintText>(start_offset, end_offset, length, node_id,
                            auto_dark_mode);

  if (!emphasis_mark_.empty()) {
    if (text_style.emphasis_mark_color != text_style.fill_color) {
      graphics_context_.SetFillColor(text_style.emphasis_mark_color);
    }
    PaintInternal<kPaintEmphasisMark>(start_offset, end_offset, length, node_id,
                                      auto_dark_mode);
  }
}

void TextPainter::PaintDecorationsExceptLineThrough(
    const TextDecorationOffsetBase& decoration_offset,
    TextDecorationInfo& decoration_info,
    const PaintInfo& paint_info,
    const Vector<AppliedTextDecoration, 1>& decorations,
    const TextPaintStyle& text_style) {
  // Updating the graphics context and looping through applied decorations is
  // expensive, so avoid doing it if the only decoration was a ‘line-through’.
  if (!decoration_info.HasAnyLine(~TextDecorationLine::kLineThrough))
    return;

  GraphicsContext& context = paint_info.context;
  GraphicsContextStateSaver state_saver(context);
  UpdateGraphicsContext(context, text_style, state_saver);

  for (wtf_size_t applied_decoration_index = 0;
       applied_decoration_index < decorations.size();
       ++applied_decoration_index) {
    decoration_info.SetDecorationIndex(applied_decoration_index);
    context.SetStrokeThickness(decoration_info.ResolvedThickness());

    if (decoration_info.HasSpellingOrGrammerError()) {
      decoration_info.SetSpellingOrGrammarErrorLineData(decoration_offset);
      // We ignore "text-decoration-skip-ink: auto" for spelling and grammar
      // error markers.
      AppliedDecorationPainter decoration_painter(context, decoration_info);
      decoration_painter.Paint();
      continue;
    }

    if (decoration_info.HasUnderline() && decoration_info.FontData()) {
      decoration_info.SetUnderlineLineData(decoration_offset);
      PaintDecorationUnderOrOverLine(context, decoration_info,
                                     TextDecorationLine::kUnderline);
    }

    if (decoration_info.HasOverline() && decoration_info.FontData()) {
      decoration_info.SetOverlineLineData(decoration_offset);
      PaintDecorationUnderOrOverLine(context, decoration_info,
                                     TextDecorationLine::kOverline);
    }
  }
}

void TextPainter::PaintDecorationsOnlyLineThrough(
    TextDecorationInfo& decoration_info,
    const PaintInfo& paint_info,
    const Vector<AppliedTextDecoration, 1>& decorations,
    const TextPaintStyle& text_style) {
  // Updating the graphics context and looping through applied decorations is
  // expensive, so avoid doing it if there are no ‘line-through’ decorations.
  if (!decoration_info.HasAnyLine(TextDecorationLine::kLineThrough))
    return;

  GraphicsContext& context = paint_info.context;
  GraphicsContextStateSaver state_saver(context);
  UpdateGraphicsContext(context, text_style, state_saver);

  for (wtf_size_t applied_decoration_index = 0;
       applied_decoration_index < decorations.size();
       ++applied_decoration_index) {
    const AppliedTextDecoration& decoration =
        decorations[applied_decoration_index];
    TextDecorationLine lines = decoration.Lines();
    if (EnumHasFlags(lines, TextDecorationLine::kLineThrough)) {
      decoration_info.SetDecorationIndex(applied_decoration_index);

      const float resolved_thickness = decoration_info.ResolvedThickness();
      context.SetStrokeThickness(resolved_thickness);
      decoration_info.SetLineThroughLineData();
      AppliedDecorationPainter decoration_painter(context, decoration_info);
      // No skip: ink for line-through,
      // compare https://github.com/w3c/csswg-drafts/issues/711
      decoration_painter.Paint();
    }
  }
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
  font_.GetTextIntercepts(text_run_paint_info, graphics_context_.FillFlags(),
                          std::make_tuple(upper, upper + stripe_width),
                          text_intercepts);

  DecorationsStripeIntercepts(upper, stripe_width, dilation, text_intercepts);
}

void TextPainter::PaintDecorationUnderOrOverLine(
    GraphicsContext& context,
    TextDecorationInfo& decoration_info,
    TextDecorationLine line,
    const cc::PaintFlags* flags) {
  AppliedDecorationPainter decoration_painter(context, decoration_info);
  if (decoration_info.TargetStyle().TextDecorationSkipInk() ==
      ETextDecorationSkipInk::kAuto) {
    // In order to ignore intersects less than 0.5px, inflate by -0.5.
    gfx::RectF decoration_bounds = decoration_info.Bounds();
    decoration_bounds.Inset(gfx::InsetsF::VH(0.5, 0));
    ClipDecorationsStripe(
        decoration_info.InkSkipClipUpper(decoration_bounds.y()),
        decoration_bounds.height(),
        std::min(decoration_info.ResolvedThickness(),
                 kDecorationClipMaxDilation));
  }
  decoration_painter.Paint(flags);
}

}  // namespace blink
