// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/ng/ng_text_painter.h"

#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_item.h"
#include "third_party/blink/renderer/core/layout/ng/ng_text_decoration_offset.h"
#include "third_party/blink/renderer/core/layout/ng/ng_unpositioned_float.h"
#include "third_party/blink/renderer/core/paint/applied_decoration_painter.h"
#include "third_party/blink/renderer/core/paint/box_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_timing_detector.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/shadow_list.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"

namespace blink {

namespace {

absl::optional<TextDecorationInfo> DecorationsForLayer(
    const NGFragmentItem& text_item,
    const PhysicalRect& decoration_rect,
    const ComputedStyle& style,
    const absl::optional<AppliedTextDecoration>& selection_text_decoration) {
  if (style.TextDecorationsInEffect() == TextDecoration::kNone ||
      // Ellipsis should not have text decorations. This is not defined, but
      // 4 impls do this.
      text_item.IsEllipsis()) {
    return absl::nullopt;
  }
  return TextDecorationInfo(decoration_rect.offset, decoration_rect.offset,
                            decoration_rect.Width(), style.GetFontBaseline(),
                            style, selection_text_decoration, nullptr);
}

}  // namespace

void NGTextPainter::Paint(unsigned start_offset,
                          unsigned end_offset,
                          unsigned length,
                          const TextPaintStyle& text_style,
                          DOMNodeId node_id,
                          ShadowMode shadow_mode) {
  GraphicsContextStateSaver state_saver(graphics_context_, false);
  UpdateGraphicsContext(graphics_context_, text_style, horizontal_, state_saver,
                        shadow_mode);
  // TODO(layout-dev): Handle combine text here or elsewhere.
  PaintInternal<kPaintText>(start_offset, end_offset, length, node_id);

  if (!emphasis_mark_.IsEmpty()) {
    if (text_style.emphasis_mark_color != text_style.fill_color)
      graphics_context_.SetFillColor(text_style.emphasis_mark_color);
    PaintInternal<kPaintEmphasisMark>(start_offset, end_offset, length,
                                      node_id);
  }
}

// This function paints text twice with different styles in order to:
// 1. Paint glyphs inside of |selection_rect| using |selection_style|, and
//    outside using |text_style|.
// 2. Paint parts of a ligature glyph.
void NGTextPainter::PaintSelectedText(unsigned start_offset,
                                      unsigned end_offset,
                                      unsigned length,
                                      const TextPaintStyle& text_style,
                                      const TextPaintStyle& selection_style,
                                      const PhysicalRect& selection_rect,
                                      DOMNodeId node_id) {
  if (!fragment_paint_info_.shape_result)
    return;

  // Use fast path if all glyphs fit in |selection_rect|. |visual_rect_| is the
  // ink bounds of all glyphs of this text fragment, including characters before
  // |start_offset| or after |end_offset|. Computing exact bounds is expensive
  // that this code only checks bounds of all glyphs.
  IntRect snapped_selection_rect(PixelSnappedIntRect(selection_rect));
  // Allowing 1px overflow is almost unnoticeable, while it can avoid two-pass
  // painting in most small text.
  snapped_selection_rect.Inflate(1);
  if (snapped_selection_rect.Contains(visual_rect_)) {
    Paint(start_offset, end_offset, length, selection_style, node_id);
    return;
  }

  // Adjust start/end offset when they are in the middle of a ligature. e.g.,
  // when |start_offset| is between a ligature of "fi", it needs to be adjusted
  // to before "f".
  fragment_paint_info_.shape_result->ExpandRangeToIncludePartialGlyphs(
      &start_offset, &end_offset);

  // Because only a part of the text glyph can be selected, we need to draw
  // the selection twice. First, draw the glyphs outside the selection area,
  // with the original style.
  FloatRect float_selection_rect(selection_rect);
  {
    GraphicsContextStateSaver state_saver(graphics_context_);
    graphics_context_.ClipOut(float_selection_rect);
    Paint(start_offset, end_offset, length, text_style, node_id,
          kTextProperOnly);
  }
  // Then draw the glyphs inside the selection area, with the selection style.
  {
    GraphicsContextStateSaver state_saver(graphics_context_);
    graphics_context_.Clip(float_selection_rect);
    Paint(start_offset, end_offset, length, selection_style, node_id);
  }
}

// Based on legacy TextPainter.
void NGTextPainter::PaintDecorationsExceptLineThrough(
    const NGFragmentItem& text_item,
    const PaintInfo& paint_info,
    const ComputedStyle& style,
    const TextPaintStyle& text_style,
    const PhysicalRect& decoration_rect,
    const absl::optional<AppliedTextDecoration>& selection_decoration,
    bool* has_line_through_decoration) {
  *has_line_through_decoration = false;

  absl::optional<TextDecorationInfo> decoration_info = DecorationsForLayer(
      text_item, decoration_rect, style, selection_decoration);

  if (!decoration_info) {
    return;
  }

  const NGTextDecorationOffset decoration_offset(decoration_info->Style(),
                                                 text_item.Style(), nullptr);

  GraphicsContext& context = paint_info.context;
  GraphicsContextStateSaver state_saver(context);
  UpdateGraphicsContext(context, text_style, horizontal_, state_saver);

  if (has_combined_text_)
    context.ConcatCTM(Rotation(text_frame_rect_, kClockwise));

  // text-underline-position may flip underline and overline.
  ResolvedUnderlinePosition underline_position =
      decoration_info->UnderlinePosition();
  bool flip_underline_and_overline = false;
  if (underline_position == ResolvedUnderlinePosition::kOver) {
    flip_underline_and_overline = true;
    underline_position = ResolvedUnderlinePosition::kUnder;
  }

  const Vector<AppliedTextDecoration>& decorations =
      style.AppliedTextDecorations();
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

    decoration_info->SetDecorationIndex(applied_decoration_index);

    float resolved_thickness = decoration_info->ResolvedThickness();
    context.SetStrokeThickness(resolved_thickness);

    if (has_underline && decoration_info->FontData()) {
      // Don't apply text-underline-offset to overline.
      Length line_offset =
          flip_underline_and_overline ? Length() : decoration.UnderlineOffset();

      const int paint_underline_offset =
          decoration_offset.ComputeUnderlineOffset(
              underline_position, decoration_info->Style().ComputedFontSize(),
              decoration_info->FontData()->GetFontMetrics(), line_offset,
              resolved_thickness);
      decoration_info->SetPerLineData(
          TextDecoration::kUnderline, paint_underline_offset,
          TextDecorationInfo::DoubleOffsetFromThickness(resolved_thickness), 1);
      PaintDecorationUnderOrOverLine(context, *decoration_info,
                                     TextDecoration::kUnderline);
    }

    if (has_overline && decoration_info->FontData()) {
      // Don't apply text-underline-offset to overline.
      Length line_offset =
          flip_underline_and_overline ? decoration.UnderlineOffset() : Length();

      FontVerticalPositionType position =
          flip_underline_and_overline ? FontVerticalPositionType::TopOfEmHeight
                                      : FontVerticalPositionType::TextTop;
      const int paint_overline_offset =
          decoration_offset.ComputeUnderlineOffsetForUnder(
              line_offset, decoration_info->Style().ComputedFontSize(),
              resolved_thickness, position);
      decoration_info->SetPerLineData(
          TextDecoration::kOverline, paint_overline_offset,
          -TextDecorationInfo::DoubleOffsetFromThickness(resolved_thickness),
          1);
      PaintDecorationUnderOrOverLine(context, *decoration_info,
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

// Based on legacy TextPainter.
void NGTextPainter::PaintDecorationsOnlyLineThrough(
    const NGFragmentItem& text_item,
    const PaintInfo& paint_info,
    const ComputedStyle& style,
    const TextPaintStyle& text_style,
    const PhysicalRect& decoration_rect,
    const absl::optional<AppliedTextDecoration>& selection_decoration) {
  absl::optional<TextDecorationInfo> decoration_info = DecorationsForLayer(
      text_item, decoration_rect, style, selection_decoration);

  DCHECK(decoration_info);

  const NGTextDecorationOffset decoration_offset(decoration_info->Style(),
                                                 text_item.Style(), nullptr);

  GraphicsContext& context = paint_info.context;
  GraphicsContextStateSaver state_saver(context);
  UpdateGraphicsContext(context, text_style, horizontal_, state_saver);

  if (has_combined_text_)
    context.ConcatCTM(Rotation(text_frame_rect_, kClockwise));

  const Vector<AppliedTextDecoration>& decorations =
      style.AppliedTextDecorations();
  for (size_t applied_decoration_index = 0;
       applied_decoration_index < decorations.size();
       ++applied_decoration_index) {
    const AppliedTextDecoration& decoration =
        decorations[applied_decoration_index];
    TextDecoration lines = decoration.Lines();
    if (EnumHasFlags(lines, TextDecoration::kLineThrough)) {
      decoration_info->SetDecorationIndex(applied_decoration_index);

      float resolved_thickness = decoration_info->ResolvedThickness();
      context.SetStrokeThickness(resolved_thickness);

      // For increased line thickness, the line-through decoration needs to grow
      // in both directions from its origin, subtract half the thickness to keep
      // it centered at the same origin.
      const float line_through_offset =
          2 * decoration_info->Baseline() / 3 - resolved_thickness / 2;
      // Floor double_offset in order to avoid double-line gap to appear
      // of different size depending on position where the double line
      // is drawn because of rounding downstream in
      // GraphicsContext::DrawLineForText.
      decoration_info->SetPerLineData(
          TextDecoration::kLineThrough, line_through_offset,
          floorf(TextDecorationInfo::DoubleOffsetFromThickness(
              resolved_thickness)),
          0);
      AppliedDecorationPainter decoration_painter(context, *decoration_info,
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

template <NGTextPainter::PaintInternalStep step>
void NGTextPainter::PaintInternalFragment(
    unsigned from,
    unsigned to,
    DOMNodeId node_id) {
  DCHECK(from <= fragment_paint_info_.text.length());
  DCHECK(to <= fragment_paint_info_.text.length());

  fragment_paint_info_.from = from;
  fragment_paint_info_.to = to;

  if (step == kPaintEmphasisMark) {
    graphics_context_.DrawEmphasisMarks(
        font_, fragment_paint_info_, emphasis_mark_,
        FloatPoint(text_origin_) + IntSize(0, emphasis_mark_offset_));
  } else {
    DCHECK(step == kPaintText);
    graphics_context_.DrawText(font_, fragment_paint_info_,
                               FloatPoint(text_origin_), node_id);
    // TODO(npm): Check that there are non-whitespace characters. See
    // crbug.com/788444.
    graphics_context_.GetPaintController().SetTextPainted();

    if (!font_.ShouldSkipDrawing())
      PaintTimingDetector::NotifyTextPaint(visual_rect_);
  }
}

template <NGTextPainter::PaintInternalStep Step>
void NGTextPainter::PaintInternal(unsigned start_offset,
                                  unsigned end_offset,
                                  unsigned truncation_point,
                                  DOMNodeId node_id) {
  // TODO(layout-dev): We shouldn't be creating text fragments without text.
  if (!fragment_paint_info_.shape_result)
    return;

  if (start_offset <= end_offset) {
    PaintInternalFragment<Step>(start_offset, end_offset, node_id);
  } else {
    if (end_offset > 0) {
      PaintInternalFragment<Step>(ellipsis_offset_, end_offset, node_id);
    }
    if (start_offset < truncation_point) {
      PaintInternalFragment<Step>(start_offset, truncation_point, node_id);
    }
  }
}

void NGTextPainter::ClipDecorationsStripe(float upper,
                                          float stripe_width,
                                          float dilation) {
  if (fragment_paint_info_.from >= fragment_paint_info_.to ||
      !fragment_paint_info_.shape_result)
    return;

  Vector<Font::TextIntercept> text_intercepts;
  font_.GetTextIntercepts(
      fragment_paint_info_, graphics_context_.DeviceScaleFactor(),
      graphics_context_.FillFlags(),
      std::make_tuple(upper, upper + stripe_width), text_intercepts);

  DecorationsStripeIntercepts(upper, stripe_width, dilation, text_intercepts);
}

void NGTextPainter::PaintEmphasisMarkForCombinedText() {}

}  // namespace blink
