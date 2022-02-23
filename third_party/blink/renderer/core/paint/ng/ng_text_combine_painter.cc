// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/ng/ng_text_combine_painter.h"

#include "third_party/blink/renderer/core/layout/ng/inline/layout_ng_text_combine.h"
#include "third_party/blink/renderer/core/layout/ng/ng_text_decoration_offset.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"

namespace blink {

NGTextCombinePainter::NGTextCombinePainter(GraphicsContext& context,
                                           const ComputedStyle& style,
                                           const PhysicalRect& text_frame_rect)
    : TextPainterBase(context,
                      style.GetFont(),
                      text_frame_rect.offset,
                      text_frame_rect,
                      /* horizontal */ false),
      style_(style) {}

NGTextCombinePainter::~NGTextCombinePainter() = default;

void NGTextCombinePainter::Paint(const PaintInfo& paint_info,
                                 const PhysicalOffset& paint_offset,
                                 const LayoutNGTextCombine& text_combine) {
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
  // These values come from |NGBoxFragmentPainter::PaintAllPhasesAtomically()|.

  const ComputedStyle& style = text_combine.Parent()->StyleRef();
  const bool has_text_decoration =
      style.TextDecorationsInEffect() != TextDecorationLine::kNone;
  const bool has_emphasis_mark =
      style.GetTextEmphasisMark() != TextEmphasisMark::kNone;
  DCHECK(has_text_decoration | has_emphasis_mark);

  const PhysicalRect& text_frame_rect =
      text_combine.ComputeTextFrameRect(paint_offset);

  // To match the logical direction
  GraphicsContextStateSaver state_saver(paint_info.context);
  paint_info.context.ConcatCTM(
      TextPainterBase::Rotation(text_frame_rect, style.GetWritingMode()));

  NGTextCombinePainter text_painter(paint_info.context, style, text_frame_rect);
  const TextPaintStyle text_style = TextPainterBase::TextPaintingStyle(
      text_combine.GetDocument(), style, paint_info);

  if (has_emphasis_mark) {
    text_painter.PaintEmphasisMark(text_style,
                                   text_combine.Parent()->StyleRef().GetFont(),
                                   text_combine.GetDocument());
  }

  if (has_text_decoration)
    text_painter.PaintDecorations(paint_info, text_style);
}

// static
bool NGTextCombinePainter::ShouldPaint(
    const LayoutNGTextCombine& text_combine) {
  const auto& style = text_combine.Parent()->StyleRef();
  return style.TextDecorationsInEffect() != TextDecorationLine::kNone ||
         style.GetTextEmphasisMark() != TextEmphasisMark::kNone;
}

void NGTextCombinePainter::ClipDecorationsStripe(float upper,
                                                 float stripe_width,
                                                 float dilation) {
  // Nothing to do.
}

void NGTextCombinePainter::PaintDecorations(const PaintInfo& paint_info,
                                            const TextPaintStyle& text_style) {
  // Setup arguments for painting text decorations
  const absl::optional<AppliedTextDecoration> selection_text_decoration;
  const ComputedStyle* const decorating_box_style = nullptr;
  TextDecorationInfo decoration_info(
      text_frame_rect_.offset, text_frame_rect_.size.width,
      style_.GetFontBaseline(), style_, style_.GetFont(),
      selection_text_decoration, decorating_box_style);

  const NGTextDecorationOffset decoration_offset(style_, style_, nullptr);
  const auto& applied_text_decorations = style_.AppliedTextDecorations();

  // Paint text decorations except line through
  bool has_line_through_decoration = false;
  PaintDecorationsExceptLineThrough(decoration_offset, decoration_info,
                                    paint_info, applied_text_decorations,
                                    text_style, &has_line_through_decoration);
  if (!has_line_through_decoration)
    return;

  // Paint line through
  PaintDecorationsOnlyLineThrough(decoration_info, paint_info,
                                  applied_text_decorations, text_style);
}

void NGTextCombinePainter::PaintEmphasisMark(const TextPaintStyle& text_style,
                                             const Font& emphasis_mark_font,
                                             const Document& document) {
  DCHECK_NE(style_.GetTextEmphasisMark(), TextEmphasisMark::kNone);
  SetEmphasisMark(style_.TextEmphasisMarkString(),
                  style_.GetTextEmphasisPosition());
  PaintEmphasisMarkForCombinedText(
      text_style, emphasis_mark_font,
      PaintAutoDarkMode(style_, DarkModeFilter::ElementRole::kForeground));
}

}  // namespace blink
