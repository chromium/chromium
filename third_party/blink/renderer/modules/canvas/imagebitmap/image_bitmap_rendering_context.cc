// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/imagebitmap/image_bitmap_rendering_context.h"

#include <utility>
#include "third_party/blink/renderer/bindings/modules/v8/offscreen_rendering_context.h"
#include "third_party/blink/renderer/bindings/modules/v8/rendering_context.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"

namespace blink {

ImageBitmapRenderingContext::ImageBitmapRenderingContext(
    CanvasRenderingContextHost* host,
    const CanvasContextCreationAttributesCore& attrs)
    : ImageBitmapRenderingContextBase(host, attrs) {}

ImageBitmapRenderingContext::~ImageBitmapRenderingContext() = default;

void ImageBitmapRenderingContext::SetCanvasGetContextResult(
    RenderingContext& result) {
  result.SetImageBitmapRenderingContext(this);
}
void ImageBitmapRenderingContext::SetOffscreenCanvasGetContextResult(
    OffscreenRenderingContext& result) {
  result.SetImageBitmapRenderingContext(this);
}

void ImageBitmapRenderingContext::transferFromImageBitmap(
    ImageBitmap* image_bitmap,
    ExceptionState& exception_state) {
  if (image_bitmap && image_bitmap->IsNeutered()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The input ImageBitmap has been detached");
    return;
  }

  if (image_bitmap && image_bitmap->WouldTaintOrigin()) {
    Host()->SetOriginTainted();
  }

  SetImage(image_bitmap);
}

ImageBitmap* ImageBitmapRenderingContext::TransferToImageBitmap(ScriptState*) {
  scoped_refptr<StaticBitmapImage> image = GetImageAndResetInternal();
  if (!image)
    return nullptr;

  image->Transfer();
  return ImageBitmap::Create(std::move(image));
}

CanvasRenderingContext* ImageBitmapRenderingContext::Factory::Create(
    CanvasRenderingContextHost* host,
    const CanvasContextCreationAttributesCore& attrs) {
  return MakeGarbageCollected<ImageBitmapRenderingContext>(host, attrs);
}

}  // namespace blink
