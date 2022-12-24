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

// We have two functions to paint text decorations, because we should paint
// text and decorations in following order:
//   1. Paint underline or overline text decorations
//   2. Paint text
//   3. Paint line through
void NGTextPainterBase::PaintUnderOrOverLineDecorations(
    const NGTextFragmentPaintInfo& fragment_paint_info,
    const TextDecorationOffsetBase& decoration_offset,
    TextDecorationInfo& decoration_info,
    TextDecorationLine lines_to_paint,
    const PaintInfo& paint_info,
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

  // Updating Graphics Context for text only (kTextProperOnly),
  // instead of the default text and shadows (kBothShadowsAndTextProper),
  // because shadows will be painted by
  // NGTextPainterBase::PaintUnderOrOverLineDecorationShadows.
  UpdateGraphicsContext(context, text_style, state_saver,
                        ShadowMode::kTextProperOnly);

  PaintUnderOrOverLineDecorationShadows(fragment_paint_info, decoration_offset,
                                        decoration_info, lines_to_paint, flags,
                                        text_style, context);

  PaintUnderOrOverLineDecorations(fragment_paint_info, decoration_offset,
                                  decoration_info, lines_to_paint, flags,
                                  context);
}

void NGTextPainterBase::PaintUnderOrOverLineDecorationShadows(
    const NGTextFragmentPaintInfo& fragment_paint_info,
    const TextDecorationOffsetBase& decoration_offset,
    TextDecorationInfo& decoration_info,
    TextDecorationLine lines_to_paint,
    const cc::PaintFlags* flags,
    const TextPaintStyle& text_style,
    GraphicsContext& context) {
  if (text_style.shadow == nullptr)
    return;

  const ShadowList* shadow_list = text_style.shadow.get();
  if (shadow_list == nullptr)
    return;

  for (const auto& shadow : shadow_list->Shadows()) {
    const Color& color = shadow.GetColor().Resolve(text_style.current_color,
                                                   text_style.color_scheme);
    // Detect when there's no effective shadow.
    if (color.IsTransparent())
      continue;

    const gfx::Vector2dF& offset = shadow.Location().OffsetFromOrigin();

    float blur = shadow.Blur();
    DCHECK_GE(blur, 0);
    const auto sigma = BlurRadiusToStdDev(blur);

    context.BeginLayer(sk_make_sp<DropShadowPaintFilter>(
        offset.x(), offset.y(), sigma, sigma, color.toSkColor4f(),
        DropShadowPaintFilter::ShadowMode::kDrawShadowOnly, nullptr));

    PaintUnderOrOverLineDecorations(fragment_paint_info, decoration_offset,
                                    decoration_info, lines_to_paint, flags,
                                    context);

    context.EndLayer();
  }
}

void NGTextPainterBase::PaintUnderOrOverLineDecorations(
    const NGTextFragmentPaintInfo& fragment_paint_info,
    const TextDecorationOffsetBase& decoration_offset,
    TextDecorationInfo& decoration_info,
    TextDecorationLine lines_to_paint,
    const cc::PaintFlags* flags,
    GraphicsContext& context) {
  for (wtf_size_t i = 0; i < decoration_info.AppliedDecorationCount(); i++) {
    decoration_info.SetDecorationIndex(i);
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
