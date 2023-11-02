// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/ng/ng_text_painter_base.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/layout/text_decoration_offset_base.h"
#include "third_party/blink/renderer/core/paint/applied_decoration_painter.h"
#include "third_party/blink/renderer/core/paint/box_painter_base.h"
#include "third_party/blink/renderer/core/paint/highlight_painting_utils.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/text_decoration_info.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/shadow_list.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/text_run_paint_info.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"

namespace blink {

// We have two functions to paint text decoations, because we should paint
// text and decorations in following order:
//   1. Paint text decorations except line through
//   2. Paint text
//   3. Paint line throguh
void NGTextPainterBase::PaintDecorationsExceptLineThrough(
    const NGTextFragmentPaintInfo& fragment_paint_info,
    const TextDecorationOffsetBase& decoration_offset,
    TextDecorationInfo& decoration_info,
    TextDecorationLine lines_to_paint,
    const PaintInfo& paint_info,
    const Vector<AppliedTextDecoration>& decorations,
    const TextPaintStyle& text_style,
    const cc::PaintFlags* flags) {
  // Updating the graphics context and looping through applied decorations is
  // expensive, so avoid doing it if there are no decorations of the given
  // |lines_to_paint|, or the only decoration was a ‘line-through’.
  if (!decoration_info.HasAnyLine(lines_to_paint &
                                  ~TextDecorationLine::kLineThrough))
    return;

  GraphicsContext& context = paint_info.context;
  GraphicsContextStateSaver state_saver(context);
  UpdateGraphicsContext(context, text_style, state_saver);

  for (wtf_size_t applied_decoration_index = 0;
       applied_decoration_index < decorations.size();
       ++applied_decoration_index) {
    decoration_info.SetDecorationIndex(applied_decoration_index);
    context.SetStrokeThickness(decoration_info.ResolvedThickness());

    if (decoration_info.HasSpellingOrGrammerError() &&
        EnumHasFlags(lines_to_paint, TextDecorationLine::kSpellingError |
                                         TextDecorationLine::kGrammarError)) {
      decoration_info.SetSpellingOrGrammarErrorLineData(decoration_offset);
      // We ignore "text-decoration-skip-ink: auto" for spelling and grammar
      // error markers.
      AppliedDecorationPainter decoration_painter(context, decoration_info);
      decoration_painter.Paint(flags);
      continue;
    }

    if (decoration_info.HasUnderline() && decoration_info.FontData() &&
        EnumHasFlags(lines_to_paint, TextDecorationLine::kUnderline)) {
      decoration_info.SetUnderlineLineData(decoration_offset);
      PaintDecorationUnderOrOverLine(fragment_paint_info, context,
                                     decoration_info,
                                     TextDecorationLine::kUnderline, flags);
    }

    if (decoration_info.HasOverline() && decoration_info.FontData() &&
        EnumHasFlags(lines_to_paint, TextDecorationLine::kOverline)) {
      decoration_info.SetOverlineLineData(decoration_offset);
      PaintDecorationUnderOrOverLine(fragment_paint_info, context,
                                     decoration_info,
                                     TextDecorationLine::kOverline, flags);
    }
  }
}

void NGTextPainterBase::PaintDecorationUnderOrOverLine(
    const NGTextFragmentPaintInfo& fragment_paint_info,
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
        fragment_paint_info,
        decoration_info.InkSkipClipUpper(decoration_bounds.y()),
        decoration_bounds.height(),
        std::min(decoration_info.ResolvedThickness(),
                 kDecorationClipMaxDilation));
  }
  decoration_painter.Paint(flags);
}

}  // namespace blink
