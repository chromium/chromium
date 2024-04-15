// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/nativepaint/clip_path_paint_image_generator_impl.h"

#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/modules/csspaint/nativepaint/clip_path_paint_definition.h"
#include "third_party/blink/renderer/platform/graphics/image.h"

namespace blink {

ClipPathPaintImageGenerator* ClipPathPaintImageGeneratorImpl::Create(
    LocalFrame& local_root) {
  ClipPathPaintDefinition* clip_path_paint_definition =
      ClipPathPaintDefinition::Create(local_root);

  ClipPathPaintImageGeneratorImpl* generator =
      MakeGarbageCollected<ClipPathPaintImageGeneratorImpl>(
          clip_path_paint_definition);

  return generator;
}

ClipPathPaintImageGeneratorImpl::ClipPathPaintImageGeneratorImpl(
    ClipPathPaintDefinition* clip_path_paint_definition)
    : clip_path_paint_definition_(clip_path_paint_definition) {}

scoped_refptr<Image> ClipPathPaintImageGeneratorImpl::Paint(
    float zoom,
    const gfx::RectF& reference_box,
    const gfx::SizeF& clip_area_size,
    const Node& node) {
  return ClipPathPaintDefinition::Paint(
      zoom, reference_box, clip_area_size, node,
      clip_path_paint_definition_->GetWorkletId());
}

gfx::RectF ClipPathPaintImageGeneratorImpl::ClipAreaRect(
    const Node& node,
    const gfx::RectF& reference_box,
    float zoom) const {
  return ClipPathPaintDefinition::ClipAreaRect(node, reference_box, zoom);
}

Animation* ClipPathPaintImageGeneratorImpl::GetAnimationIfCompositable(
    const Element* element) {
  return ClipPathPaintDefinition::GetAnimationIfCompositable(element);
}

void ClipPathPaintImageGeneratorImpl::Shutdown() {
  clip_path_paint_definition_->UnregisterProxyClient();
}

void ClipPathPaintImageGeneratorImpl::Trace(Visitor* visitor) const {
  visitor->Trace(clip_path_paint_definition_);
  ClipPathPaintImageGenerator::Trace(visitor);
}

}  // namespace blink
