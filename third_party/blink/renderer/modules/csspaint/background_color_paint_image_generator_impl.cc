// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/background_color_paint_image_generator_impl.h"

#include "third_party/blink/renderer/modules/csspaint/background_color_paint_worklet.h"
#include "third_party/blink/renderer/platform/graphics/image.h"

namespace blink {

BackgroundColorPaintImageGenerator*
BackgroundColorPaintImageGeneratorImpl::Create(LocalFrame& local_root) {
  BackgroundColorPaintWorklet* background_color_paint_worklet =
      BackgroundColorPaintWorklet::Create(local_root);

  DCHECK(background_color_paint_worklet);
  BackgroundColorPaintImageGeneratorImpl* generator =
      MakeGarbageCollected<BackgroundColorPaintImageGeneratorImpl>(
          background_color_paint_worklet);

  return generator;
}

BackgroundColorPaintImageGeneratorImpl::BackgroundColorPaintImageGeneratorImpl(
    BackgroundColorPaintWorklet* background_color_paint_worklet)
    : background_color_paint_worklet_(background_color_paint_worklet) {}

scoped_refptr<Image> BackgroundColorPaintImageGeneratorImpl::Paint(
    const FloatSize& container_size,
    SkColor color) {
  return background_color_paint_worklet_->Paint(container_size, color);
}

void BackgroundColorPaintImageGeneratorImpl::Shutdown() {
  background_color_paint_worklet_->UnregisterProxyClient();
}

void BackgroundColorPaintImageGeneratorImpl::Trace(Visitor* visitor) const {
  visitor->Trace(background_color_paint_worklet_);
  BackgroundColorPaintImageGenerator::Trace(visitor);
}

}  // namespace blink
