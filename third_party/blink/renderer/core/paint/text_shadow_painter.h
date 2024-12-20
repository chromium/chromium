// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_SHADOW_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_SHADOW_PAINTER_H_

#include "third_party/blink/renderer/core/paint/text_paint_style.h"
#include "third_party/blink/renderer/core/paint/text_painter.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class GraphicsContext;

// Helper class that creates a layer on the specified GraphicsContext to apply
// a text-shadow effect based on the TextPaintStyle in the scope of the object.
class ScopedTextShadowPainter {
  STACK_ALLOCATED();

 public:
  ScopedTextShadowPainter(GraphicsContext& context,
                          const TextPaintStyle& text_style,
                          const bool is_horizontal) {
    if (!text_style.shadow) {
      return;
    }
    ApplyShadowList(context, text_style, is_horizontal);
  }
  ~ScopedTextShadowPainter() {
    if (context_) {
      context_->EndLayer();
    }
  }
  bool HasEffectiveShadow() const { return context_; }

 private:
  void ApplyShadowList(GraphicsContext&,
                       const TextPaintStyle&,
                       const bool is_horizontal);

  GraphicsContext* context_ = nullptr;
};

enum class TextShadowPaintPhase {
  kShadow,
  kForeground,
};

template <typename PaintProc>
void PaintWithTextShadow(
    PaintProc paint_proc,
    GraphicsContext& context,
    const TextPaintStyle& text_style,
    const bool is_horizontal,
    const TextPainter::ShadowMode shadow_mode =
        TextPainter::ShadowMode::kBothShadowsAndTextProper) {
  if (text_style.shadow &&
      shadow_mode != TextPainter::ShadowMode::kTextProperOnly) {
    ScopedTextShadowPainter shadow_painter(context, text_style, is_horizontal);
    if (shadow_painter.HasEffectiveShadow()) {
      paint_proc(TextShadowPaintPhase::kShadow);
    }
  }
  if (shadow_mode != TextPainter::ShadowMode::kShadowsOnly) {
    paint_proc(TextShadowPaintPhase::kForeground);
  }
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_SHADOW_PAINTER_H_
