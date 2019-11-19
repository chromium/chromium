// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/shapedetection/shape_detector.h"

#include <utility>

#include "base/numerics/checked_math.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/html/canvas/image_data.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"

namespace blink {

ScriptPromise ShapeDetector::detect(
    ScriptState* script_state,
    const ImageBitmapSourceUnion& image_source) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  // ImageDatas cannot be tainted by definition.
  if (image_source.IsImageData())
    return DetectShapesOnImageData(resolver, image_source.GetAsImageData());

  CanvasImageSource* canvas_image_source;
  if (image_source.IsHTMLImageElement()) {
    canvas_image_source = image_source.GetAsHTMLImageElement();
  } else if (image_source.IsImageBitmap()) {
    canvas_image_source = image_source.GetAsImageBitmap();
  } else if (image_source.IsHTMLVideoElement()) {
    canvas_image_source = image_source.GetAsHTMLVideoElement();
  } else if (image_source.IsHTMLCanvasElement()) {
    canvas_image_source = image_source.GetAsHTMLCanvasElement();
  } else if (image_source.IsOffscreenCanvas()) {
    canvas_image_source = image_source.GetAsOffscreenCanvas();
  } else {
    NOTREACHED() << "Unsupported CanvasImageSource";
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotSupportedError, "Unsupported source."));
    return promise;
  }

  if (canvas_image_source->WouldTaintOrigin()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kSecurityError, "Source would taint origin."));
    return promise;
  }

  if (image_source.IsHTMLImageElement()) {
    return DetectShapesOnImageElement(resolver,
                                      image_source.GetAsHTMLImageElement());
  }

  // TODO(mcasas): Check if |video| is actually playing a MediaStream by using
  // HTMLMediaElement::isMediaStreamURL(video->currentSrc().getString()); if
  // there is a local WebCam associated, there might be sophisticated ways to
  // detect faces on it. Until then, treat as a normal <video> element.

  const FloatSize size(canvas_image_source->ElementSize(FloatSize()));

  SourceImageStatus source_image_status = kInvalidSourceImageStatus;
  scoped_refptr<Image> image = canvas_image_source->GetSourceImageForCanvas(
      &source_image_status, kPreferNoAcceleration, size);
  if (!image || source_image_status != kNormalSourceImageStatus) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, "Invalid element or state."));
    return promise;
  }
  if (size.IsEmpty()) {
    resolver->Resolve(HeapVector<Member<DOMRect>>());
    return promise;
  }

  // makeNonTextureImage() will make a raster copy of
  // PaintImageForCurrentFrame() if needed, otherwise returning the original
  // SkImage.
  const sk_sp<SkImage> sk_image =
      image->PaintImageForCurrentFrame().GetSkImage()->makeNonTextureImage();

  SkBitmap sk_bitmap;
  if (!sk_image->asLegacyBitmap(&sk_bitmap)) {
    // TODO(mcasas): retrieve the pixels from elsewhere.
    NOTREACHED();
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        "Failed to get pixels for current frame."));
    return promise;
  }

  return DoDetect(resolver, std::move(sk_bitmap));
}

ScriptPromise ShapeDetector::DetectShapesOnImageData(
    ScriptPromiseResolver* resolver,
    ImageData* image_data) {
  ScriptPromise promise = resolver->Promise();

  if (image_data->Size().IsZero()) {
    resolver->Resolve(HeapVector<Member<DOMRect>>());
    return promise;
  }

  if (image_data->BufferBase()->IsDetached()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        "The image data has been detached."));
    return promise;
  }

  SkBitmap sk_bitmap;
  if (!sk_bitmap.tryAllocPixels(
          SkImageInfo::Make(image_data->width(), image_data->height(),
                            kN32_SkColorType, kOpaque_SkAlphaType),
          image_data->width() * 4 /* bytes per pixel */)) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        "Failed to allocate pixels for current frame."));
    return promise;
  }

  base::CheckedNumeric<int> allocation_size = image_data->Size().Area() * 4;
  CHECK_EQ(allocation_size.ValueOrDefault(0), sk_bitmap.computeByteSize());

  memcpy(sk_bitmap.getPixels(), image_data->data()->Data(),
         sk_bitmap.computeByteSize());

  return DoDetect(resolver, std::move(sk_bitmap));
}

ScriptPromise ShapeDetector::DetectShapesOnImageElement(
    ScriptPromiseResolver* resolver,
    const HTMLImageElement* img) {
  ScriptPromise promise = resolver->Promise();

  if (img->BitmapSourceSize().IsZero()) {
    resolver->Resolve(HeapVector<Member<DOMRect>>());
    return promise;
  }

  ImageResourceContent* const image_resource = img->CachedImage();
  if (!image_resource || image_resource->ErrorOccurred()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        "Failed to load or decode HTMLImageElement."));
    return promise;
  }

  Image* const blink_image = image_resource->GetImage();
  if (!blink_image) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        "Failed to get image from resource."));
    return promise;
  }

  const sk_sp<SkImage> sk_image =
      blink_image->PaintImageForCurrentFrame().GetSkImage();
  DCHECK_EQ(img->naturalWidth(), static_cast<unsigned>(sk_image->width()));
  DCHECK_EQ(img->naturalHeight(), static_cast<unsigned>(sk_image->height()));

  SkBitmap sk_bitmap;

  if (!sk_image || !sk_image->asLegacyBitmap(&sk_bitmap)) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        "Failed to get image from current frame."));
    return promise;
  }

  return DoDetect(resolver, std::move(sk_bitmap));
}

}  // namespace blink
