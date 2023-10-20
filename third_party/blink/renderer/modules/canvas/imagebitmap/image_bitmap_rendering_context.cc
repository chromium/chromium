// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/imagebitmap/image_bitmap_rendering_context.h"

#include <utility>

#include "third_party/blink/renderer/bindings/modules/v8/v8_union_canvasrenderingcontext2d_gpucanvascontext_imagebitmaprenderingcontext_webgl2renderingcontext_webglrenderingcontext.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_gpucanvascontext_imagebitmaprenderingcontext_offscreencanvasrenderingcontext2d_webgl2renderingcontext_webglrenderingcontext.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"

namespace blink {

ImageBitmapRenderingContext::ImageBitmapRenderingContext(
    CanvasRenderingContextHost* host,
    const CanvasContextCreationAttributesCore& attrs)
    : ImageBitmapRenderingContextBase(host, attrs) {}

ImageBitmapRenderingContext::~ImageBitmapRenderingContext() = default;

V8RenderingContext* ImageBitmapRenderingContext::AsV8RenderingContext() {
  return MakeGarbageCollected<V8RenderingContext>(this);
}

V8OffscreenRenderingContext*
ImageBitmapRenderingContext::AsV8OffscreenRenderingContext() {
  return MakeGarbageCollected<V8OffscreenRenderingContext>(this);
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

ImageBitmap* ImageBitmapRenderingContext::TransferToImageBitmap(
    ScriptState*,
    ExceptionState&) {
  scoped_refptr<StaticBitmapImage> image = GetImageAndResetInternal();
  if (!image)
    return nullptr;

  image->Transfer();
  return MakeGarbageCollected<ImageBitmap>(std::move(image));
}

CanvasRenderingContext* ImageBitmapRenderingContext::Factory::Create(
    CanvasRenderingContextHost* host,
    const CanvasContextCreationAttributesCore& attrs) {
  CanvasRenderingContext* rendering_context =
      MakeGarbageCollected<ImageBitmapRenderingContext>(host, attrs);
  DCHECK(rendering_context);
  return rendering_context;
}

}  // namespace blink
