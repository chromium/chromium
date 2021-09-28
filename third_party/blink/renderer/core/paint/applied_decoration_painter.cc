// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/applied_decoration_painter.h"

#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"

namespace blink {

void AppliedDecorationPainter::Paint(const PaintFlags* flags) {
  context_.SetStrokeStyle(decoration_info_.StrokeStyle());
  context_.SetStrokeColor(decoration_info_.LineColor());

  AutoDarkMode auto_dark_mode(PaintAutoDarkMode(
      decoration_info_.Style(), DarkModeFilter::ElementRole::kText));
  switch (decoration_info_.DecorationStyle()) {
    case ETextDecorationStyle::kWavy:
      StrokeWavyTextDecoration(flags);
      break;
    case ETextDecorationStyle::kDotted:
    case ETextDecorationStyle::kDashed:
      context_.SetShouldAntialias(decoration_info_.ShouldAntialias());
      FALLTHROUGH;
    default:
      context_.DrawLineForText(decoration_info_.StartPoint(line_),
                               decoration_info_.Width(), auto_dark_mode, flags);

      if (decoration_info_.DecorationStyle() == ETextDecorationStyle::kDouble) {
        context_.DrawLineForText(
            decoration_info_.StartPoint(line_) +
                FloatPoint(0, decoration_info_.DoubleOffset(line_)),
            decoration_info_.Width(), auto_dark_mode, flags);
      }
  }
}

void AppliedDecorationPainter::StrokeWavyTextDecoration(
    const PaintFlags* flags) {
  context_.SetShouldAntialias(true);
  absl::optional<Path> path = decoration_info_.PrepareWavyStrokePath(line_);
  AutoDarkMode auto_dark_mode(PaintAutoDarkMode(
      decoration_info_.Style(), DarkModeFilter::ElementRole::kText));
  if (flags)
    context_.DrawPath(path->GetSkPath(), *flags, auto_dark_mode);
  else
    context_.StrokePath(path.value(), auto_dark_mode);
}

}  // namespace blink
