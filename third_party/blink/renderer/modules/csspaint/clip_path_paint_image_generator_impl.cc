// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/clip_path_paint_image_generator_impl.h"

#include "third_party/blink/renderer/modules/csspaint/clip_path_paint_worklet.h"
#include "third_party/blink/renderer/platform/graphics/image.h"

namespace blink {

ClipPathPaintImageGenerator* ClipPathPaintImageGeneratorImpl::Create(
    LocalFrame& local_root) {
  ClipPathPaintWorklet* clip_path_paint_worklet =
      ClipPathPaintWorklet::Create(local_root);

  DCHECK(clip_path_paint_worklet);
  ClipPathPaintImageGeneratorImpl* generator =
      MakeGarbageCollected<ClipPathPaintImageGeneratorImpl>(
          clip_path_paint_worklet);

  return generator;
}

ClipPathPaintImageGeneratorImpl::ClipPathPaintImageGeneratorImpl(
    ClipPathPaintWorklet* clip_path_paint_worklet)
    : clip_path_paint_worklet_(clip_path_paint_worklet) {}

scoped_refptr<Image> ClipPathPaintImageGeneratorImpl::Paint(
    float zoom,
    const FloatRect& reference_box,
    const Node& node) {
  return clip_path_paint_worklet_->Paint(zoom, reference_box, node);
}

void ClipPathPaintImageGeneratorImpl::Shutdown() {
  clip_path_paint_worklet_->UnregisterProxyClient();
}

void ClipPathPaintImageGeneratorImpl::Trace(Visitor* visitor) const {
  visitor->Trace(clip_path_paint_worklet_);
  ClipPathPaintImageGenerator::Trace(visitor);
}

}  // namespace blink
