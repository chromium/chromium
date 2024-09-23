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

#include "third_party/blink/renderer/modules/canvas/imagebitmap/image_bitmap_factories.h"

#include <memory>
#include <utility>

#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_image_bitmap_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_blob_htmlcanvaselement_htmlimageelement_htmlvideoelement_imagebitmap_imagedata_offscreencanvas_svgimageelement_videoframe.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/frame/deprecation/deprecation.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/canvas/image_data.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/core/svg/svg_image_element.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_handle.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_skia.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "v8/include/v8.h"

namespace blink {

namespace {
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
  kCreateImageBitmapSourceVideoFrame = 8,
  kMaxValue = kCreateImageBitmapSourceVideoFrame,
};

constexpr const char* kImageBitmapOptionNone = "none";

gfx::Rect NormalizedCropRect(int x, int y, int width, int height) {
  if (width < 0) {
    x = base::ClampAdd(x, width);
    width = base::ClampSub(0, width);
  }
  if (height < 0) {
    y = base::ClampAdd(y, height);
    height = base::ClampSub(0, height);
  }
  return gfx::Rect(x, y, width, height);
}

}  // namespace

inline ImageBitmapSource* ToImageBitmapSourceInternal(
    const V8ImageBitmapSource* value,
    const ImageBitmapOptions* options,
    bool has_crop_rect) {
  DCHECK(value);

  switch (value->GetContentType()) {
    case V8ImageBitmapSource::ContentType::kBlob:
      UMA_HISTOGRAM_ENUMERATION("Blink.Canvas.CreateImageBitmapSource",
                                kCreateImageBitmapSourceBlob);
      return value->GetAsBlob();
    case V8ImageBitmapSource::ContentType::kHTMLCanvasElement:
      UMA_HISTOGRAM_ENUMERATION("Blink.Canvas.CreateImageBitmapSource",
                                kCreateImageBitmapSourceHTMLCanvasElement);
      return value->GetAsHTMLCanvasElement();
    case V8ImageBitmapSource::ContentType::kHTMLImageElement:
      UMA_HISTOGRAM_ENUMERATION("Blink.Canvas.CreateImageBitmapSource",
                                kCreateImageBitmapSourceHTMLImageElement);
      return value->GetAsHTMLImageElement();
    case V8ImageBitmapSource::ContentType::kHTMLVideoElement:
      UMA_HISTOGRAM_ENUMERATION("Blink.Canvas.CreateImageBitmapSource",
                                kCreateImageBitmapSourceHTMLVideoElement);
      return value->GetAsHTMLVideoElement();
    case V8ImageBitmapSource::ContentType::kImageBitmap:
      UMA_HISTOGRAM_ENUMERATION("Blink.Canvas.CreateImageBitmapSource",
                                kCreateImageBitmapSourceImageBitmap);
      return value->GetAsImageBitmap();
    case V8ImageBitmapSource::ContentType::kImageData:
      UMA_HISTOGRAM_ENUMERATION("Blink.Canvas.CreateImageBitmapSource",
                                kCreateImageBitmapSourceImageData);
      return value->GetAsImageData();
    case V8ImageBitmapSource::ContentType::kOffscreenCanvas:
      UMA_HISTOGRAM_ENUMERATION("Blink.Canvas.CreateImageBitmapSource",
                                kCreateImageBitmapSourceOffscreenCanvas);
      return value->GetAsOffscreenCanvas();
    case V8ImageBitmapSource::ContentType::kSVGImageElement:
      UMA_HISTOGRAM_ENUMERATION("Blink.Canvas.CreateImageBitmapSource",
                                kCreateImageBitmapSourceSVGImageElement);
      return value->GetAsSVGImageElement();
    case V8ImageBitmapSource::ContentType::kVideoFrame:
      UMA_HISTOGRAM_ENUMERATION("Blink.Canvas.CreateImageBitmapSource",
                                kCreateImageBitmapSourceVideoFrame);
      return value->GetAsVideoFrame();
  }

  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

ScriptPromise<ImageBitmap> ImageBitmapFactories::CreateImageBitmapFromBlob(
    ScriptState* script_state,
    ImageBitmapSource* bitmap_source,
    std::optional<gfx::Rect> crop_rect,
    const ImageBitmapOptions* options) {
  if (!script_state->ContextIsValid()) {
    return EmptyPromise();
  }

  // imageOrientation: 'from-image' will be used to replace imageOrientation:
  // 'none'. Adding a deprecation warning when 'none' is called in
  // createImageBitmap.
  if (options->imageOrientation() == kImageBitmapOptionNone) {
    auto* execution_context =
        ExecutionContext::From(script_state->GetContext());
    Deprecation::CountDeprecation(
        execution_context,
        WebFeature::kObsoleteCreateImageBitmapImageOrientationNone);
  }

  ImageBitmapFactories& factory = From(*ExecutionContext::From(script_state));
  ImageBitmapLoader* loader = ImageBitmapFactories::ImageBitmapLoader::Create(
      factory, crop_rect, options, script_state);
  factory.AddLoader(loader);
  loader->LoadBlobAsync(static_cast<Blob*>(bitmap_source));
  return loader->Promise();
}

ScriptPromise<ImageBitmap> ImageBitmapFactories::CreateImageBitmap(
    ScriptState* script_state,
    const V8ImageBitmapSource* bitmap_source,
    const ImageBitmapOptions* options,
    ExceptionState& exception_state) {
  WebFeature feature = WebFeature::kCreateImageBitmap;
  UseCounter::Count(ExecutionContext::From(script_state), feature);
  ImageBitmapSource* bitmap_source_internal =
      ToImageBitmapSourceInternal(bitmap_source, options, false);
  if (!bitmap_source_internal)
    return EmptyPromise();
  return CreateImageBitmap(script_state, bitmap_source_internal, std::nullopt,
                           options, exception_state);
}

ScriptPromise<ImageBitmap> ImageBitmapFactories::CreateImageBitmap(
    ScriptState* script_state,
    const V8ImageBitmapSource* bitmap_source,
    int sx,
    int sy,
    int sw,
    int sh,
    const ImageBitmapOptions* options,
    ExceptionState& exception_state) {
  WebFeature feature = WebFeature::kCreateImageBitmap;
  UseCounter::Count(ExecutionContext::From(script_state), feature);
  ImageBitmapSource* bitmap_source_internal =
      ToImageBitmapSourceInternal(bitmap_source, options, true);
  if (!bitmap_source_internal)
    return EmptyPromise();
  gfx::Rect crop_rect = NormalizedCropRect(sx, sy, sw, sh);
  return CreateImageBitmap(script_state, bitmap_source_internal, crop_rect,
                           options, exception_state);
}

ScriptPromise<ImageBitmap> ImageBitmapFactories::CreateImageBitmap(
    ScriptState* script_state,
    ImageBitmapSource* bitmap_source,
    std::optional<gfx::Rect> crop_rect,
    const ImageBitmapOptions* options,
    ExceptionState& exception_state) {
  if (crop_rect && (crop_rect->width() == 0 || crop_rect->height() == 0)) {
    exception_state.ThrowRangeError(String::Format(
        "The crop rect %s is 0.", crop_rect->width() ? "height" : "width"));
    return EmptyPromise();
  }

  if (bitmap_source->IsBlob()) {
    return CreateImageBitmapFromBlob(script_state, bitmap_source, crop_rect,
                                     options);
  }

  if (bitmap_source->BitmapSourceSize().width() == 0 ||
      bitmap_source->BitmapSourceSize().height() == 0) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        String::Format(
            "The source image %s is 0.",
            bitmap_source->BitmapSourceSize().width() ? "height" : "width"));
    return EmptyPromise();
  }

  return bitmap_source->CreateImageBitmap(script_state, crop_rect, options,
                                          exception_state);
}

const char ImageBitmapFactories::kSupplementName[] = "ImageBitmapFactories";

ImageBitmapFactories& ImageBitmapFactories::From(ExecutionContext& context) {
  ImageBitmapFactories* supplement =
      Supplement<ExecutionContext>::From<ImageBitmapFactories>(context);
  if (!supplement) {
    supplement = MakeGarbageCollected<ImageBitmapFactories>(context);
    Supplement<ExecutionContext>::ProvideTo(context, supplement);
  }
  return *supplement;
}

ImageBitmapFactories::ImageBitmapFactories(ExecutionContext& context)
    : Supplement(context) {}

void ImageBitmapFactories::AddLoader(ImageBitmapLoader* loader) {
  pending_loaders_.insert(loader);
}

void ImageBitmapFactories::DidFinishLoading(ImageBitmapLoader* loader) {
  DCHECK(pending_loaders_.Contains(loader));
  pending_loaders_.erase(loader);
}

void ImageBitmapFactories::Trace(Visitor* visitor) const {
  visitor->Trace(pending_loaders_);
  Supplement<ExecutionContext>::Trace(visitor);
}

ImageBitmapFactories::ImageBitmapLoader::ImageBitmapLoader(
    ImageBitmapFactories& factory,
    std::optional<gfx::Rect> crop_rect,
    ScriptState* script_state,
    const ImageBitmapOptions* options)
    : ExecutionContextLifecycleObserver(ExecutionContext::From(script_state)),
      loader_(MakeGarbageCollected<FileReaderLoader>(
          this,
          GetExecutionContext()->GetTaskRunner(TaskType::kFileReading))),
      factory_(&factory),
      resolver_(MakeGarbageCollected<ScriptPromiseResolver<ImageBitmap>>(
          script_state)),
      crop_rect_(crop_rect),
      options_(options) {}

void ImageBitmapFactories::ImageBitmapLoader::LoadBlobAsync(Blob* blob) {
  loader_->Start(blob->GetBlobDataHandle());
}

ImageBitmapFactories::ImageBitmapLoader::~ImageBitmapLoader() {
  DCHECK(!loader_);
}

void ImageBitmapFactories::ImageBitmapLoader::RejectPromise(
    ImageBitmapRejectionReason reason) {
  CHECK(resolver_);
  ScriptState* resolver_script_state = resolver_->GetScriptState();
  if (!IsInParallelAlgorithmRunnable(resolver_->GetExecutionContext(),
                                     resolver_script_state)) {
    if (loader_) {
      loader_->Cancel();
      loader_.Clear();
    }
    factory_->DidFinishLoading(this);
    return;
  }
  ScriptState::Scope script_state_scope(resolver_script_state);
  switch (reason) {
    case kUndecodableImageBitmapRejectionReason:
      resolver_->Reject(V8ThrowDOMException::CreateOrEmpty(
          resolver_script_state->GetIsolate(),
          DOMExceptionCode::kInvalidStateError,
          "The source image could not be decoded."));
      break;
    case kAllocationFailureImageBitmapRejectionReason:
      resolver_->Reject(V8ThrowDOMException::CreateOrEmpty(
          resolver_script_state->GetIsolate(),
          DOMExceptionCode::kInvalidStateError,
          "The ImageBitmap could not be allocated."));
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  if (loader_) {
    loader_->Cancel();
    loader_.Clear();
  }
  factory_->DidFinishLoading(this);
}

void ImageBitmapFactories::ImageBitmapLoader::ContextDestroyed() {
  if (loader_) {
    factory_->DidFinishLoading(this);
    loader_->Cancel();
    loader_.Clear();
  }
}

void ImageBitmapFactories::ImageBitmapLoader::DidFinishLoading(
    FileReaderData data) {
  auto contents = std::move(data).AsArrayBufferContents();
  loader_.Clear();
  if (!contents.IsValid()) {
    RejectPromise(kAllocationFailureImageBitmapRejectionReason);
    return;
  }
  ScheduleAsyncImageBitmapDecoding(std::move(contents));
}

void ImageBitmapFactories::ImageBitmapLoader::DidFail(
    FileErrorCode error_code) {
  FileReaderAccumulator::DidFail(error_code);
  RejectPromise(kUndecodableImageBitmapRejectionReason);
}

namespace {
void DecodeImageOnDecoderThread(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    ArrayBufferContents contents,
    ImageDecoder::AlphaOption alpha_option,
    ColorBehavior color_behavior,
    WTF::CrossThreadOnceFunction<
        void(sk_sp<SkImage>, const ImageOrientationEnum)> result_callback) {
  const bool data_complete = true;
  std::unique_ptr<ImageDecoder> decoder = ImageDecoder::Create(
      SegmentReader::CreateFromSkData(
          SkData::MakeWithoutCopy(contents.Data(), contents.DataLength())),
      data_complete, alpha_option, ImageDecoder::kDefaultBitDepth,
      color_behavior, cc::AuxImage::kDefault,
      Platform::GetMaxDecodedImageBytes());
  sk_sp<SkImage> frame;
  ImageOrientationEnum orientation = ImageOrientationEnum::kDefault;
  if (decoder) {
    orientation = decoder->Orientation();
    frame = ImageBitmap::GetSkImageFromDecoder(std::move(decoder));
  }
  PostCrossThreadTask(*task_runner, FROM_HERE,
                      CrossThreadBindOnce(std::move(result_callback),
                                          std::move(frame), orientation));
}
}  // namespace

void ImageBitmapFactories::ImageBitmapLoader::ScheduleAsyncImageBitmapDecoding(
    ArrayBufferContents contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kNetworking);
  ImageDecoder::AlphaOption alpha_option =
      options_->premultiplyAlpha() != "none"
          ? ImageDecoder::AlphaOption::kAlphaPremultiplied
          : ImageDecoder::AlphaOption::kAlphaNotPremultiplied;
  ColorBehavior color_behavior = options_->colorSpaceConversion() == "none"
                                     ? ColorBehavior::kIgnore
                                     : ColorBehavior::kTag;
  worker_pool::PostTask(
      FROM_HERE,
      CrossThreadBindOnce(
          DecodeImageOnDecoderThread, std::move(task_runner),
          std::move(contents), alpha_option, color_behavior,
          CrossThreadBindOnce(&ImageBitmapFactories::ImageBitmapLoader::
                                  ResolvePromiseOnOriginalThread,
                              MakeUnwrappingCrossThreadWeakHandle(this))));
}

void ImageBitmapFactories::ImageBitmapLoader::ResolvePromiseOnOriginalThread(
    sk_sp<SkImage> frame,
    const ImageOrientationEnum orientation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!frame) {
    RejectPromise(kUndecodableImageBitmapRejectionReason);
    return;
  }
  DCHECK(frame->width());
  DCHECK(frame->height());
  scoped_refptr<StaticBitmapImage> image =
      UnacceleratedStaticBitmapImage::Create(std::move(frame), orientation);

  image->SetOriginClean(true);
  auto* image_bitmap =
      MakeGarbageCollected<ImageBitmap>(image, crop_rect_, options_);
  if (image_bitmap && image_bitmap->BitmapImage()) {
    resolver_->Resolve(image_bitmap);
  } else {
    RejectPromise(kAllocationFailureImageBitmapRejectionReason);
    return;
  }
  factory_->DidFinishLoading(this);
}

void ImageBitmapFactories::ImageBitmapLoader::Trace(Visitor* visitor) const {
  ExecutionContextLifecycleObserver::Trace(visitor);
  FileReaderAccumulator::Trace(visitor);
  visitor->Trace(factory_);
  visitor->Trace(resolver_);
  visitor->Trace(options_);
  visitor->Trace(loader_);
}

}  // namespace blink
