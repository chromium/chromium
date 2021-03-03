// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"

#include <utility>

#include "base/memory/scoped_refptr.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_metadata.h"
#include "media/base/video_frame_pool.h"
#include "media/base/video_util.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_plane_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_frame_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_frame_plane_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_pixel_format.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_image_source.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_piece.h"
#include "third_party/blink/renderer/modules/canvas/imagebitmap/image_bitmap_factories.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/video_frame_image_util.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"

namespace blink {

namespace {

struct YUVReadbackContext {
  gfx::Size coded_size;
  gfx::Rect visible_rect;
  gfx::Size natural_size;
  base::TimeDelta timestamp;
  scoped_refptr<media::VideoFrame> frame;
};

void OnYUVReadbackDone(
    void* raw_ctx,
    std::unique_ptr<const SkImage::AsyncReadResult> async_result) {
  if (!async_result)
    return;
  auto* context = reinterpret_cast<YUVReadbackContext*>(raw_ctx);
  context->frame = media::VideoFrame::WrapExternalYuvData(
      media::PIXEL_FORMAT_I420, context->coded_size, context->visible_rect,
      context->natural_size, static_cast<int>(async_result->rowBytes(0)),
      static_cast<int>(async_result->rowBytes(1)),
      static_cast<int>(async_result->rowBytes(2)),
      // TODO(crbug.com/1161304): We should be able to wrap readonly memory in
      // a VideoFrame without resorting to a const_cast.
      reinterpret_cast<uint8_t*>(const_cast<void*>(async_result->data(0))),
      reinterpret_cast<uint8_t*>(const_cast<void*>(async_result->data(1))),
      reinterpret_cast<uint8_t*>(const_cast<void*>(async_result->data(2))),
      context->timestamp);
  if (!context->frame)
    return;
  context->frame->AddDestructionObserver(
      ConvertToBaseOnceCallback(WTF::CrossThreadBindOnce(
          base::DoNothing::Once<
              std::unique_ptr<const SkImage::AsyncReadResult>>(),
          std::move(async_result))));
}

media::VideoPixelFormat ToMediaPixelFormat(V8VideoPixelFormat::Enum fmt) {
  switch (fmt) {
    case V8VideoPixelFormat::Enum::kI420:
      return media::PIXEL_FORMAT_I420;
    case V8VideoPixelFormat::Enum::kNV12:
      return media::PIXEL_FORMAT_NV12;
    case V8VideoPixelFormat::Enum::kABGR:
      return media::PIXEL_FORMAT_ABGR;
    case V8VideoPixelFormat::Enum::kXBGR:
      return media::PIXEL_FORMAT_XBGR;
    case V8VideoPixelFormat::Enum::kARGB:
      return media::PIXEL_FORMAT_ARGB;
    case V8VideoPixelFormat::Enum::kXRGB:
      return media::PIXEL_FORMAT_XRGB;
  }
}

class CachedVideoFramePool : public GarbageCollected<CachedVideoFramePool>,
                             public Supplement<ExecutionContext> {
 public:
  static const char kSupplementName[];

  static CachedVideoFramePool& From(ExecutionContext& context) {
    CachedVideoFramePool* supplement =
        Supplement<ExecutionContext>::From<CachedVideoFramePool>(context);
    if (!supplement) {
      supplement = MakeGarbageCollected<CachedVideoFramePool>(context);
      Supplement<ExecutionContext>::ProvideTo(context, supplement);
    }
    return *supplement;
  }

  explicit CachedVideoFramePool(ExecutionContext& context)
      : Supplement<ExecutionContext>(context),
        task_runner_(Thread::Current()->GetTaskRunner()) {}
  virtual ~CachedVideoFramePool() = default;

  // Disallow copy and assign.
  CachedVideoFramePool& operator=(const CachedVideoFramePool&) = delete;
  CachedVideoFramePool(const CachedVideoFramePool&) = delete;

  scoped_refptr<media::VideoFrame> CreateFrame(media::VideoPixelFormat format,
                                               const gfx::Size& coded_size,
                                               const gfx::Rect& visible_rect,
                                               const gfx::Size& natural_size,
                                               base::TimeDelta timestamp) {
    if (!frame_pool_)
      CreatePoolAndStartIdleObsever();

    last_frame_creation_ = base::TimeTicks::Now();
    return frame_pool_->CreateFrame(format, coded_size, visible_rect,
                                    natural_size, timestamp);
  }

  void Trace(Visitor* visitor) const override {
    Supplement<ExecutionContext>::Trace(visitor);
  }

 private:
  static const base::TimeDelta kIdleTimeout;

  void PostMonitoringTask() {
    DCHECK(!task_handle_.IsActive());
    task_handle_ = PostDelayedCancellableTask(
        *task_runner_, FROM_HERE,
        WTF::Bind(&CachedVideoFramePool::PurgeIdleFramePool,
                  WrapWeakPersistent(this)),
        kIdleTimeout);
  }

  void CreatePoolAndStartIdleObsever() {
    DCHECK(!frame_pool_);
    frame_pool_ = std::make_unique<media::VideoFramePool>();
    PostMonitoringTask();
  }

  // We don't want a VideoFramePool to stick around forever wasting memory, so
  // once we haven't issued any VideoFrames for a while, turn down the pool.
  void PurgeIdleFramePool() {
    if (base::TimeTicks::Now() - last_frame_creation_ > kIdleTimeout) {
      frame_pool_.reset();
      return;
    }
    PostMonitoringTask();
  }

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::unique_ptr<media::VideoFramePool> frame_pool_;
  base::TimeTicks last_frame_creation_;
  TaskHandle task_handle_;
};

// static -- defined out of line to satisfy link time requirements.
const char CachedVideoFramePool::kSupplementName[] = "CachedVideoFramePool";
const base::TimeDelta CachedVideoFramePool::kIdleTimeout =
    base::TimeDelta::FromSeconds(10);

bool IsSupportedPlanarFormat(const media::VideoFrame& frame) {
  if (!frame.IsMappable() && !frame.HasGpuMemoryBuffer())
    return false;

  const size_t num_planes = frame.layout().num_planes();
  switch (frame.format()) {
    case media::PIXEL_FORMAT_I420:
      return num_planes == 3;
    case media::PIXEL_FORMAT_I420A:
      return num_planes == 4;
    case media::PIXEL_FORMAT_NV12:
      return num_planes == 2;
    case media::PIXEL_FORMAT_XBGR:
    case media::PIXEL_FORMAT_XRGB:
    case media::PIXEL_FORMAT_ABGR:
    case media::PIXEL_FORMAT_ARGB:
      return num_planes == 1;
    default:
      return false;
  }
}

}  // namespace

VideoFrame::VideoFrame(scoped_refptr<media::VideoFrame> frame,
                       ExecutionContext* context) {
  DCHECK(frame);
  handle_ = base::MakeRefCounted<VideoFrameHandle>(std::move(frame), context);
}

VideoFrame::VideoFrame(scoped_refptr<VideoFrameHandle> handle)
    : handle_(std::move(handle)) {
  DCHECK(handle_);

  // Note: The provided |handle| may be invalid if close() has been called while
  // a frame is in transit to another thread.
}

// static
VideoFrame* VideoFrame::Create(ScriptState* script_state,
                               const CanvasImageSourceUnion& source,
                               const VideoFrameInit* init,
                               ExceptionState& exception_state) {
  auto* image_source = ToCanvasImageSource(source, exception_state);
  if (!image_source) {
    // ToCanvasImageSource() will throw a source appropriate exception.
    return nullptr;
  }

  if (image_source->WouldTaintOrigin()) {
    exception_state.ThrowSecurityError(
        "VideoFrames can't be created from tainted sources.");
    return nullptr;
  }

  // Special case <video> and VideoFrame to directly use the underlying frame.
  if (source.IsVideoFrame() || source.IsHTMLVideoElement()) {
    scoped_refptr<media::VideoFrame> source_frame;
    if (source.IsVideoFrame()) {
      if (!init || (!init->hasTimestamp() && !init->hasDuration()))
        return source.GetAsVideoFrame()->clone(script_state, exception_state);
      source_frame = source.GetAsVideoFrame()->frame();
    } else if (source.IsHTMLVideoElement()) {
      if (auto* wmp = source.GetAsHTMLVideoElement()->GetWebMediaPlayer())
        source_frame = wmp->GetCurrentFrame();
    }

    if (!source_frame) {
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        "Invalid source state");
      return nullptr;
    }

    // We can't modify the timestamp or duration directly since there may be
    // other owners accessing these fields concurrently.
    if (init && (init->hasTimestamp() || init->hasDuration())) {
      source_frame = media::VideoFrame::WrapVideoFrame(
          source_frame, source_frame->format(), source_frame->visible_rect(),
          source_frame->natural_size());
      if (init->hasTimestamp()) {
        source_frame->set_timestamp(
            base::TimeDelta::FromMicroseconds(init->timestamp()));
      }
      if (init->hasDuration()) {
        source_frame->metadata().frame_duration =
            base::TimeDelta::FromMicroseconds(init->duration());
      }
    }

    return MakeGarbageCollected<VideoFrame>(
        std::move(source_frame), ExecutionContext::From(script_state));
  }

  SourceImageStatus status = kInvalidSourceImageStatus;
  auto image = image_source->GetSourceImageForCanvas(&status, FloatSize());
  if (!image || status != kNormalSourceImageStatus) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid source state");
    return nullptr;
  }

  const auto timestamp = base::TimeDelta::FromMicroseconds(
      (init && init->hasTimestamp()) ? init->timestamp() : 0);

  const auto sk_image = image->PaintImageForCurrentFrame().GetSkImage();
  const auto sk_image_info = sk_image->imageInfo();

  auto sk_color_space = sk_image_info.refColorSpace();
  if (!sk_color_space)
    sk_color_space = SkColorSpace::MakeSRGB();

  const auto gfx_color_space = gfx::ColorSpace(*sk_color_space);
  if (!gfx_color_space.IsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid color space");
    return nullptr;
  }

  const gfx::Size coded_size(sk_image_info.width(), sk_image_info.height());
  const gfx::Rect visible_rect(coded_size);
  const gfx::Size natural_size = coded_size;

  scoped_refptr<media::VideoFrame> frame;
  if (sk_image->isTextureBacked()) {
    YUVReadbackContext result;
    result.coded_size = coded_size;
    result.visible_rect = visible_rect;
    result.natural_size = natural_size;
    result.timestamp = timestamp;

    // While this function indicates it's asynchronous, the flushAndSubmit()
    // call below ensures it completes synchronously.
    sk_image->asyncRescaleAndReadPixelsYUV420(
        kRec709_SkYUVColorSpace, sk_color_space, sk_image_info.bounds(),
        sk_image_info.dimensions(), SkImage::RescaleGamma::kSrc,
        SkImage::RescaleMode::kRepeatedCubic, &OnYUVReadbackDone, &result);
    GrDirectContext* gr_context = image->ContextProvider()->GetGrContext();
    DCHECK(gr_context);
    gr_context->flushAndSubmit(/*syncCpu=*/true);

    if (!result.frame) {
      exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                        "YUV conversion error during readback");
      return nullptr;
    }

    frame = std::move(result.frame);
    frame->set_color_space(gfx::ColorSpace::CreateREC709());
    if (init && init->hasDuration()) {
      frame->metadata().frame_duration =
          base::TimeDelta::FromMicroseconds(init->duration());
    }
    return MakeGarbageCollected<VideoFrame>(
        std::move(frame), ExecutionContext::From(script_state));
  }

  frame =
      media::CreateFromSkImage(sk_image, visible_rect, natural_size, timestamp);
  if (!frame) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Failed to create video frame");
    return nullptr;
  }
  frame->set_color_space(gfx_color_space);
  if (init && init->hasDuration()) {
    frame->metadata().frame_duration =
        base::TimeDelta::FromMicroseconds(init->duration());
  }
  return MakeGarbageCollected<VideoFrame>(
      base::MakeRefCounted<VideoFrameHandle>(
          std::move(frame), std::move(sk_image),
          ExecutionContext::From(script_state)));
}

// static
VideoFrame* VideoFrame::Create(ScriptState* script_state,
                               const String& format,
                               const HeapVector<Member<PlaneInit>>& planes,
                               const VideoFramePlaneInit* init,
                               ExceptionState& exception_state) {
  if (!init->hasCodedWidth() || !init->hasCodedHeight()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kConstraintError,
        "Coded size is required for planar construction");
    return nullptr;
  }

  // Type formats are enforced by V8.
  auto typed_fmt = V8VideoPixelFormat::Create(format);
  DCHECK(typed_fmt);

  auto media_fmt = ToMediaPixelFormat(typed_fmt->AsEnum());

  // There's no I420A pixel format, so treat I420 + 4 planes as I420A.
  if (media_fmt == media::PIXEL_FORMAT_I420 && planes.size() == 4u)
    media_fmt = media::PIXEL_FORMAT_I420A;

  if (media::VideoFrame::NumPlanes(media_fmt) != planes.size()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kConstraintError,
        String::Format("Invalid number of planes for format %s; expected %zu, "
                       "received %u",
                       format.Ascii().c_str(),
                       media::VideoFrame::NumPlanes(media_fmt), planes.size()));
    return nullptr;
  }

  const gfx::Size coded_size(init->codedWidth(), init->codedHeight());
  if (coded_size.IsEmpty()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kConstraintError,
        String::Format("Invalid coded size (%d, %d) provided",
                       init->codedWidth(), init->codedHeight()));
    return nullptr;
  }

  for (wtf_size_t i = 0; i < planes.size(); ++i) {
    const auto minimum_size =
        media::VideoFrame::PlaneSize(media_fmt, i, coded_size);
    if (planes[i]->stride() < uint32_t{minimum_size.width()}) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kConstraintError,
          String::Format(
              "The stride of plane %u is too small for the given coded size "
              "(%s); expected at least %d, received %u",
              i, coded_size.ToString().c_str(), minimum_size.width(),
              planes[i]->stride()));
      return nullptr;
    }
    if (planes[i]->rows() != uint32_t{minimum_size.height()}) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kConstraintError,
          String::Format(
              "The row count for plane %u is incorrect for the given coded "
              "size (%s); expected %d, received %u",
              i, coded_size.ToString().c_str(), minimum_size.height(),
              planes[i]->rows()));
      return nullptr;
    }

    // This requires the full stride to be provided for every row.
    gfx::Size provided_size(planes[i]->stride(), planes[i]->rows());
    const auto required_byte_size = provided_size.GetCheckedArea();
    if (!required_byte_size.IsValid()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kConstraintError,
          String::Format("The size of plane %u is too large", i));
      return nullptr;
    }

    DOMArrayPiece buffer(planes[i]->src());
    if (buffer.ByteLength() < required_byte_size.ValueOrDie()) {
      // Note: We use GetArea() below instead of area.ValueOrDie() since the
      // base::StrictNumeric seems to confuse the printf() format checks.
      exception_state.ThrowDOMException(
          DOMExceptionCode::kConstraintError,
          String::Format(
              "The size of plane %u is too small for the given coded "
              "size (%s); expected at least %d, received %zu",
              i, coded_size.ToString().c_str(), provided_size.GetArea(),
              buffer.ByteLength()));
      return nullptr;
    }
  }

  auto visible_rect = gfx::Rect(coded_size);
  if (init->hasCropLeft() || init->hasCropTop() || init->hasCropWidth() ||
      init->hasCropHeight()) {
    const auto crop_left = init->hasCropLeft() ? init->cropLeft() : 0;
    const auto crop_top = init->hasCropTop() ? init->cropTop() : 0;
    const auto crop_w =
        init->hasCropWidth() ? visible_rect.width() - init->cropWidth() : 0;
    const auto crop_h =
        init->hasCropHeight() ? visible_rect.height() - init->cropHeight() : 0;
    if (crop_w < 0 || crop_h < 0 || crop_w > unsigned{visible_rect.width()} ||
        crop_h > unsigned{visible_rect.height()}) {
      visible_rect = gfx::Rect();
    } else {
      visible_rect.Inset(crop_left, crop_top, crop_w, crop_h);
    }

    if (visible_rect.IsEmpty()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kConstraintError,
          String::Format(
              "Invalid visble rect (%s) after crop (%d, %d, %d, %d) applied",
              visible_rect.ToString().c_str(), crop_left, crop_top, crop_w,
              crop_h));
      return nullptr;
    }
  }

  auto natural_size = visible_rect.size();
  if (init->hasDisplayWidth())
    natural_size.set_width(init->displayWidth());
  if (init->hasDisplayHeight())
    natural_size.set_height(init->displayHeight());
  if (coded_size.IsEmpty()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kConstraintError,
        String::Format("Invalid display size (%s) provided",
                       natural_size.ToString().c_str()));
    return nullptr;
  }

  const auto timestamp = base::TimeDelta::FromMicroseconds(init->timestamp());
  auto& frame_pool =
      CachedVideoFramePool::From(*ExecutionContext::From(script_state));
  auto frame = frame_pool.CreateFrame(media_fmt, coded_size, visible_rect,
                                      natural_size, timestamp);
  if (!frame) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kConstraintError,
        String::Format(
            "Failed to create a video frame with configuration {format:%s, "
            "coded_size:%s, visible_rect:%s, display_size:%s}",
            VideoPixelFormatToString(media_fmt).c_str(),
            coded_size.ToString().c_str(), visible_rect.ToString().c_str(),
            natural_size.ToString().c_str()));
    return nullptr;
  }

  for (wtf_size_t i = 0; i < planes.size(); ++i) {
    const auto minimum_size =
        media::VideoFrame::PlaneSize(media_fmt, i, coded_size);

    DOMArrayPiece buffer(planes[i]->src());

    uint8_t* dest_ptr = frame->visible_data(i);
    const uint8_t* src_ptr = reinterpret_cast<uint8_t*>(buffer.Data());
    for (size_t r = 0; r < planes[i]->rows(); ++r) {
      DCHECK_LE(
          src_ptr + planes[i]->stride(),
          reinterpret_cast<uint8_t*>(buffer.Data()) + buffer.ByteLength());

      memcpy(dest_ptr, src_ptr, minimum_size.width());
      src_ptr += planes[i]->stride();
      dest_ptr += frame->stride(i);
    }
  }

  return MakeGarbageCollected<VideoFrame>(std::move(frame),
                                          ExecutionContext::From(script_state));
}

String VideoFrame::format() const {
  auto local_frame = handle_->frame();
  if (!local_frame || !IsSupportedPlanarFormat(*local_frame))
    return String();

  switch (local_frame->format()) {
    case media::PIXEL_FORMAT_I420:
    case media::PIXEL_FORMAT_I420A:
      return V8VideoPixelFormat(V8VideoPixelFormat::Enum::kI420);
    case media::PIXEL_FORMAT_NV12:
      return V8VideoPixelFormat(V8VideoPixelFormat::Enum::kNV12);
    case media::PIXEL_FORMAT_ABGR:
      return V8VideoPixelFormat(V8VideoPixelFormat::Enum::kABGR);
    case media::PIXEL_FORMAT_XBGR:
      return V8VideoPixelFormat(V8VideoPixelFormat::Enum::kXBGR);
    case media::PIXEL_FORMAT_ARGB:
      return V8VideoPixelFormat(V8VideoPixelFormat::Enum::kARGB);
    case media::PIXEL_FORMAT_XRGB:
      return V8VideoPixelFormat(V8VideoPixelFormat::Enum::kXRGB);
    default:
      NOTREACHED();
      return String();
  }
}

base::Optional<HeapVector<Member<Plane>>> VideoFrame::planes() {
  // Verify that |this| has not been invalidated, and that the format is
  // supported.
  auto local_frame = handle_->frame();
  if (!local_frame || !IsSupportedPlanarFormat(*local_frame))
    return base::nullopt;

  // Create a Plane for each VideoFrame plane, but only the first time.
  if (planes_.IsEmpty()) {
    for (size_t i = 0; i < local_frame->layout().num_planes(); i++) {
      // Note: |handle_| may have been invalidated since |local_frame| was read.
      planes_.push_back(MakeGarbageCollected<Plane>(handle_, i));
    }
  }

  return planes_;
}

uint32_t VideoFrame::codedWidth() const {
  auto local_frame = handle_->frame();
  if (!local_frame)
    return 0;
  return local_frame->coded_size().width();
}

uint32_t VideoFrame::codedHeight() const {
  auto local_frame = handle_->frame();
  if (!local_frame)
    return 0;
  return local_frame->coded_size().height();
}

uint32_t VideoFrame::cropLeft() const {
  auto local_frame = handle_->frame();
  if (!local_frame)
    return 0;
  return local_frame->visible_rect().x();
}

uint32_t VideoFrame::cropTop() const {
  auto local_frame = handle_->frame();
  if (!local_frame)
    return 0;
  return local_frame->visible_rect().y();
}

uint32_t VideoFrame::cropWidth() const {
  auto local_frame = handle_->frame();
  if (!local_frame)
    return 0;
  return local_frame->visible_rect().width();
}

uint32_t VideoFrame::cropHeight() const {
  auto local_frame = handle_->frame();
  if (!local_frame)
    return 0;
  return local_frame->visible_rect().height();
}

uint32_t VideoFrame::displayWidth() const {
  auto local_frame = handle_->frame();
  if (!local_frame)
    return 0;

  const auto transform =
      local_frame->metadata().transformation.value_or(media::kNoTransformation);
  if (transform == media::kNoTransformation ||
      transform.rotation == media::VIDEO_ROTATION_0 ||
      transform.rotation == media::VIDEO_ROTATION_180) {
    return local_frame->natural_size().width();
  }
  return local_frame->natural_size().height();
}

uint32_t VideoFrame::displayHeight() const {
  auto local_frame = handle_->frame();
  if (!local_frame)
    return 0;
  const auto transform =
      local_frame->metadata().transformation.value_or(media::kNoTransformation);
  if (transform == media::kNoTransformation ||
      transform.rotation == media::VIDEO_ROTATION_0 ||
      transform.rotation == media::VIDEO_ROTATION_180) {
    return local_frame->natural_size().height();
  }
  return local_frame->natural_size().width();
}

base::Optional<uint64_t> VideoFrame::timestamp() const {
  auto local_frame = handle_->frame();
  if (!local_frame || local_frame->timestamp() == media::kNoTimestamp)
    return base::nullopt;
  return local_frame->timestamp().InMicroseconds();
}

base::Optional<uint64_t> VideoFrame::duration() const {
  auto local_frame = handle_->frame();
  // TODO(sandersd): Can a duration be kNoTimestamp?
  if (!local_frame || !local_frame->metadata().frame_duration.has_value())
    return base::nullopt;
  return local_frame->metadata().frame_duration->InMicroseconds();
}

void VideoFrame::close() {
  handle_->Invalidate();
}

void VideoFrame::destroy(ExecutionContext* execution_context) {
  execution_context->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::blink::ConsoleMessageSource::kDeprecation,
      mojom::blink::ConsoleMessageLevel::kWarning,
      "VideoFrame.destroy() is deprecated; use VideoFrame.close()."));
  close();
}

VideoFrame* VideoFrame::clone(ScriptState* script_state,
                              ExceptionState& exception_state) {
  VideoFrame* frame = CloneFromNative(ExecutionContext::From(script_state));

  if (!frame) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot clone closed VideoFrame.");
    return nullptr;
  }

  return frame;
}

VideoFrame* VideoFrame::CloneFromNative(ExecutionContext* context) {
  // The returned handle will be nullptr if it was already invalidated.
  auto handle = handle_->Clone();
  return handle ? MakeGarbageCollected<VideoFrame>(std::move(handle)) : nullptr;
}

ScriptPromise VideoFrame::createImageBitmap(ScriptState* script_state,
                                            const ImageBitmapOptions* options,
                                            ExceptionState& exception_state) {
  VideoFrameLogger::From(*ExecutionContext::From(script_state))
      .LogCreateImageBitmapDeprecationNotice();

  base::Optional<IntRect> crop_rect;
  if (auto local_frame = handle_->frame())
    crop_rect = IntRect(local_frame->visible_rect());

  return ImageBitmapFactories::CreateImageBitmap(script_state, this, crop_rect,
                                                 options, exception_state);
}

scoped_refptr<Image> VideoFrame::GetSourceImageForCanvas(
    SourceImageStatus* status,
    const FloatSize&) {
  const auto local_handle = handle_->CloneForInternalUse();
  if (!local_handle) {
    DLOG(ERROR) << "GetSourceImageForCanvas() called for closed frame.";
    *status = kInvalidSourceImageStatus;
    return nullptr;
  }

  const auto orientation_enum = VideoTransformationToImageOrientation(
      local_handle->frame()->metadata().transformation.value_or(
          media::kNoTransformation));
  if (auto sk_img = local_handle->sk_image()) {
    *status = kNormalSourceImageStatus;
    return UnacceleratedStaticBitmapImage::Create(std::move(sk_img),
                                                  orientation_enum);
  }

  const auto image = CreateImageFromVideoFrame(local_handle->frame());
  if (!image) {
    *status = kInvalidSourceImageStatus;
    return nullptr;
  }

  *status = kNormalSourceImageStatus;
  return image;
}

bool VideoFrame::WouldTaintOrigin() const {
  // VideoFrames can't be created from untainted sources currently. If we ever
  // add that ability we will need a tainting signal on the VideoFrame itself.
  // One example would be allowing <video> elements to provide a VideoFrame.
  return false;
}

FloatSize VideoFrame::ElementSize(
    const FloatSize& default_object_size,
    const RespectImageOrientationEnum respect_orientation) const {
  // BitmapSourceSize() will always ignore orientation.
  if (respect_orientation == kRespectImageOrientation) {
    auto local_frame = handle_->frame();
    if (!local_frame)
      return FloatSize();

    const auto orientation_enum = VideoTransformationToImageOrientation(
        local_frame->metadata().transformation.value_or(
            media::kNoTransformation));
    auto orientation_adjusted_size = FloatSize(local_frame->natural_size());
    if (ImageOrientation(orientation_enum).UsesWidthAsHeight())
      return orientation_adjusted_size.TransposedSize();
    return orientation_adjusted_size;
  }
  return FloatSize(BitmapSourceSize());
}

bool VideoFrame::IsVideoFrame() const {
  return true;
}

bool VideoFrame::IsOpaque() const {
  if (auto local_frame = handle_->frame())
    return media::IsOpaque(local_frame->format());
  return false;
}

bool VideoFrame::IsAccelerated() const {
  if (auto local_handle = handle_->CloneForInternalUse()) {
    return handle_->sk_image() ? false
                               : WillCreateAcceleratedImagesFromVideoFrame(
                                     local_handle->frame().get());
  }
  return false;
}

IntSize VideoFrame::BitmapSourceSize() const {
  auto local_frame = handle_->frame();
  if (!local_frame)
    return IntSize();

  // ImageBitmaps should always return the size w/o respecting orientation.
  return IntSize(local_frame->natural_size());
}

ScriptPromise VideoFrame::CreateImageBitmap(ScriptState* script_state,
                                            base::Optional<IntRect> crop_rect,
                                            const ImageBitmapOptions* options,
                                            ExceptionState& exception_state) {
  const auto local_handle = handle_->CloneForInternalUse();
  if (!local_handle) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Cannot create ImageBitmap from closed VideoFrame.");
    return ScriptPromise();
  }

  const auto orientation_enum = VideoTransformationToImageOrientation(
      local_handle->frame()->metadata().transformation.value_or(
          media::kNoTransformation));
  if (auto sk_img = local_handle->sk_image()) {
    auto* image_bitmap = MakeGarbageCollected<ImageBitmap>(
        UnacceleratedStaticBitmapImage::Create(std::move(sk_img),
                                               orientation_enum),
        crop_rect, options);
    return ImageBitmapSource::FulfillImageBitmap(script_state, image_bitmap,
                                                 exception_state);
  }

  const auto image = CreateImageFromVideoFrame(local_handle->frame());
  if (!image) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        String(("Unsupported VideoFrame: " +
                local_handle->frame()->AsHumanReadableString())
                   .c_str()));
    return ScriptPromise();
  }

  auto* image_bitmap =
      MakeGarbageCollected<ImageBitmap>(image, crop_rect, options);
  return ImageBitmapSource::FulfillImageBitmap(script_state, image_bitmap,
                                               exception_state);
}

void VideoFrame::Trace(Visitor* visitor) const {
  visitor->Trace(planes_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
