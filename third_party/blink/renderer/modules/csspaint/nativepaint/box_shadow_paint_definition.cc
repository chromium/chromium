// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/nativepaint/box_shadow_paint_definition.h"

namespace blink {

// static
BoxShadowPaintDefinition* BoxShadowPaintDefinition::Create(
    LocalFrame& local_root) {
  return MakeGarbageCollected<BoxShadowPaintDefinition>(local_root);
}

BoxShadowPaintDefinition::BoxShadowPaintDefinition(LocalFrame& local_root)
    : NativePaintDefinition(
          &local_root,
          PaintWorkletInput::PaintWorkletInputType::kClipPath) {}

sk_sp<PaintRecord> BoxShadowPaintDefinition::Paint(
    const CompositorPaintWorkletInput* compositor_input,
    const CompositorPaintWorkletJob::AnimatedPropertyValues&
        animated_property_values) {
  // TODO(crbug.com/1258126): implement me.
  return nullptr;
}

scoped_refptr<Image> BoxShadowPaintDefinition::Paint() {
  // TODO(crbug.com/1258126): implement me.
  return nullptr;
}

void BoxShadowPaintDefinition::Trace(Visitor* visitor) const {
  NativePaintDefinition::Trace(visitor);
}

}  // namespace blink
