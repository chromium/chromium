// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/shapedetection/shape_detector.h"

#include <utility>

#include "base/numerics/checked_math.h"
#include "skia/ext/skia_utils_base.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
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
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"

namespace blink {

std::optional<SkBitmap> ShapeDetector::GetBitmapFromSource(
    ScriptState* script_state,
    const V8ImageBitmapSource* image_source,
    ExceptionState& exception_state) {
  DCHECK(image_source);

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
      return GetBitmapFromImageData(
          script_state, image_source->GetAsImageData(), exception_state);
    case V8ImageBitmapSource::ContentType::kOffscreenCanvas:
      canvas_image_source = image_source->GetAsOffscreenCanvas();
      break;
    case V8ImageBitmapSource::ContentType::kBlob:
    case V8ImageBitmapSource::ContentType::kSVGImageElement:
    case V8ImageBitmapSource::ContentType::kVideoFrame:
      exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                        "Unsupported source.");
      return std::nullopt;
  }
  DCHECK(canvas_image_source);

  if (canvas_image_source->IsNeutered()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The image source is detached.");
    return std::nullopt;
  }

  if (canvas_image_source->WouldTaintOrigin()) {
    exception_state.ThrowSecurityError("Source would taint origin.", "");
    return std::nullopt;
  }

  if (image_source->IsHTMLImageElement()) {
    return GetBitmapFromImageElement(
        script_state, image_source->GetAsHTMLImageElement(), exception_state);
  }

  // TODO(mcasas): Check if |video| is actually playing a MediaStream by using
  // HTMLMediaElement::isMediaStreamURL(video->currentSrc().getString()); if
  // there is a local WebCam associated, there might be sophisticated ways to
  // detect faces on it. Until then, treat as a normal <video> element.

  const gfx::SizeF size =
      canvas_image_source->ElementSize(gfx::SizeF(), kRespectImageOrientation);

  SourceImageStatus source_image_status = kInvalidSourceImageStatus;
  scoped_refptr<Image> image = canvas_image_source->GetSourceImageForCanvas(
      FlushReason::kShapeDetector, &source_image_status, size,
      kPremultiplyAlpha);
  if (!image || source_image_status != kNormalSourceImageStatus) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid element or state.");
    return std::nullopt;
  }
  if (size.IsEmpty()) {
    return SkBitmap();
  }

  // GetSwSkImage() will make a raster copy of PaintImageForCurrentFrame()
  // if needed, otherwise returning the original SkImage. May return nullptr
  // if resource allocation failed.
  const sk_sp<SkImage> sk_image =
      image->PaintImageForCurrentFrame().GetSwSkImage();

  SkBitmap sk_bitmap;
  SkBitmap n32_bitmap;
  if (!sk_image || !sk_image->asLegacyBitmap(&sk_bitmap) ||
      !skia::SkBitmapToN32OpaqueOrPremul(sk_bitmap, &n32_bitmap)) {
    // TODO(crbug.com/1467598): retrieve the pixels from elsewhere.
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Failed to get pixels for current frame.");
    return std::nullopt;
  }

  return std::move(n32_bitmap);
}

std::optional<SkBitmap> ShapeDetector::GetBitmapFromImageData(
    ScriptState* script_state,
    ImageData* image_data,
    ExceptionState& exception_state) {
  if (image_data->Size().IsZero()) {
    return SkBitmap();
  }

  if (image_data->IsBufferBaseDetached()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The image data has been detached.");
    return std::nullopt;
  }

  SkPixmap image_data_pixmap = image_data->GetSkPixmap();
  SkBitmap sk_bitmap;
  // Pass 0 for rowBytes to have SkBitmap calculate minimum valid size.
  if (!sk_bitmap.tryAllocPixels(
          image_data_pixmap.info().makeColorType(kN32_SkColorType),
          /*rowBytes=*/0)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Failed to allocate pixels for current frame.");
    return std::nullopt;
  }
  if (!sk_bitmap.writePixels(image_data_pixmap, 0, 0)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Failed to copy pixels for current frame.");
    return std::nullopt;
  }

  return std::move(sk_bitmap);
}

std::optional<SkBitmap> ShapeDetector::GetBitmapFromImageElement(
    ScriptState* script_state,
    const HTMLImageElement* img,
    ExceptionState& exception_state) {
  ImageResourceContent* const image_content = img->CachedImage();
  if (!image_content || !image_content->IsLoaded() ||
      image_content->ErrorOccurred()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Failed to load or decode HTMLImageElement.");
    return std::nullopt;
  }

  if (!image_content->HasImage()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Failed to get image from resource.");
    return std::nullopt;
  }

  Image* const blink_image = image_content->GetImage();
  if (blink_image->Size().IsZero()) {
    return SkBitmap();
  }

  // The call to asLegacyBitmap() below forces a readback so getting SwSkImage
  // here doesn't readback unnecessarily
  const sk_sp<SkImage> sk_image =
      blink_image->PaintImageForCurrentFrame().GetSwSkImage();
  DCHECK_EQ(img->naturalWidth(), static_cast<unsigned>(sk_image->width()));
  DCHECK_EQ(img->naturalHeight(), static_cast<unsigned>(sk_image->height()));

  SkBitmap sk_bitmap;
  if (!sk_image || !sk_image->asLegacyBitmap(&sk_bitmap)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Failed to get image from current frame.");
    return std::nullopt;
  }

  return std::move(sk_bitmap);
}

}  // namespace blink
