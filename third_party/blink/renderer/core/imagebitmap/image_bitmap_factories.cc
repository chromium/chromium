/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/imagebitmap/image_bitmap_factories.h"

#include <memory>

#include "base/location.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/canvas/image_data.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap_options.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image.h"
#include "third_party/blink/renderer/core/svg/svg_image_element.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/histogram.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/blink/renderer/platform/scheduler/public/background_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/shared_buffer.h"
#include "v8/include/v8.h"

namespace blink {

// This enum is used in a UMA histogram.
enum CreateImageBitmapSource {
  kCreateImageBitmapSourceBlob = 0,
  kCreateImageBitmapSourceImageBitmap = 1,
  kCreateImageBitmapSourceImageData = 2,
  kCreateImageBitmapSourceHTMLCanvasElement = 3,
  kCreateImageBitmapSourceHTMLImageElement = 4,
  kCreateImageBitmapSourceHTMLVideoElement = 5,
  kCreateImageBitmapSourceOffscreenCanvas = 6,
  kCreateImageBitmapSourceSVGImageElement = 7,
  kCreateImageBitmapSourceCount,
};

static inline ImageBitmapSource* ToImageBitmapSourceInternal(
    const ImageBitmapSourceUnion& value,
    const ImageBitmapOptions& options,
    bool has_crop_rect) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      EnumerationHistogram, image_bitmap_source_histogram,
      ("Canvas.CreateImageBitmapSource", kCreateImageBitmapSourceCount));
  if (value.IsHTMLVideoElement()) {
    image_bitmap_source_histogram.Count(
        kCreateImageBitmapSourceHTMLVideoElement);
    return value.GetAsHTMLVideoElement();
  }
  if (value.IsHTMLImageElement()) {
    image_bitmap_source_histogram.Count(
        kCreateImageBitmapSourceHTMLImageElement);
    return value.GetAsHTMLImageElement();
  }
  if (value.IsSVGImageElement()) {
    image_bitmap_source_histogram.Count(
        kCreateImageBitmapSourceSVGImageElement);
    return value.GetAsSVGImageElement();
  }
  if (value.IsHTMLCanvasElement()) {
    image_bitmap_source_histogram.Count(
        kCreateImageBitmapSourceHTMLCanvasElement);
    return value.GetAsHTMLCanvasElement();
  }
  if (value.IsBlob()) {
    image_bitmap_source_histogram.Count(kCreateImageBitmapSourceBlob);
    return value.GetAsBlob();
  }
  if (value.IsImageData()) {
    image_bitmap_source_histogram.Count(kCreateImageBitmapSourceImageData);
    return value.GetAsImageData();
  }
  if (value.IsImageBitmap()) {
    image_bitmap_source_histogram.Count(kCreateImageBitmapSourceImageBitmap);
    return value.GetAsImageBitmap();
  }
  if (value.IsOffscreenCanvas()) {
    image_bitmap_source_histogram.Count(
        kCreateImageBitmapSourceOffscreenCanvas);
    return value.GetAsOffscreenCanvas();
  }
  NOTREACHED();
  return nullptr;
}

ScriptPromise ImageBitmapFactories::CreateImageBitmapFromBlob(
    ScriptState* script_state,
    EventTarget& event_target,
    ImageBitmapSource* bitmap_source,
    base::Optional<IntRect> crop_rect,
    const ImageBitmapOptions& options) {
  Blob* blob = static_cast<Blob*>(bitmap_source);
  ImageBitmapLoader* loader = ImageBitmapFactories::ImageBitmapLoader::Create(
      From(event_target), crop_rect, options, script_state);
  ScriptPromise promise = loader->Promise();
  From(event_target).AddLoader(loader);
  loader->LoadBlobAsync(blob);
  return promise;
}

ScriptPromise ImageBitmapFactories::createImageBitmap(
    ScriptState* script_state,
    EventTarget& event_target,
    const ImageBitmapSourceUnion& bitmap_source,
    const ImageBitmapOptions& options) {
  WebFeature feature = WebFeature::kCreateImageBitmap;
  UseCounter::Count(ExecutionContext::From(script_state), feature);
  ImageBitmapSource* bitmap_source_internal =
      ToImageBitmapSourceInternal(bitmap_source, options, false);
  if (!bitmap_source_internal)
    return ScriptPromise();
  return createImageBitmap(script_state, event_target, bitmap_source_internal,
                           base::Optional<IntRect>(), options);
}

ScriptPromise ImageBitmapFactories::createImageBitmap(
    ScriptState* script_state,
    EventTarget& event_target,
    const ImageBitmapSourceUnion& bitmap_source,
    int sx,
    int sy,
    int sw,
    int sh,
    const ImageBitmapOptions& options) {
  WebFeature feature = WebFeature::kCreateImageBitmap;
  UseCounter::Count(ExecutionContext::From(script_state), feature);
  ImageBitmapSource* bitmap_source_internal =
      ToImageBitmapSourceInternal(bitmap_source, options, true);
  if (!bitmap_source_internal)
    return ScriptPromise();
  base::Optional<IntRect> crop_rect = IntRect(sx, sy, sw, sh);
  return createImageBitmap(script_state, event_target, bitmap_source_internal,
                           crop_rect, options);
}

ScriptPromise ImageBitmapFactories::createImageBitmap(
    ScriptState* script_state,
    EventTarget& event_target,
    ImageBitmapSource* bitmap_source,
    base::Optional<IntRect> crop_rect,
    const ImageBitmapOptions& options) {
  if (crop_rect && (crop_rect->Width() == 0 || crop_rect->Height() == 0)) {
    return ScriptPromise::Reject(
        script_state,
        V8ThrowException::CreateRangeError(
            script_state->GetIsolate(),
            String::Format("The crop rect %s is 0.",
                           crop_rect->Width() ? "height" : "width")));
  }

  if (bitmap_source->IsBlob()) {
    return CreateImageBitmapFromBlob(script_state, event_target, bitmap_source,
                                     crop_rect, options);
  }

  if (bitmap_source->BitmapSourceSize().Width() == 0 ||
      bitmap_source->BitmapSourceSize().Height() == 0) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(
            DOMExceptionCode::kInvalidStateError,
            String::Format("The source image %s is 0.",
                           bitmap_source->BitmapSourceSize().Width()
                               ? "height"
                               : "width")));
  }

  return bitmap_source->CreateImageBitmap(script_state, event_target, crop_rect,
                                          options);
}

const char ImageBitmapFactories::kSupplementName[] = "ImageBitmapFactories";

ImageBitmapFactories& ImageBitmapFactories::From(EventTarget& event_target) {
  if (LocalDOMWindow* window = event_target.ToLocalDOMWindow())
    return FromInternal(*window);

  return ImageBitmapFactories::FromInternal(
      *To<WorkerGlobalScope>(event_target.GetExecutionContext()));
}

template <class GlobalObject>
ImageBitmapFactories& ImageBitmapFactories::FromInternal(GlobalObject& object) {
  ImageBitmapFactories* supplement =
      Supplement<GlobalObject>::template From<ImageBitmapFactories>(object);
  if (!supplement) {
    supplement = new ImageBitmapFactories;
    Supplement<GlobalObject>::ProvideTo(object, supplement);
  }
  return *supplement;
}

void ImageBitmapFactories::AddLoader(ImageBitmapLoader* loader) {
  pending_loaders_.insert(loader);
}

void ImageBitmapFactories::DidFinishLoading(ImageBitmapLoader* loader) {
  DCHECK(pending_loaders_.Contains(loader));
  pending_loaders_.erase(loader);
}

ImageBitmapFactories::ImageBitmapLoader::ImageBitmapLoader(
    ImageBitmapFactories& factory,
    base::Optional<IntRect> crop_rect,
    ScriptState* script_state,
    const ImageBitmapOptions& options)
    : loader_(
          FileReaderLoader::Create(FileReaderLoader::kReadAsArrayBuffer, this)),
      factory_(&factory),
      resolver_(ScriptPromiseResolver::Create(script_state)),
      crop_rect_(crop_rect),
      options_(options) {}

void ImageBitmapFactories::ImageBitmapLoader::LoadBlobAsync(
    Blob* blob) {
  loader_->Start(blob->GetBlobDataHandle());
}

void ImageBitmapFactories::Trace(blink::Visitor* visitor) {
  visitor->Trace(pending_loaders_);
  Supplement<LocalDOMWindow>::Trace(visitor);
  Supplement<WorkerGlobalScope>::Trace(visitor);
}

void ImageBitmapFactories::ImageBitmapLoader::RejectPromise(
    ImageBitmapRejectionReason reason) {
  switch (reason) {
    case kUndecodableImageBitmapRejectionReason:
      resolver_->Reject(
          DOMException::Create(DOMExceptionCode::kInvalidStateError,
                               "The source image could not be decoded."));
      break;
    case kAllocationFailureImageBitmapRejectionReason:
      resolver_->Reject(
          DOMException::Create(DOMExceptionCode::kInvalidStateError,
                               "The ImageBitmap could not be allocated."));
      break;
    default:
      NOTREACHED();
  }
  factory_->DidFinishLoading(this);
}

void ImageBitmapFactories::ImageBitmapLoader::DidFinishLoading() {
  DOMArrayBuffer* array_buffer = loader_->ArrayBufferResult();
  if (!array_buffer) {
    RejectPromise(kAllocationFailureImageBitmapRejectionReason);
    return;
  }
  ScheduleAsyncImageBitmapDecoding(array_buffer);
}

void ImageBitmapFactories::ImageBitmapLoader::DidFail(FileError::ErrorCode) {
  RejectPromise(kUndecodableImageBitmapRejectionReason);
}

void ImageBitmapFactories::ImageBitmapLoader::ScheduleAsyncImageBitmapDecoding(
    DOMArrayBuffer* array_buffer) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      Platform::Current()->CurrentThread()->GetTaskRunner();
  background_scheduler::PostOnBackgroundThread(
      FROM_HERE,
      CrossThreadBind(
          &ImageBitmapFactories::ImageBitmapLoader::DecodeImageOnDecoderThread,
          WrapCrossThreadPersistent(this), std::move(task_runner),
          WrapCrossThreadPersistent(array_buffer), options_.premultiplyAlpha(),
          options_.colorSpaceConversion()));
}

void ImageBitmapFactories::ImageBitmapLoader::DecodeImageOnDecoderThread(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    DOMArrayBuffer* array_buffer,
    const String& premultiply_alpha_option,
    const String& color_space_conversion_option) {
  DCHECK(!IsMainThread());

  ImageDecoder::AlphaOption alpha_op = ImageDecoder::kAlphaPremultiplied;
  if (premultiply_alpha_option == "none")
    alpha_op = ImageDecoder::kAlphaNotPremultiplied;
  bool ignore_color_space = false;
  if (color_space_conversion_option == "none")
    ignore_color_space = true;
  const bool data_complete = true;
  std::unique_ptr<ImageDecoder> decoder(ImageDecoder::Create(
      SegmentReader::CreateFromSkData(SkData::MakeWithoutCopy(
          array_buffer->Data(), array_buffer->ByteLength())),
      data_complete, alpha_op, ImageDecoder::kDefaultBitDepth,
      ignore_color_space ? ColorBehavior::Ignore() : ColorBehavior::Tag()));
  sk_sp<SkImage> frame;
  if (decoder) {
    frame = ImageBitmap::GetSkImageFromDecoder(std::move(decoder));
  }
  PostCrossThreadTask(
      *task_runner, FROM_HERE,
      CrossThreadBind(&ImageBitmapFactories::ImageBitmapLoader::
                          ResolvePromiseOnOriginalThread,
                      WrapCrossThreadPersistent(this), std::move(frame)));
}

void ImageBitmapFactories::ImageBitmapLoader::ResolvePromiseOnOriginalThread(
    sk_sp<SkImage> frame) {
  if (!frame) {
    RejectPromise(kUndecodableImageBitmapRejectionReason);
    return;
  }
  DCHECK(frame->width());
  DCHECK(frame->height());

  scoped_refptr<StaticBitmapImage> image =
      StaticBitmapImage::Create(std::move(frame));
  image->SetOriginClean(true);
  ImageBitmap* image_bitmap = ImageBitmap::Create(image, crop_rect_, options_);
  if (image_bitmap && image_bitmap->BitmapImage()) {
    resolver_->Resolve(image_bitmap);
  } else {
    RejectPromise(kAllocationFailureImageBitmapRejectionReason);
    return;
  }
  factory_->DidFinishLoading(this);
}

void ImageBitmapFactories::ImageBitmapLoader::Trace(blink::Visitor* visitor) {
  visitor->Trace(factory_);
  visitor->Trace(resolver_);
}

}  // namespace blink
