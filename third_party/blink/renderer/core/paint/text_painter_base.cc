// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/text_painter_base.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/layout/text_decoration_offset_base.h"
#include "third_party/blink/renderer/core/paint/applied_decoration_painter.h"
#include "third_party/blink/renderer/core/paint/box_painter_base.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/selection_painting_utils.h"
#include "third_party/blink/renderer/core/paint/text_decoration_info.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/shadow_list.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"

namespace blink {

namespace {

// We usually use the text decoration thickness to determine how far
// ink-skipped text decorations should be away from the glyph
// contours. Cap this at 5 CSS px in each direction when thickness
// growths larger than that. A value of 13 closely matches FireFox'
// implementation.
constexpr float kDecorationClipMaxDilation = 13;

}  // anonymous namespace

TextPainterBase::TextPainterBase(GraphicsContext& context,
                                 const Font& font,
                                 const PhysicalOffset& text_origin,
                                 const PhysicalRect& text_frame_rect,
                                 bool horizontal)
    : graphics_context_(context),
      font_(font),
      text_origin_(text_origin),
      text_frame_rect_(text_frame_rect),
      horizontal_(horizontal),
      has_combined_text_(false),
      emphasis_mark_offset_(0),
      ellipsis_offset_(0) {}

TextPainterBase::~TextPainterBase() = default;

void TextPainterBase::SetEmphasisMark(const AtomicString& emphasis_mark,
                                      TextEmphasisPosition position) {
  emphasis_mark_ = emphasis_mark;
  const SimpleFontData* font_data = font_.PrimaryFont();
  DCHECK(font_data);

  if (!font_data || emphasis_mark.IsNull()) {
    emphasis_mark_offset_ = 0;
  } else if ((horizontal_ && (position == TextEmphasisPosition::kOverRight ||
                              position == TextEmphasisPosition::kOverLeft)) ||
             (!horizontal_ &&
              (position == TextEmphasisPosition::kOverRight ||
               position == TextEmphasisPosition::kUnderRight))) {
    emphasis_mark_offset_ = -font_data->GetFontMetrics().Ascent() -
                            font_.EmphasisMarkDescent(emphasis_mark);
  } else {
    DCHECK(position == TextEmphasisPosition::kUnderRight ||
           position == TextEmphasisPosition::kUnderLeft ||
           position == TextEmphasisPosition::kOverLeft);
    emphasis_mark_offset_ = font_data->GetFontMetrics().Descent() +
                            font_.EmphasisMarkAscent(emphasis_mark);
  }
}

// static
void TextPainterBase::UpdateGraphicsContext(
    GraphicsContext& context,
    const TextPaintStyle& text_style,
    bool horizontal,
    GraphicsContextStateSaver& state_saver) {
  TextDrawingModeFlags mode = context.TextDrawingMode();
  if (text_style.stroke_width > 0) {
    TextDrawingModeFlags new_mode = mode | kTextModeStroke;
    if (mode != new_mode) {
      state_saver.SaveIfNeeded();
      context.SetTextDrawingMode(new_mode);
      mode = new_mode;
    }
  }

  if (mode & kTextModeFill && text_style.fill_color != context.FillColor())
    context.SetFillColor(text_style.fill_color);

  if (mode & kTextModeStroke) {
    if (text_style.stroke_color != context.StrokeColor())
      context.SetStrokeColor(text_style.stroke_color);
    if (text_style.stroke_width != context.StrokeThickness())
      context.SetStrokeThickness(text_style.stroke_width);
  }

  if (text_style.shadow) {
    state_saver.SaveIfNeeded();
    context.SetDrawLooper(text_style.shadow->CreateDrawLooper(
        DrawLooperBuilder::kShadowIgnoresAlpha, text_style.current_color,
        text_style.color_scheme, horizontal));
  }
}

Color TextPainterBase::TextColorForWhiteBackground(Color text_color) {
  int distance_from_white = DifferenceSquared(text_color, Color::kWhite);
  // semi-arbitrarily chose 65025 (255^2) value here after a few tests;
  return distance_from_white > 65025 ? text_color : text_color.Dark();
}

// static
TextPaintStyle TextPainterBase::TextPaintingStyle(const Document& document,
                                                  const ComputedStyle& style,
                                                  const PaintInfo& paint_info) {
  TextPaintStyle text_style;
  text_style.stroke_width = style.TextStrokeWidth();
  text_style.color_scheme = style.UsedColorScheme();
  bool is_printing = paint_info.IsPrinting();

  if (paint_info.phase == PaintPhase::kTextClip) {
    // When we use the text as a clip, we only care about the alpha, thus we
    // make all the colors black.
    text_style.current_color = Color::kBlack;
    text_style.fill_color = Color::kBlack;
    text_style.stroke_color = Color::kBlack;
    text_style.emphasis_mark_color = Color::kBlack;
    text_style.shadow = nullptr;
  } else {
    text_style.current_color =
        style.VisitedDependentColor(GetCSSPropertyColor());
    text_style.fill_color =
        style.VisitedDependentColor(GetCSSPropertyWebkitTextFillColor());
    text_style.stroke_color =
        style.VisitedDependentColor(GetCSSPropertyWebkitTextStrokeColor());
    text_style.emphasis_mark_color =
        style.VisitedDependentColor(GetCSSPropertyWebkitTextEmphasisColor());
    text_style.shadow = style.TextShadow();

    // Adjust text color when printing with a white background.
    DCHECK_EQ(document.Printing(), is_printing);
    bool force_background_to_white =
        BoxPainterBase::ShouldForceWhiteBackgroundForPrintEconomy(document,
                                                                  style);
    if (force_background_to_white) {
      text_style.fill_color =
          TextColorForWhiteBackground(text_style.fill_color);
      text_style.stroke_color =
          TextColorForWhiteBackground(text_style.stroke_color);
      text_style.emphasis_mark_color =
          TextColorForWhiteBackground(text_style.emphasis_mark_color);
    }
  }

  return text_style;
}

TextPaintStyle TextPainterBase::SelectionPaintingStyle(
    const Document& document,
    const ComputedStyle& style,
    Node* node,
    bool have_selection,
    const PaintInfo& paint_info,
    const TextPaintStyle& text_style) {
  return SelectionPaintingUtils::SelectionPaintingStyle(
      document, style, node, have_selection, text_style, paint_info);
}

void TextPainterBase::DecorationsStripeIntercepts(
    float upper,
    float stripe_width,
    float dilation,
    const Vector<Font::TextIntercept>& text_intercepts) {
  for (auto intercept : text_intercepts) {
    FloatPoint clip_origin(text_origin_);
    FloatRect clip_rect(
        clip_origin + FloatPoint(intercept.begin_, upper),
        FloatSize(intercept.end_ - intercept.begin_, stripe_width));
    clip_rect.InflateX(dilation);
    // We need to ensure the clip rectangle is covering the full underline
    // extent. For horizontal drawing, using enclosingIntRect would be
    // sufficient, since we can clamp to full device pixels that way. However,
    // for vertical drawing, we have a transformation applied, which breaks the
    // integers-equal-device pixels assumption, so vertically inflating by 1
    // pixel makes sure we're always covering. This should only be done on the
    // clipping rectangle, not when computing the glyph intersects.
    clip_rect.InflateY(1.0);

    if (!clip_rect.IsFinite())
      continue;
    graphics_context_.ClipOut(clip_rect);
  }
}

void TextPainterBase::PaintDecorationsExceptLineThrough(
    const TextDecorationOffsetBase& decoration_offset,
    TextDecorationInfo& decoration_info,
    const PaintInfo& paint_info,
    const Vector<AppliedTextDecoration>& decorations,
    const TextPaintStyle& text_style,
    bool* has_line_through_decoration) {
  GraphicsContext& context = paint_info.context;
  GraphicsContextStateSaver state_saver(context);
  UpdateGraphicsContext(context, text_style, horizontal_, state_saver);

  if (has_combined_text_)
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
  if (has_combined_text_)
    context.ConcatCTM(Rotation(text_frame_rect_, kCounterclockwise));
}

void TextPainterBase::PaintDecorationsOnlyLineThrough(
    TextDecorationInfo& decoration_info,
    const PaintInfo& paint_info,
    const Vector<AppliedTextDecoration>& decorations,
    const TextPaintStyle& text_style) {
  GraphicsContext& context = paint_info.context;
  GraphicsContextStateSaver state_saver(context);
  UpdateGraphicsContext(context, text_style, horizontal_, state_saver);

  if (has_combined_text_)
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
  if (has_combined_text_)
    context.ConcatCTM(Rotation(text_frame_rect_, kCounterclockwise));
}

void TextPainterBase::PaintDecorationUnderOrOverLine(
    GraphicsContext& context,
    TextDecorationInfo& decoration_info,
    TextDecoration line) {
  AppliedDecorationPainter decoration_painter(context, decoration_info, line);
  if (decoration_info.Style().TextDecorationSkipInk() ==
      ETextDecorationSkipInk::kAuto) {
    // In order to ignore intersects less than 0.5px, inflate by -0.5.
    FloatRect decoration_bounds = decoration_info.BoundsForLine(line);
    decoration_bounds.InflateY(-0.5);
    ClipDecorationsStripe(
        decoration_info.InkSkipClipUpper(decoration_bounds.Y()),
        decoration_bounds.Height(),
        std::min(decoration_info.ResolvedThickness(),
                 kDecorationClipMaxDilation));
  }
  decoration_painter.Paint();
}

}  // namespace blink
