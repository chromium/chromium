// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/shapedetection/shape_detector.h"

#include <utility>

#include "base/numerics/checked_math.h"
#include "skia/ext/skia_utils_base.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_blob_htmlcanvaselement_htmlimageelement_htmlvideoelement_imagebitmap_imagedata_offscreencanvas_svgimageelement_videoframe.h"
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
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"

namespace blink {

ScriptPromise ShapeDetector::detect(ScriptState* script_state,
                                    const V8ImageBitmapSource* image_source) {
  DCHECK(image_source);

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  CanvasImageSource* canvas_image_source = nullptr;
  switch (image_source->GetContentType()) {
    case V8ImageBitmapSource::ContentType::kHTMLCanvasElement:
      canvas_image_source = image_source->GetAsHTMLCanvasElement();
      break;
    case V8ImageBitmapSource::ContentType::kHTMLImageElement:
      canvas_image_source = image_source->GetAsHTMLImageElement();
      break;
    case V8ImageBitmapSource::ContentType::kHTMLVideoElement:
      canvas_image_source = image_source->GetAsHTMLVideoElement();
      break;
    case V8ImageBitmapSource::ContentType::kImageBitmap:
      canvas_image_source = image_source->GetAsImageBitmap();
      break;
    case V8ImageBitmapSource::ContentType::kImageData:
      // ImageData cannot be tainted by definition.
      return DetectShapesOnImageData(resolver, image_source->GetAsImageData());
    case V8ImageBitmapSource::ContentType::kOffscreenCanvas:
      canvas_image_source = image_source->GetAsOffscreenCanvas();
      break;
    case V8ImageBitmapSource::ContentType::kBlob:
    case V8ImageBitmapSource::ContentType::kSVGImageElement:
    case V8ImageBitmapSource::ContentType::kVideoFrame:
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError, "Unsupported source."));
      return promise;
  }
  DCHECK(canvas_image_source);

  if (canvas_image_source->IsNeutered()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, "The image source is detached."));
    return promise;
  }

  if (canvas_image_source->WouldTaintOrigin()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kSecurityError, "Source would taint origin."));
    return promise;
  }

  if (image_source->IsHTMLImageElement()) {
    return DetectShapesOnImageElement(resolver,
                                      image_source->GetAsHTMLImageElement());
  }

  // TODO(mcasas): Check if |video| is actually playing a MediaStream by using
  // HTMLMediaElement::isMediaStreamURL(video->currentSrc().getString()); if
  // there is a local WebCam associated, there might be sophisticated ways to
  // detect faces on it. Until then, treat as a normal <video> element.

  const gfx::SizeF size =
      canvas_image_source->ElementSize(gfx::SizeF(), kRespectImageOrientation);

  SourceImageStatus source_image_status = kInvalidSourceImageStatus;
  scoped_refptr<Image> image =
      canvas_image_source->GetSourceImageForCanvas(&source_image_status, size);
  if (!image || source_image_status != kNormalSourceImageStatus) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, "Invalid element or state."));
    return promise;
  }
  if (size.IsEmpty()) {
    resolver->Resolve(HeapVector<Member<DOMRect>>());
    return promise;
  }

  // GetSwSkImage() will make a raster copy of PaintImageForCurrentFrame()
  // if needed, otherwise returning the original SkImage.
  const sk_sp<SkImage> sk_image =
      image->PaintImageForCurrentFrame().GetSwSkImage();

  SkBitmap sk_bitmap;
  SkBitmap n32_bitmap;
  if (!sk_image->asLegacyBitmap(&sk_bitmap) ||
      !skia::SkBitmapToN32OpaqueOrPremul(sk_bitmap, &n32_bitmap)) {
    // TODO(mcasas): retrieve the pixels from elsewhere.
    NOTREACHED();
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        "Failed to get pixels for current frame."));
    return promise;
  }

  return DoDetect(resolver, std::move(n32_bitmap));
}

ScriptPromise ShapeDetector::DetectShapesOnImageData(
    ScriptPromiseResolver* resolver,
    ImageData* image_data) {
  ScriptPromise promise = resolver->Promise();

  if (image_data->Size().IsZero()) {
    resolver->Resolve(HeapVector<Member<DOMRect>>());
    return promise;
  }

  if (image_data->IsBufferBaseDetached()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        "The image data has been detached."));
    return promise;
  }

  SkPixmap image_data_pixmap = image_data->GetSkPixmap();
  SkBitmap sk_bitmap;
  if (!sk_bitmap.tryAllocPixels(
          image_data_pixmap.info().makeColorType(kN32_SkColorType),
          image_data_pixmap.rowBytes())) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        "Failed to allocate pixels for current frame."));
    return promise;
  }
  if (!sk_bitmap.writePixels(image_data_pixmap, 0, 0)) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        "Failed to copy pixels for current frame."));
    return promise;
  }

  return DoDetect(resolver, std::move(sk_bitmap));
}

ScriptPromise ShapeDetector::DetectShapesOnImageElement(
    ScriptPromiseResolver* resolver,
    const HTMLImageElement* img) {
  ScriptPromise promise = resolver->Promise();

  ImageResourceContent* const image_content = img->CachedImage();
  if (!image_content || !image_content->IsLoaded() ||
      image_content->ErrorOccurred()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        "Failed to load or decode HTMLImageElement."));
    return promise;
  }

  if (!image_content->HasImage()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        "Failed to get image from resource."));
    return promise;
  }

  Image* const blink_image = image_content->GetImage();
  if (blink_image->Size().IsZero()) {
    resolver->Resolve(HeapVector<Member<DOMRect>>());
    return promise;
  }

  // The call to asLegacyBitmap() below forces a readback so getting SwSkImage
  // here doesn't readback unnecessarily
  const sk_sp<SkImage> sk_image =
      blink_image->PaintImageForCurrentFrame().GetSwSkImage();
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
