// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/text_decoration_painter.h"

#include "third_party/blink/renderer/core/layout/inline/fragment_item.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline_text.h"
#include "third_party/blink/renderer/core/layout/text_decoration_offset.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/text_painter.h"
#include "third_party/blink/renderer/core/paint/text_shadow_painter.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"

namespace blink {

namespace {

Color LineColorForPhase(TextDecorationInfo& decoration_info,
                        TextShadowPaintPhase phase) {
  if (phase == TextShadowPaintPhase::kShadow) {
    return Color::kBlack;
  }
  return decoration_info.LineColor();
}

}  // namespace

TextDecorationPainter::TextDecorationPainter(
    TextPainter& text_painter,
    const InlinePaintContext* inline_context,
    const PaintInfo& paint_info,
    const ComputedStyle& style,
    const TextPaintStyle& text_style,
    const LineRelativeRect& decoration_rect,
    HighlightPainter::SelectionPaintState* selection)
    : text_painter_(text_painter),
      inline_context_(inline_context),
      paint_info_(paint_info),
      style_(style),
      text_style_(text_style),
      decoration_rect_(decoration_rect),
      selection_(selection),
      step_(kBegin),
      phase_(kOriginating) {}

TextDecorationPainter::~TextDecorationPainter() {
  DCHECK(step_ == kBegin);
}

void TextDecorationPainter::UpdateDecorationInfo(
    std::optional<TextDecorationInfo>& result,
    const FragmentItem& text_item,
    const ComputedStyle& style,
    std::optional<LineRelativeRect> decoration_rect_override,
    const AppliedTextDecoration* decoration_override) {
  result.reset();

  if ((!style.HasAppliedTextDecorations() && !decoration_override) ||
      // Ellipsis should not have text decorations. This is not defined, but
      // 4 impls do this: <https://github.com/w3c/csswg-drafts/issues/6531>
      text_item.IsEllipsis()) {
    return;
  }

  TextDecorationLine effective_selection_decoration_lines =
      TextDecorationLine::kNone;
  Color effective_selection_decoration_color;
  if (phase_ == kSelection) [[unlikely]] {
    effective_selection_decoration_lines =
        selection_->GetSelectionStyle().selection_decoration_lines;
    effective_selection_decoration_color =
        selection_->GetSelectionStyle().selection_decoration_color;
  }

  if (text_item.IsSvgText() && paint_info_.IsRenderingResourceSubtree()) {
    // Need to recompute a scaled font and a scaling factor because they
    // depend on the scaling factor of an element referring to the text.
    float scaling_factor = 1;
    Font scaled_font;
    LayoutSVGInlineText::ComputeNewScaledFontForStyle(
        *text_item.GetLayoutObject(), scaling_factor, scaled_font);
    DCHECK(scaling_factor);
    // Adjust the origin of the decoration because
    // TextPainter::PaintDecorationsExceptLineThrough() will change the
    // scaling of the GraphicsContext.
    LayoutUnit top = decoration_rect_.offset.line_over;
    // In svg/text/text-decorations-in-scaled-pattern.svg, the size of
    // ScaledFont() is zero, and the top position is unreliable. So we
    // adjust the baseline position, then shift it for scaled_font.
    top += text_item.ScaledFont().PrimaryFont()->GetFontMetrics().FixedAscent();
    top *= scaling_factor / text_item.SvgScalingFactor();
    top -= scaled_font.PrimaryFont()->GetFontMetrics().FixedAscent();
    result.emplace(LineRelativeOffset{decoration_rect_.offset.line_left, top},
                   decoration_rect_.InlineSize(), style, inline_context_,
                   effective_selection_decoration_lines,
                   effective_selection_decoration_color, decoration_override,
                   &scaled_font, MinimumThickness1(false),
                   text_item.SvgScalingFactor() / scaling_factor);
  } else {
    LineRelativeRect decoration_rect =
        decoration_rect_override.value_or(decoration_rect_);
    result.emplace(decoration_rect.offset, decoration_rect.InlineSize(), style,
                   inline_context_, effective_selection_decoration_lines,
                   effective_selection_decoration_color, decoration_override,
                   &text_item.ScaledFont(),
                   MinimumThickness1(!text_item.IsSvgText()));
  }
}

gfx::RectF TextDecorationPainter::ExpandRectForSVGDecorations(
    const LineRelativeRect& rect) {
  // Until SVG text has correct InkOverflow, we need to hack it.
  gfx::RectF clip_rect{rect};
  clip_rect.set_y(clip_rect.y() - clip_rect.height());
  clip_rect.set_height(3 * clip_rect.height());
  return clip_rect;
}

void TextDecorationPainter::Begin(const FragmentItem& text_item, Phase phase) {
  DCHECK(step_ == kBegin);

  phase_ = phase;
  UpdateDecorationInfo(decoration_info_, text_item, style_);
  clip_rect_.reset();

  if (decoration_info_ && selection_) [[unlikely]] {
    if (text_item.IsSvgText()) [[unlikely]] {
      clip_rect_.emplace(
          ExpandRectForSVGDecorations(selection_->LineRelativeSelectionRect()));
    } else {
      const LineRelativeRect selection_rect =
          selection_->LineRelativeSelectionRect();
      const PhysicalRect& ink_overflow_rect = text_item.InkOverflowRect();
      clip_rect_.emplace(selection_rect.offset.line_left, ink_overflow_rect.Y(),
                         selection_rect.size.inline_size,
                         ink_overflow_rect.Height());
    }
  }

  step_ = kExcept;
}

void TextDecorationPainter::PaintUnderOrOverLineDecorations(
    TextDecorationInfo& decoration_info,
    const TextFragmentPaintInfo& fragment_paint_info,
    const TextPaintStyle& text_style,
    TextDecorationLine lines_to_paint) {
  if (paint_info_.IsRenderingResourceSubtree()) {
    paint_info_.context.Scale(1, decoration_info.ScalingFactor());
  }
  const TextDecorationOffset decoration_offset(style_);

  PaintWithTextShadow(
      [&](TextShadowPaintPhase phase) {
        for (wtf_size_t i = 0; i < decoration_info.AppliedDecorationCount();
             i++) {
          decoration_info.SetDecorationIndex(i);

          if (decoration_info.HasSpellingOrGrammerError() &&
              EnumHasFlags(lines_to_paint,
                           TextDecorationLine::kSpellingError |
                               TextDecorationLine::kGrammarError)) {
            decoration_info.SetSpellingOrGrammarErrorLineData(
                decoration_offset);
            // We ignore "text-decoration-skip-ink: auto" for spelling and
            // grammar error markers.
            text_painter_.PaintDecorationLine(
                decoration_info, LineColorForPhase(decoration_info, phase),
                nullptr);
            continue;
          }

          if (decoration_info.HasUnderline() && decoration_info.FontData() &&
              EnumHasFlags(lines_to_paint, TextDecorationLine::kUnderline)) {
            decoration_info.SetUnderlineLineData(decoration_offset);
            text_painter_.PaintDecorationLine(
                decoration_info, LineColorForPhase(decoration_info, phase),
                &fragment_paint_info);
          }

          if (decoration_info.HasOverline() && decoration_info.FontData() &&
              EnumHasFlags(lines_to_paint, TextDecorationLine::kOverline)) {
            decoration_info.SetOverlineLineData(decoration_offset);
            text_painter_.PaintDecorationLine(
                decoration_info, LineColorForPhase(decoration_info, phase),
                &fragment_paint_info);
          }
        }
      },
      paint_info_.context, text_style);
}

void TextDecorationPainter::PaintLineThroughDecorations(
    TextDecorationInfo& decoration_info,
    const TextPaintStyle& text_style) {
  if (paint_info_.IsRenderingResourceSubtree()) {
    paint_info_.context.Scale(1, decoration_info.ScalingFactor());
  }

  PaintWithTextShadow(
      [&](TextShadowPaintPhase phase) {
        for (wtf_size_t applied_decoration_index = 0;
             applied_decoration_index <
             decoration_info.AppliedDecorationCount();
             ++applied_decoration_index) {
          const AppliedTextDecoration& decoration =
              decoration_info.AppliedDecoration(applied_decoration_index);
          TextDecorationLine lines = decoration.Lines();
          if (EnumHasFlags(lines, TextDecorationLine::kLineThrough)) {
            decoration_info.SetDecorationIndex(applied_decoration_index);

            decoration_info.SetLineThroughLineData();

            // No skip: ink for line-through,
            // compare https://github.com/w3c/csswg-drafts/issues/711
            text_painter_.PaintDecorationLine(
                decoration_info, LineColorForPhase(decoration_info, phase),
                nullptr);
          }
        }
      },
      paint_info_.context, text_style);
}

void TextDecorationPainter::PaintExceptLineThrough(
    const TextFragmentPaintInfo& fragment_paint_info) {
  DCHECK(step_ == kExcept);

  // Clipping the canvas unnecessarily is expensive, so avoid doing it if the
  // only decoration was a ‘line-through’.
  if (decoration_info_ &&
      decoration_info_->HasAnyLine(~TextDecorationLine::kLineThrough)) {
    GraphicsContextStateSaver state_saver(paint_info_.context);
    ClipIfNeeded(state_saver);
    PaintUnderOrOverLineDecorations(*decoration_info_, fragment_paint_info,
                                    text_style_, ~TextDecorationLine::kNone);
  }

  step_ = kOnly;
}

void TextDecorationPainter::PaintOnlyLineThrough() {
  DCHECK(step_ == kOnly);

  // Clipping the canvas unnecessarily is expensive, so avoid doing it if there
  // are no ‘line-through’ decorations.
  if (decoration_info_ &&
      decoration_info_->HasAnyLine(TextDecorationLine::kLineThrough)) {
    GraphicsContextStateSaver state_saver(paint_info_.context);
    ClipIfNeeded(state_saver);
    PaintLineThroughDecorations(*decoration_info_, text_style_);
  }

  step_ = kBegin;
}

void TextDecorationPainter::PaintExceptLineThrough(
    TextDecorationInfo& decoration_info,
    const TextPaintStyle& text_style,
    const TextFragmentPaintInfo& fragment_paint_info,
    TextDecorationLine lines_to_paint) {
  if (!decoration_info.HasAnyLine(lines_to_paint &
                                  ~TextDecorationLine::kLineThrough)) {
    return;
  }
  GraphicsContextStateSaver state_saver(paint_info_.context);
  PaintUnderOrOverLineDecorations(decoration_info, fragment_paint_info,
                                  text_style, lines_to_paint);
}

void TextDecorationPainter::PaintOnlyLineThrough(
    TextDecorationInfo& decoration_info,
    const TextPaintStyle& text_style) {
  if (!decoration_info.HasAnyLine(TextDecorationLine::kLineThrough)) {
    return;
  }
  GraphicsContextStateSaver state_saver(paint_info_.context);
  PaintLineThroughDecorations(decoration_info, text_style);
}

void TextDecorationPainter::ClipIfNeeded(
    GraphicsContextStateSaver& state_saver) {
  DCHECK(step_ != kBegin);

  if (clip_rect_) {
    state_saver.SaveIfNeeded();
    if (phase_ == kSelection)
      paint_info_.context.Clip(*clip_rect_);
    else
      paint_info_.context.ClipOut(*clip_rect_);
  }
}

}  // namespace blink
