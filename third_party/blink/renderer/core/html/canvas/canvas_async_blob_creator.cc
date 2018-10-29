// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/canvas_async_blob_creator.h"

#include "base/location.h"
#include "build/build_config.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/graphics/image_data_buffer.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/histogram.h"
#include "third_party/blink/renderer/platform/image-encoders/image_encoder_utils.h"
#include "third_party/blink/renderer/platform/scheduler/public/background_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/web_task_runner.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/time.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

namespace {

// small slack period between deadline and current time for safety
constexpr TimeDelta kCreateBlobSlackBeforeDeadline =
    TimeDelta::FromMilliseconds(1);
constexpr TimeDelta kEncodeRowSlackBeforeDeadline =
    TimeDelta::FromMicroseconds(100);

/* The value is based on user statistics on Nov 2017. */
#if (defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_WIN))
const double kIdleTaskStartTimeoutDelayMs = 1000.0;
#else
const double kIdleTaskStartTimeoutDelayMs = 4000.0;  // For ChromeOS, Mobile
#endif

/* The value is based on user statistics on May 2018. */
// We should be more lenient on completion timeout delay to ensure that the
// switch from idle to main thread only happens to a minority of toBlob calls
#if !defined(OS_ANDROID)
// Png image encoding on 4k by 4k canvas on Mac HDD takes 5.7+ seconds
// We see that 99% users require less than 5 seconds.
const double kIdleTaskCompleteTimeoutDelayMs = 5700.0;
#else
// Png image encoding on 4k by 4k canvas on Android One takes 9.0+ seconds
// We see that 99% users require less than 9 seconds.
const double kIdleTaskCompleteTimeoutDelayMs = 9000.0;
#endif

bool IsCreateBlobDeadlineNearOrPassed(TimeTicks deadline) {
  return CurrentTimeTicks() >= deadline - kCreateBlobSlackBeforeDeadline;
}

bool IsEncodeRowDeadlineNearOrPassed(TimeTicks deadline, size_t image_width) {
  // Rough estimate of the row encoding time in micro seconds. We will consider
  // a slack time later to not pass the idle task deadline.
  int row_encode_time_us = 1000 * (kIdleTaskCompleteTimeoutDelayMs / 4000.0) *
                           (image_width / 4000.0);
  TimeDelta row_encode_time_delta =
      TimeDelta::FromMicroseconds(row_encode_time_us);
  return CurrentTimeTicks() >=
         deadline - row_encode_time_delta - kEncodeRowSlackBeforeDeadline;
}

void RecordIdleTaskStatusHistogram(
    CanvasAsyncBlobCreator::IdleTaskStatus status) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(EnumerationHistogram,
                                  to_blob_idle_task_status,
                                  ("Blink.Canvas.ToBlob.IdleTaskStatus",
                                   CanvasAsyncBlobCreator::kIdleTaskCount));
  to_blob_idle_task_status.Count(status);
}

// This enum is used in histogram and any more types should be appended at the
// end of the list.
enum ElapsedTimeHistogramType {
  kInitiateEncodingDelay,
  kCompleteEncodingDelay,
  kToBlobDuration,
  kNumberOfElapsedTimeHistogramTypes
};

void RecordElapsedTimeHistogram(ElapsedTimeHistogramType type,
                                ImageEncodingMimeType mime_type,
                                TimeDelta elapsed_time) {
  if (type == kInitiateEncodingDelay) {
    if (mime_type == kMimeTypePng) {
      DEFINE_THREAD_SAFE_STATIC_LOCAL(
          CustomCountHistogram, to_blob_png_initiate_encoding_counter,
          ("Blink.Canvas.ToBlob.InitiateEncodingDelay.PNG", 0, 10000000, 50));
      to_blob_png_initiate_encoding_counter.CountMicroseconds(elapsed_time);
    } else if (mime_type == kMimeTypeJpeg) {
      DEFINE_THREAD_SAFE_STATIC_LOCAL(
          CustomCountHistogram, to_blob_jpeg_initiate_encoding_counter,
          ("Blink.Canvas.ToBlob.InitiateEncodingDelay.JPEG", 0, 10000000, 50));
      to_blob_jpeg_initiate_encoding_counter.CountMicroseconds(elapsed_time);
    }
  } else if (type == kCompleteEncodingDelay) {
    if (mime_type == kMimeTypePng) {
      DEFINE_THREAD_SAFE_STATIC_LOCAL(
          CustomCountHistogram, to_blob_png_idle_encode_counter,
          ("Blink.Canvas.ToBlob.CompleteEncodingDelay.PNG", 0, 10000000, 50));
      to_blob_png_idle_encode_counter.CountMicroseconds(elapsed_time);
    } else if (mime_type == kMimeTypeJpeg) {
      DEFINE_THREAD_SAFE_STATIC_LOCAL(
          CustomCountHistogram, to_blob_jpeg_idle_encode_counter,
          ("Blink.Canvas.ToBlob.CompleteEncodingDelay.JPEG", 0, 10000000, 50));
      to_blob_jpeg_idle_encode_counter.CountMicroseconds(elapsed_time);
    }
  } else if (type == kToBlobDuration) {
    if (mime_type == kMimeTypePng) {
      DEFINE_THREAD_SAFE_STATIC_LOCAL(
          CustomCountHistogram, to_blob_png_counter,
          ("Blink.Canvas.ToBlobDuration.PNG", 0, 10000000, 50));
      to_blob_png_counter.CountMicroseconds(elapsed_time);
    } else if (mime_type == kMimeTypeJpeg) {
      DEFINE_THREAD_SAFE_STATIC_LOCAL(
          CustomCountHistogram, to_blob_jpeg_counter,
          ("Blink.Canvas.ToBlobDuration.JPEG", 0, 10000000, 50));
      to_blob_jpeg_counter.CountMicroseconds(elapsed_time);
    } else if (mime_type == kMimeTypeWebp) {
      DEFINE_THREAD_SAFE_STATIC_LOCAL(
          CustomCountHistogram, to_blob_webp_counter,
          ("Blink.Canvas.ToBlobDuration.WEBP", 0, 10000000, 50));
      to_blob_webp_counter.CountMicroseconds(elapsed_time);
    }
  }
}

}  // anonymous namespace

CanvasAsyncBlobCreator* CanvasAsyncBlobCreator::Create(
    scoped_refptr<StaticBitmapImage> image,
    const ImageEncodingMimeType mime_type,
    V8BlobCallback* callback,
    ToBlobFunctionType function_type,
    TimeTicks start_time,
    ExecutionContext* context) {
  ImageEncodeOptions options;
  options.setType(ImageEncodingMimeTypeName(mime_type));
  return new CanvasAsyncBlobCreator(image, options, function_type, callback,
                                    start_time, context, nullptr);
}

CanvasAsyncBlobCreator* CanvasAsyncBlobCreator::Create(
    scoped_refptr<StaticBitmapImage> image,
    const ImageEncodeOptions& options,
    ToBlobFunctionType function_type,
    TimeTicks start_time,
    ExecutionContext* context,
    ScriptPromiseResolver* resolver) {
  return new CanvasAsyncBlobCreator(image, options, function_type, nullptr,
                                    start_time, context, resolver);
}

CanvasAsyncBlobCreator::CanvasAsyncBlobCreator(
    scoped_refptr<StaticBitmapImage> image,
    const ImageEncodeOptions& options,
    ToBlobFunctionType function_type,
    V8BlobCallback* callback,
    TimeTicks start_time,
    ExecutionContext* context,
    ScriptPromiseResolver* resolver)
    : fail_encoder_initialization_for_test_(false),
      enforce_idle_encoding_for_test_(false),
      image_(image),
      context_(context),
      encode_options_(options),
      function_type_(function_type),
      start_time_(start_time),
      static_bitmap_image_loaded_(false),
      callback_(ToV8PersistentCallbackFunction(callback)),
      script_promise_resolver_(resolver) {
  DCHECK(image);

  mime_type_ = ImageEncoderUtils::ToEncodingMimeType(
      encode_options_.type(),
      ImageEncoderUtils::kEncodeReasonConvertToBlobPromise);

  // We use pixmap to access the image pixels. Make the image unaccelerated if
  // necessary.
  image_ = image_->MakeUnaccelerated();

  sk_sp<SkImage> skia_image = image_->PaintImageForCurrentFrame().GetSkImage();
  DCHECK(skia_image);

  // If image is lazy decoded, call readPixels() to trigger decoding.
  if (skia_image->isLazyGenerated()) {
    SkImageInfo info = SkImageInfo::MakeN32Premul(1, 1);
    uint8_t pixel[info.bytesPerPixel()];
    skia_image->readPixels(info, pixel, info.minRowBytes(), 0, 0);
  }

  // For kHTMLCanvasToBlobCallback and kOffscreenCanvasConvertToBlobPromise
  // to-blob function types, we color convert to sRGB and do not tag the image
  // with any color space info.
  // For kHTMLCanvasConvertToBlobPromise to-blob function type, we color
  // covnert to the requested color space and pixel format.
  if (function_type_ != kHTMLCanvasConvertToBlobPromise) {
    if (skia_image->colorSpace()) {
      image_ = image_->ConvertToColorSpace(SkColorSpace::MakeSRGB());
      skia_image = image_->PaintImageForCurrentFrame().GetSkImage();
    }

    if (skia_image->peekPixels(&src_data_)) {
      src_data_.setColorSpace(nullptr);
      static_bitmap_image_loaded_ = true;
    }
    DCHECK(!src_data_.colorSpace());
  } else {
    sk_sp<SkColorSpace> blob_color_space =
        BlobColorSpaceToSkColorSpace(encode_options_.colorSpace());
    bool needs_color_space_conversion = !ApproximatelyEqualSkColorSpaces(
        skia_image->refColorSpace(), blob_color_space);
    if (needs_color_space_conversion && !skia_image->colorSpace()) {
      skia_image->peekPixels(&src_data_);
      src_data_.setColorSpace(SkColorSpace::MakeSRGB());
      skia_image = SkImage::MakeFromRaster(src_data_, nullptr, nullptr);
      DCHECK(skia_image->colorSpace());
    }

    SkColorType target_color_type = kN32_SkColorType;
    if (encode_options_.pixelFormat() == kRGBA16ImagePixelFormatName)
      target_color_type = kRGBA_F16_SkColorType;
    // We can do color space and color type conversion together.
    if (needs_color_space_conversion) {
      image_ = StaticBitmapImage::Create(skia_image);
      image_ = image_->ConvertToColorSpace(blob_color_space, target_color_type);
      skia_image = image_->PaintImageForCurrentFrame().GetSkImage();
    } else if (skia_image->colorType() != target_color_type) {
      size_t data_length = skia_image->width() * skia_image->height() *
                           SkColorTypeBytesPerPixel(target_color_type);
      png_data_helper_ = SkData::MakeUninitialized(data_length);
      SkImageInfo info = SkImageInfo::Make(
          skia_image->width(), skia_image->height(), target_color_type,
          skia_image->alphaType(), skia_image->refColorSpace());
      SkPixmap src_data_f16(info, png_data_helper_->writable_data(),
                            info.minRowBytes());
      skia_image->readPixels(src_data_f16, 0, 0);
      skia_image = SkImage::MakeFromRaster(src_data_f16, nullptr, nullptr);
      image_ = StaticBitmapImage::Create(skia_image);
    }

    if (skia_image->peekPixels(&src_data_))
      static_bitmap_image_loaded_ = true;
  }

  if (static_bitmap_image_loaded_) {
    // Ensure that the size of the to-be-encoded-image does not pass the maximum
    // size supported by the encoders.
    int max_dimension = ImageEncoder::MaxDimension(mime_type_);
    if (std::max(src_data_.width(), src_data_.height()) > max_dimension) {
      SkImageInfo info = src_data_.info();
      info = info.makeWH(std::min(info.width(), max_dimension),
                         std::min(info.height(), max_dimension));
      src_data_.reset(info, src_data_.addr(), src_data_.rowBytes());
    }
  }

  idle_task_status_ = kIdleTaskNotSupported;
  num_rows_completed_ = 0;
  if (context->IsDocument()) {
    parent_frame_task_runner_ =
        context->GetTaskRunner(TaskType::kCanvasBlobSerialization);
  }
}

CanvasAsyncBlobCreator::~CanvasAsyncBlobCreator() = default;

void CanvasAsyncBlobCreator::Dispose() {
  // Eagerly let go of references to prevent retention of these
  // resources while any remaining posted tasks are queued.
  context_.Clear();
  callback_.Clear();
  script_promise_resolver_.Clear();
  image_ = nullptr;
}

ImageEncodeOptions CanvasAsyncBlobCreator::GetImageEncodeOptionsForMimeType(
    ImageEncodingMimeType mime_type) {
  ImageEncodeOptions encode_options;
  encode_options.setType(ImageEncodingMimeTypeName(mime_type));
  return encode_options;
}

bool CanvasAsyncBlobCreator::EncodeImage(const double& quality) {
  std::unique_ptr<ImageDataBuffer> buffer = ImageDataBuffer::Create(src_data_);
  if (!buffer)
    return false;
  return buffer->EncodeImage(mime_type_, quality, &encoded_image_);
}

void CanvasAsyncBlobCreator::ScheduleAsyncBlobCreation(const double& quality) {
  if (!static_bitmap_image_loaded_) {
    context_->GetTaskRunner(TaskType::kCanvasBlobSerialization)
        ->PostTask(FROM_HERE,
                   WTF::Bind(&CanvasAsyncBlobCreator::CreateNullAndReturnResult,
                             WrapPersistent(this)));
    return;
  }
  // Webp encoder does not support progressive encoding. We also don't use idle
  // encoding for layout tests, since the idle task start and completition
  // deadlines (6.7s or 13s) bypass the layout test running deadline (6s)
  // and result in timeouts on different tests. We use
  // enforce_idle_encoding_for_test_ to test idle encoding in unit tests.
  bool use_idle_encoding =
      (mime_type_ != kMimeTypeWebp) &&
      (enforce_idle_encoding_for_test_ ||
       !RuntimeEnabledFeatures::NoIdleEncodingForLayoutTestsEnabled());

  if (!use_idle_encoding) {
    if (!IsMainThread()) {
      DCHECK(function_type_ == kHTMLCanvasConvertToBlobPromise ||
             function_type_ == kOffscreenCanvasConvertToBlobPromise);
      // When OffscreenCanvas.convertToBlob() occurs on worker thread,
      // we do not need to use background task runner to reduce load on main.
      // So we just directly encode images on the worker thread.
      if (!EncodeImage(quality)) {
        context_->GetTaskRunner(TaskType::kCanvasBlobSerialization)
            ->PostTask(
                FROM_HERE,
                WTF::Bind(&CanvasAsyncBlobCreator::CreateNullAndReturnResult,
                          WrapPersistent(this)));

        return;
      }
      context_->GetTaskRunner(TaskType::kCanvasBlobSerialization)
          ->PostTask(
              FROM_HERE,
              WTF::Bind(&CanvasAsyncBlobCreator::CreateBlobAndReturnResult,
                        WrapPersistent(this)));

    } else {
      background_scheduler::PostOnBackgroundThread(
          FROM_HERE,
          CrossThreadBind(&CanvasAsyncBlobCreator::EncodeImageOnEncoderThread,
                          WrapCrossThreadPersistent(this), quality));
    }
  } else {
    idle_task_status_ = kIdleTaskNotStarted;
    ScheduleInitiateEncoding(quality);

    // We post the below task to check if the above idle task isn't late.
    // There's no risk of concurrency as both tasks are on the same thread.
    PostDelayedTaskToCurrentThread(
        FROM_HERE,
        WTF::Bind(&CanvasAsyncBlobCreator::IdleTaskStartTimeoutEvent,
                  WrapPersistent(this), quality),
        kIdleTaskStartTimeoutDelayMs);
  }
}

void CanvasAsyncBlobCreator::ScheduleInitiateEncoding(double quality) {
  schedule_idle_task_start_time_ = WTF::CurrentTimeTicks();
  Platform::Current()->CurrentThread()->Scheduler()->PostIdleTask(
      FROM_HERE, WTF::Bind(&CanvasAsyncBlobCreator::InitiateEncoding,
                           WrapPersistent(this), quality));
}

void CanvasAsyncBlobCreator::InitiateEncoding(double quality,
                                              TimeTicks deadline) {
  if (idle_task_status_ == kIdleTaskSwitchedToImmediateTask) {
    return;
  }
  RecordElapsedTimeHistogram(
      kInitiateEncodingDelay, mime_type_,
      WTF::CurrentTimeTicks() - schedule_idle_task_start_time_);

  DCHECK(idle_task_status_ == kIdleTaskNotStarted);
  idle_task_status_ = kIdleTaskStarted;

  if (!InitializeEncoder(quality)) {
    idle_task_status_ = kIdleTaskFailed;
    return;
  }

  // Re-use this time variable to collect data on complete encoding delay
  schedule_idle_task_start_time_ = WTF::CurrentTimeTicks();
  IdleEncodeRows(deadline);
}

void CanvasAsyncBlobCreator::IdleEncodeRows(TimeTicks deadline) {
  if (idle_task_status_ == kIdleTaskSwitchedToImmediateTask) {
    return;
  }

  for (int y = num_rows_completed_; y < src_data_.height(); ++y) {
    if (IsEncodeRowDeadlineNearOrPassed(deadline, src_data_.width())) {
      num_rows_completed_ = y;
      Platform::Current()->CurrentThread()->Scheduler()->PostIdleTask(
          FROM_HERE, WTF::Bind(&CanvasAsyncBlobCreator::IdleEncodeRows,
                               WrapPersistent(this)));
      return;
    }

    if (!encoder_->encodeRows(1)) {
      idle_task_status_ = kIdleTaskFailed;
      CreateNullAndReturnResult();
      return;
    }
  }
  num_rows_completed_ = src_data_.height();

  idle_task_status_ = kIdleTaskCompleted;
  TimeDelta elapsed_time =
      WTF::CurrentTimeTicks() - schedule_idle_task_start_time_;
  RecordElapsedTimeHistogram(kCompleteEncodingDelay, mime_type_, elapsed_time);
  if (IsCreateBlobDeadlineNearOrPassed(deadline)) {
    context_->GetTaskRunner(TaskType::kCanvasBlobSerialization)
        ->PostTask(FROM_HERE,
                   WTF::Bind(&CanvasAsyncBlobCreator::CreateBlobAndReturnResult,
                             WrapPersistent(this)));
  } else {
    CreateBlobAndReturnResult();
  }
}

void CanvasAsyncBlobCreator::ForceEncodeRowsOnCurrentThread() {
  DCHECK(idle_task_status_ == kIdleTaskSwitchedToImmediateTask);

  // Continue encoding from the last completed row
  for (int y = num_rows_completed_; y < src_data_.height(); ++y) {
    if (!encoder_->encodeRows(1)) {
      idle_task_status_ = kIdleTaskFailed;
      CreateNullAndReturnResult();
      return;
    }
  }
  num_rows_completed_ = src_data_.height();

  if (IsMainThread()) {
    CreateBlobAndReturnResult();
  } else {
    PostCrossThreadTask(
        *context_->GetTaskRunner(TaskType::kCanvasBlobSerialization), FROM_HERE,
        CrossThreadBind(&CanvasAsyncBlobCreator::CreateBlobAndReturnResult,
                        WrapCrossThreadPersistent(this)));
  }

  SignalAlternativeCodePathFinishedForTesting();
}

void CanvasAsyncBlobCreator::CreateBlobAndReturnResult() {
  RecordIdleTaskStatusHistogram(idle_task_status_);
  RecordElapsedTimeHistogram(kToBlobDuration, mime_type_,
                             WTF::CurrentTimeTicks() - start_time_);

  Blob* result_blob = Blob::Create(encoded_image_.data(), encoded_image_.size(),
                                   ImageEncodingMimeTypeName(mime_type_));
  if (function_type_ == kHTMLCanvasToBlobCallback) {
    context_->GetTaskRunner(TaskType::kCanvasBlobSerialization)
        ->PostTask(FROM_HERE,
                   WTF::Bind(&V8PersistentCallbackFunction<
                                 V8BlobCallback>::InvokeAndReportException,
                             WrapPersistent(callback_.Get()), nullptr,
                             WrapPersistent(result_blob)));
  } else {
    script_promise_resolver_->Resolve(result_blob);
  }
  // Avoid unwanted retention, see dispose().
  Dispose();
}

void CanvasAsyncBlobCreator::CreateNullAndReturnResult() {
  RecordIdleTaskStatusHistogram(idle_task_status_);
  if (function_type_ == kHTMLCanvasToBlobCallback) {
    DCHECK(IsMainThread());
    RecordIdleTaskStatusHistogram(idle_task_status_);
    context_->GetTaskRunner(TaskType::kCanvasBlobSerialization)
        ->PostTask(
            FROM_HERE,
            WTF::Bind(&V8PersistentCallbackFunction<
                          V8BlobCallback>::InvokeAndReportException,
                      WrapPersistent(callback_.Get()), nullptr, nullptr));
  } else {
    script_promise_resolver_->Reject(
        DOMException::Create(DOMExceptionCode::kEncodingError,
                             "Encoding of the source image has failed."));
  }
  // Avoid unwanted retention, see dispose().
  Dispose();
}

void CanvasAsyncBlobCreator::EncodeImageOnEncoderThread(double quality) {
  DCHECK(!IsMainThread());
  if (!EncodeImage(quality)) {
    PostCrossThreadTask(
        *parent_frame_task_runner_, FROM_HERE,
        CrossThreadBind(&CanvasAsyncBlobCreator::CreateNullAndReturnResult,
                        WrapCrossThreadPersistent(this)));
    return;
  }

  PostCrossThreadTask(
      *parent_frame_task_runner_, FROM_HERE,
      CrossThreadBind(&CanvasAsyncBlobCreator::CreateBlobAndReturnResult,
                      WrapCrossThreadPersistent(this)));
}

bool CanvasAsyncBlobCreator::InitializeEncoder(double quality) {
  // This is solely used for unit tests.
  if (fail_encoder_initialization_for_test_)
    return false;
  if (mime_type_ == kMimeTypeJpeg) {
    SkJpegEncoder::Options options;
    options.fQuality = ImageEncoder::ComputeJpegQuality(quality);
    options.fAlphaOption = SkJpegEncoder::AlphaOption::kBlendOnBlack;
    if (options.fQuality == 100) {
      options.fDownsample = SkJpegEncoder::Downsample::k444;
    }
    encoder_ = ImageEncoder::Create(&encoded_image_, src_data_, options);
  } else {
    // Progressive encoding is only applicable to png and jpeg image format,
    // and thus idle tasks scheduling can only be applied to these image
    // formats.
    // TODO(zakerinasab): Progressive encoding on webp image formats
    // (crbug.com/571399)
    DCHECK_EQ(kMimeTypePng, mime_type_);
    SkPngEncoder::Options options;
    options.fFilterFlags = SkPngEncoder::FilterFlag::kSub;
    options.fZLibLevel = 3;
    encoder_ = ImageEncoder::Create(&encoded_image_, src_data_, options);
  }

  return encoder_.get();
}

void CanvasAsyncBlobCreator::IdleTaskStartTimeoutEvent(double quality) {
  if (idle_task_status_ == kIdleTaskStarted) {
    // Even if the task started quickly, we still want to ensure completion
    PostDelayedTaskToCurrentThread(
        FROM_HERE,
        WTF::Bind(&CanvasAsyncBlobCreator::IdleTaskCompleteTimeoutEvent,
                  WrapPersistent(this)),
        kIdleTaskCompleteTimeoutDelayMs);
  } else if (idle_task_status_ == kIdleTaskNotStarted) {
    // If the idle task does not start after a delay threshold, we will
    // force it to happen on main thread (even though it may cause more
    // janks) to prevent toBlob being postponed forever in extreme cases.
    idle_task_status_ = kIdleTaskSwitchedToImmediateTask;
    SignalTaskSwitchInStartTimeoutEventForTesting();

    DCHECK(mime_type_ == kMimeTypePng || mime_type_ == kMimeTypeJpeg);
    if (InitializeEncoder(quality)) {
      context_->GetTaskRunner(TaskType::kCanvasBlobSerialization)
          ->PostTask(
              FROM_HERE,
              WTF::Bind(&CanvasAsyncBlobCreator::ForceEncodeRowsOnCurrentThread,
                        WrapPersistent(this)));
    } else {
      // Failing in initialization of encoder
      SignalAlternativeCodePathFinishedForTesting();
    }
  } else {
    DCHECK(idle_task_status_ == kIdleTaskFailed ||
           idle_task_status_ == kIdleTaskCompleted);
    SignalAlternativeCodePathFinishedForTesting();
  }
}

void CanvasAsyncBlobCreator::IdleTaskCompleteTimeoutEvent() {
  DCHECK(idle_task_status_ != kIdleTaskNotStarted);

  if (idle_task_status_ == kIdleTaskStarted) {
    // It has taken too long to complete for the idle task.
    idle_task_status_ = kIdleTaskSwitchedToImmediateTask;
    SignalTaskSwitchInCompleteTimeoutEventForTesting();

    DCHECK(mime_type_ == kMimeTypePng || mime_type_ == kMimeTypeJpeg);
    context_->GetTaskRunner(TaskType::kCanvasBlobSerialization)
        ->PostTask(
            FROM_HERE,
            WTF::Bind(&CanvasAsyncBlobCreator::ForceEncodeRowsOnCurrentThread,
                      WrapPersistent(this)));
  } else {
    DCHECK(idle_task_status_ == kIdleTaskFailed ||
           idle_task_status_ == kIdleTaskCompleted);
    SignalAlternativeCodePathFinishedForTesting();
  }
}

void CanvasAsyncBlobCreator::PostDelayedTaskToCurrentThread(
    const base::Location& location,
    base::OnceClosure task,
    double delay_ms) {
  context_->GetTaskRunner(TaskType::kCanvasBlobSerialization)
      ->PostDelayedTask(location, std::move(task),
                        TimeDelta::FromMillisecondsD(delay_ms));
}

void CanvasAsyncBlobCreator::Trace(blink::Visitor* visitor) {
  visitor->Trace(context_);
  visitor->Trace(callback_);
  visitor->Trace(script_promise_resolver_);
}

sk_sp<SkColorSpace> CanvasAsyncBlobCreator::BlobColorSpaceToSkColorSpace(
    String blob_color_space) {
  SkColorSpace::Gamut gamut = SkColorSpace::kSRGB_Gamut;
  if (blob_color_space == kDisplayP3ImageColorSpaceName)
    gamut = SkColorSpace::kDCIP3_D65_Gamut;
  else if (blob_color_space == kRec2020ImageColorSpaceName)
    gamut = SkColorSpace::kRec2020_Gamut;
  return SkColorSpace::MakeRGB(SkColorSpace::kSRGB_RenderTargetGamma, gamut);
}

bool CanvasAsyncBlobCreator::EncodeImageForConvertToBlobTest() {
  if (!static_bitmap_image_loaded_)
    return false;
  std::unique_ptr<ImageDataBuffer> buffer = ImageDataBuffer::Create(src_data_);
  if (!buffer)
    return false;
  return buffer->EncodeImage(mime_type_, encode_options_.quality(),
                             &encoded_image_);
}

}  // namespace blink
