// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_APPLIED_DECORATION_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_APPLIED_DECORATION_PAINTER_H_

#include "cc/paint/paint_flags.h"
#include "third_party/blink/renderer/core/paint/text_decoration_info.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class GraphicsContext;

// Helper class for painting a text decorations. Each instance paints a single
// decoration.
class AppliedDecorationPainter final {
  STACK_ALLOCATED();

 public:
  AppliedDecorationPainter(GraphicsContext& context,
                           const TextDecorationInfo& decoration_info)
      : context_(context), decoration_info_(decoration_info) {}

  void Paint(const cc::PaintFlags* flags = nullptr);

 private:
  void PaintWavyTextDecoration();

  GraphicsContext& context_;
  const TextDecorationInfo& decoration_info_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_APPLIED_DECORATION_PAINTER_H_
