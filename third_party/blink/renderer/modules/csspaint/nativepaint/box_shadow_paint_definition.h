// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_NATIVEPAINT_BOX_SHADOW_PAINT_DEFINITION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_NATIVEPAINT_BOX_SHADOW_PAINT_DEFINITION_H_

#include "third_party/blink/renderer/modules/csspaint/nativepaint/native_paint_definition.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/graphics/image.h"

namespace blink {

class LocalFrame;

class MODULES_EXPORT BoxShadowPaintDefinition final
    : public GarbageCollected<BoxShadowPaintDefinition>,
      public NativePaintDefinition {
 public:
  static BoxShadowPaintDefinition* Create(LocalFrame& local_root);

  explicit BoxShadowPaintDefinition(LocalFrame& local_root);
  ~BoxShadowPaintDefinition() final = default;
  BoxShadowPaintDefinition(const BoxShadowPaintDefinition&) = delete;
  BoxShadowPaintDefinition& operator=(const BoxShadowPaintDefinition&) = delete;

  // PaintDefinition override
  sk_sp<PaintRecord> Paint(
      const CompositorPaintWorkletInput*,
      const CompositorPaintWorkletJob::AnimatedPropertyValues&) override;

  scoped_refptr<Image> Paint();

  void Trace(Visitor* visitor) const override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_NATIVEPAINT_BOX_SHADOW_PAINT_DEFINITION_H_
