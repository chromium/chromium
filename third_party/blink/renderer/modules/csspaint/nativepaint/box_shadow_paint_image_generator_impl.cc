// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/nativepaint/box_shadow_paint_image_generator_impl.h"

#include "third_party/blink/renderer/modules/csspaint/nativepaint/box_shadow_paint_definition.h"
#include "third_party/blink/renderer/platform/graphics/image.h"

namespace blink {

BoxShadowPaintImageGenerator* BoxShadowPaintImageGeneratorImpl::Create(
    LocalFrame& local_root) {
  BoxShadowPaintDefinition* box_shadow_paint_definition =
      BoxShadowPaintDefinition::Create(local_root);

  DCHECK(box_shadow_paint_definition);
  BoxShadowPaintImageGeneratorImpl* generator =
      MakeGarbageCollected<BoxShadowPaintImageGeneratorImpl>(
          box_shadow_paint_definition);

  return generator;
}

BoxShadowPaintImageGeneratorImpl::BoxShadowPaintImageGeneratorImpl(
    BoxShadowPaintDefinition* box_shadow_paint_definition)
    : box_shadow_paint_definition_(box_shadow_paint_definition) {}

scoped_refptr<Image> BoxShadowPaintImageGeneratorImpl::Paint() {
  return box_shadow_paint_definition_->Paint();
}

Animation* BoxShadowPaintImageGeneratorImpl::GetAnimationIfCompositable(
    const Element* element) {
  return BoxShadowPaintDefinition::GetAnimationIfCompositable(element);
}

void BoxShadowPaintImageGeneratorImpl::Shutdown() {
  box_shadow_paint_definition_->UnregisterProxyClient();
}

void BoxShadowPaintImageGeneratorImpl::Trace(Visitor* visitor) const {
  visitor->Trace(box_shadow_paint_definition_);
  BoxShadowPaintImageGenerator::Trace(visitor);
}

}  // namespace blink
