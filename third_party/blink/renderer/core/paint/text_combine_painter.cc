// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/text_combine_painter.h"

#include "third_party/blink/renderer/core/layout/layout_text_combine.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/text_decoration_painter.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/fonts/plain_text_node.h"
#include "third_party/blink/renderer/platform/fonts/plain_text_painter.h"
#include "third_party/blink/renderer/platform/fonts/text_fragment_paint_info.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/text/text_run.h"

namespace blink {

TextCombinePainter::TextCombinePainter(
    GraphicsContext& context,
    const SvgContextPaints* svg_context_paints,
    const gfx::Rect& visual_rect,
    const ComputedStyle& style,
    const LineRelativeOffset& text_origin)
    : TextPainter(context,
                  svg_context_paints,
                  *style.GetFont(),
                  visual_rect,
                  text_origin),
      style_(style) {}

TextCombinePainter::~TextCombinePainter() = default;

void TextCombinePainter::Paint(const PaintInfo& paint_info,
                               const PhysicalOffset& paint_offset,
                               const LayoutTextCombine& text_combine) {
  if (paint_info.phase == PaintPhase::kBlockBackground ||
      paint_info.phase == PaintPhase::kForcedColorsModeBackplate ||
      paint_info.phase == PaintPhase::kFloat ||
      paint_info.phase == PaintPhase::kSelfBlockBackgroundOnly ||
      paint_info.phase == PaintPhase::kDescendantBlockBackgroundsOnly ||
      paint_info.phase == PaintPhase::kSelfOutlineOnly) {
    // Note: We should not paint text decoration and emphasis markr in above
    // paint phases. Otherwise, text decoration and emphasis mark are painted
    // multiple time and anti-aliasing is broken.
    // See virtual/text-antialias/emphasis-combined-text.html
    return;
  }

  // Here |paint_info.phases| is one of following:
  //    PaintPhase::kSelectionDragImage
  //    PaintPhase::kTextClip
  //    PaintPhase::kForeground
  //    PaintPhase::kOutline
  // These values come from |BoxFragmentPainter::PaintAllPhasesAtomically()|.

  const ComputedStyle& style = text_combine.Parent()->StyleRef();
  const bool has_text_decoration = style.HasAppliedTextDecorations();
  const bool has_emphasis_mark =
      style.GetTextEmphasisMark() != TextEmphasisMark::kNone;
  DCHECK(has_text_decoration | has_emphasis_mark);

  const LineRelativeRect& text_frame_rect =
      text_combine.ComputeTextFrameRect(paint_offset);

  // To match the logical direction
  GraphicsContextStateSaver state_saver(paint_info.context);
  paint_info.context.ConcatCTM(
      text_frame_rect.ComputeRelativeToPhysicalTransform(
          style.GetWritingMode()));

  TextCombinePainter text_painter(paint_info.context,
                                  paint_info.GetSvgContextPaints(),
                                  text_combine.VisualRectForPaint(paint_offset),
                                  style, text_frame_rect.offset);
  const TextPaintStyle text_style = TextPainter::TextPaintingStyle(
      text_combine.GetDocument(), style, paint_info);

  // Setup arguments for painting text decorations
  std::optional<TextDecorationInfo> decoration_info;
  std::optional<TextDecorationPainter> decoration_painter;
  if (has_text_decoration) {
    decoration_info.emplace(
        text_frame_rect.offset, text_frame_rect.InlineSize(), style,
        /* inline_context */ nullptr, TextDecorationLine::kNone, Color());
    decoration_painter.emplace(text_painter, /* inline_context */ nullptr,
                               paint_info, style, text_style, text_frame_rect,
                               nullptr);

    // Paint underline and overline text decorations.
    decoration_painter->PaintExceptLineThrough(*decoration_info, text_style,
                                               TextFragmentPaintInfo{},
                                               ~TextDecorationLine::kNone);
  }

  if (has_emphasis_mark) {
    text_painter.PaintEmphasisMark(text_style, *style.GetFont());
  }

  if (has_text_decoration) {
    // Paint line through if needed.
    decoration_painter->PaintOnlyLineThrough(*decoration_info, text_style);
  }
}

// static
bool TextCombinePainter::ShouldPaint(const LayoutTextCombine& text_combine) {
  const auto& style = text_combine.Parent()->StyleRef();
  return style.HasAppliedTextDecorations() ||
         style.GetTextEmphasisMark() != TextEmphasisMark::kNone;
}

void TextCombinePainter::ClipDecorationLine(const DecorationGeometry&,
                                            float ink_skip_offset,
                                            const TextFragmentPaintInfo&) {
  // Nothing to do.
}

void TextCombinePainter::PaintEmphasisMark(const TextPaintStyle& text_style,
                                           const Font& emphasis_mark_font) {
  DCHECK_NE(style_.GetTextEmphasisMark(), TextEmphasisMark::kNone);
  SetEmphasisMark(style_.TextEmphasisMarkString(),
                  style_.GetTextEmphasisLineLogicalSide());
  DCHECK(emphasis_mark_font.GetFontDescription().IsVerticalBaseline());
  DCHECK(emphasis_mark());
  const SimpleFontData* const font_data = font().PrimaryFont();
  DCHECK(font_data);
  if (!font_data) {
    return;
  }
  if (text_style.emphasis_mark_color != text_style.fill_color) {
    // See virtual/text-antialias/emphasis-combined-text.html
    graphics_context().SetFillColor(text_style.emphasis_mark_color);
  }

  const int font_ascent = font_data->GetFontMetrics().Ascent();
  // https://drafts.csswg.org/css-writing-modes/#text-combine-layout
  // > For other text layout purposes, e.g. emphasis marks, text-decoration,
  // > spacing, etc. the resulting composition is treated as a single glyph
  // > representing the Object Replacement Character U+FFFC.
  //
  // However the shape size of U+FFFC isn't suitable for emphasis mark
  // positioning. We use Hiragana Letter A instead. See crbug.com/40386493
  const TextRun placeholder_text_run(
      base::span_from_ref(uchar::kHiraganaLetterA));
  const gfx::PointF emphasis_mark_text_origin =
      gfx::PointF(text_origin()) +
      gfx::Vector2dF(0, font_ascent + emphasis_mark_offset());

  const PlainTextNode& node = PlainTextPainter::Shared().SegmentAndShape(
      placeholder_text_run, emphasis_mark_font);
  if (node.ItemList().empty()) {
    return;
  }
  const ShapeResultView* shape_view = node.ItemList()[0].EnsureView();
  if (!shape_view) {
    return;
  }
  graphics_context().DrawEmphasisMarks(
      emphasis_mark_font,
      TextFragmentPaintInfo{placeholder_text_run.ToStringView(), 0, 1,
                            shape_view},
      emphasis_mark(), emphasis_mark_text_origin,
      PaintAutoDarkMode(style_, DarkModeFilter::ElementRole::kForeground));
}

}  // namespace blink
