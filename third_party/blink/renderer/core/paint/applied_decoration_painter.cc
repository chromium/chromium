// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/applied_decoration_painter.h"

#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_shader.h"

namespace blink {

void AppliedDecorationPainter::Paint(const cc::PaintFlags* flags) {
  ETextDecorationStyle decoration_style = decoration_info_.DecorationStyle();

  context_.SetStrokeStyle(decoration_info_.StrokeStyle());
  context_.SetStrokeColor(decoration_info_.LineColor());

  AutoDarkMode auto_dark_mode(
      PaintAutoDarkMode(decoration_info_.TargetStyle(),
                        DarkModeFilter::ElementRole::kForeground));

  // TODO(crbug.com/1346281) make other decoration styles work with PaintFlags
  switch (decoration_style) {
    case ETextDecorationStyle::kWavy:
      PaintWavyTextDecoration();
      break;
    case ETextDecorationStyle::kDotted:
    case ETextDecorationStyle::kDashed:
      context_.SetShouldAntialias(decoration_info_.ShouldAntialias());
      [[fallthrough]];
    default:
      context_.DrawLineForText(decoration_info_.StartPoint(),
                               decoration_info_.Width(), auto_dark_mode, flags);

      if (decoration_info_.DecorationStyle() == ETextDecorationStyle::kDouble) {
        context_.DrawLineForText(
            decoration_info_.StartPoint() +
                gfx::Vector2dF(0, decoration_info_.DoubleOffset()),
            decoration_info_.Width(), auto_dark_mode, flags);
      }
  }
}

void AppliedDecorationPainter::PaintWavyTextDecoration() {
  // We need this because of the clipping we're doing below, as we paint both
  // overlines and underlines here. That clip would hide the overlines, when
  // painting the underlines.
  GraphicsContextStateSaver state_saver(context_);

  context_.SetShouldAntialias(true);

  // The wavy line is larger than the line, as we add whole waves before and
  // after the line in TextDecorationInfo::PrepareWavyStrokePath().
  gfx::PointF origin = decoration_info_.Bounds().origin();

  AutoDarkMode auto_dark_mode(
      PaintAutoDarkMode(decoration_info_.TargetStyle(),
                        DarkModeFilter::ElementRole::kForeground));
  cc::PaintFlags flags;

  flags.setAntiAlias(true);
  flags.setShader(PaintShader::MakePaintRecord(
      decoration_info_.WavyTileRecord(),
      gfx::RectFToSkRect(decoration_info_.WavyTileRect()), SkTileMode::kRepeat,
      SkTileMode::kDecal, nullptr));
  context_.Translate(origin.x(), origin.y());
  context_.DrawRect(gfx::RectFToSkRect(decoration_info_.WavyPaintRect()), flags,
                    auto_dark_mode);
}

}  // namespace blink
