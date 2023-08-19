// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/nativepaint/box_shadow_paint_definition.h"
#include "third_party/blink/renderer/core/animation/css/compositor_keyframe_double.h"
#include "third_party/blink/renderer/core/animation/css_color_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/css/css_color.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"

namespace blink {

// static
BoxShadowPaintDefinition* BoxShadowPaintDefinition::Create(
    LocalFrame& local_root) {
  return MakeGarbageCollected<BoxShadowPaintDefinition>(local_root);
}

BoxShadowPaintDefinition::BoxShadowPaintDefinition(LocalFrame& local_root)
    : NativeCssPaintDefinition(
          &local_root,
          PaintWorkletInput::PaintWorkletInputType::kClipPath) {}

PaintRecord BoxShadowPaintDefinition::Paint(
    const CompositorPaintWorkletInput* compositor_input,
    const CompositorPaintWorkletJob::AnimatedPropertyValues&
        animated_property_values) {
  // TODO(crbug.com/1258126): implement me.
  return PaintRecord();
}

scoped_refptr<Image> BoxShadowPaintDefinition::Paint() {
  // TODO(crbug.com/1258126): implement me.
  return nullptr;
}

Animation* BoxShadowPaintDefinition::GetAnimationIfCompositable(
    const Element* element) {
  return GetAnimationForProperty(element, GetCSSPropertyBoxShadow());
}

void GetCompositorKeyframeOffset(const PropertySpecificKeyframe* frame,
                                 Vector<double>* offsets) {
  const CompositorKeyframeDouble& value =
      To<CompositorKeyframeDouble>(*(frame->GetCompositorKeyframeValue()));
  offsets->push_back(value.ToDouble());
}

void BoxShadowPaintDefinition::Trace(Visitor* visitor) const {
  NativePaintDefinition::Trace(visitor);
}

}  // namespace blink
