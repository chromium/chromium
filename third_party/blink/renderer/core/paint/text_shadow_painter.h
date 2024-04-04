// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_SHADOW_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_SHADOW_PAINTER_H_

#include "third_party/blink/renderer/core/paint/text_paint_style.h"
#include "third_party/blink/renderer/core/paint/text_painter.h"
#include "third_party/blink/renderer/platform/graphics/draw_looper_builder.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"

namespace blink {

class GraphicsContext;

enum class TextShadowPaintPhase {
  kShadow,
  kForeground,
};

template <typename PaintProc>
void PaintWithTextShadow(PaintProc paint_proc,
                         GraphicsContext& context,
                         const TextPaintStyle& text_style) {
  if (text_style.shadow) {
    context.SetDrawLooper(TextPainter::CreateDrawLooper(
        text_style.shadow.get(), DrawLooperBuilder::kShadowIgnoresAlpha,
        text_style.current_color, text_style.color_scheme,
        TextPainter::kBothShadowsAndTextProper));
  }
  paint_proc(TextShadowPaintPhase::kForeground);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_SHADOW_PAINTER_H_
