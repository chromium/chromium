// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/applied_decoration_painter.h"

#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"

namespace blink {

void AppliedDecorationPainter::Paint(const cc::PaintFlags* flags) {
  context_.SetStrokeStyle(decoration_info_.StrokeStyle());
  context_.SetStrokeColor(decoration_info_.LineColor());

  AutoDarkMode auto_dark_mode(
      PaintAutoDarkMode(decoration_info_.TargetStyle(),
                        DarkModeFilter::ElementRole::kForeground));
  switch (decoration_info_.DecorationStyle()) {
    case ETextDecorationStyle::kWavy:
      StrokeWavyTextDecoration(flags);
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

void AppliedDecorationPainter::StrokeWavyTextDecoration(
    const cc::PaintFlags* flags) {
  // We need this because of the clipping we're doing below, as we paint both
  // overlines and underlines here. That clip would hide the overlines, when
  // painting the underlines.
  GraphicsContextStateSaver state_saver(context_);

  context_.SetShouldAntialias(true);

  // The wavy line is larger than the line, as we add whole waves before and
  // after the line in TextDecorationInfo::PrepareWavyStrokePath().
  context_.Clip(decoration_info_.Bounds());

  absl::optional<Path> path = decoration_info_.StrokePath();
  AutoDarkMode auto_dark_mode(
      PaintAutoDarkMode(decoration_info_.TargetStyle(),
                        DarkModeFilter::ElementRole::kForeground));
  if (flags)
    context_.DrawPath(path->GetSkPath(), *flags, auto_dark_mode);
  else
    context_.StrokePath(path.value(), auto_dark_mode);
}

}  // namespace blink
