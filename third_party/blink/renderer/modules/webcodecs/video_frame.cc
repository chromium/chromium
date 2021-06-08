// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"

#include <utility>

#include "base/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/checked_math.h"
#include "media/base/limits.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_metadata.h"
#include "media/base/video_frame_pool.h"
#include "media/base/video_util.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/bindings/core/v8/array_buffer_or_array_buffer_view.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_plane_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_cssimagevalue_htmlcanvaselement_htmlimageelement_htmlvideoelement_imagebitmap_offscreencanvas_svgimageelement_videoframe.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_frame_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_frame_plane_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_pixel_format.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_image_source.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_piece.h"
#include "third_party/blink/renderer/modules/canvas/imagebitmap/image_bitmap_factories.h"
#include "third_party/blink/renderer/modules/webcodecs/parsed_read_into_options.h"
#include "third_party/blink/renderer/modules/webcodecs/plane_layout.h"
#include "third_party/blink/renderer/modules/webcodecs/webcodecs_logger.h"
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
    case V8VideoPixelFormat::Enum::kI422:
      return media::PIXEL_FORMAT_I422;
    case V8VideoPixelFormat::Enum::kI444:
      return media::PIXEL_FORMAT_I444;
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

media::VideoPixelFormat ToOpaqueMediaPixelFormat(media::VideoPixelFormat fmt) {
  DCHECK(!media::IsOpaque(fmt));
  switch (fmt) {
    case media::PIXEL_FORMAT_I420A:
      return media::PIXEL_FORMAT_I420;
    case media::PIXEL_FORMAT_ARGB:
      return media::PIXEL_FORMAT_XRGB;
    case media::PIXEL_FORMAT_ABGR:
      return media::PIXEL_FORMAT_XBGR;
    default:
      NOTIMPLEMENTED() << "Missing support for making " << fmt << " opaque.";
      return fmt;
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
    case media::PIXEL_FORMAT_I422:
    case media::PIXEL_FORMAT_I444:
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
#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
                               const V8CanvasImageSource* source,
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
                               const CanvasImageSourceUnion& source,
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
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

  constexpr char kAlphaDiscard[] = "discard";
  constexpr char kAlphaKeep[] = "keep";

  // Special case <video> and VideoFrame to directly use the underlying frame.
  if (
#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
      source->IsVideoFrame() || source->IsHTMLVideoElement()
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
      source.IsVideoFrame() || source.IsHTMLVideoElement()
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  ) {
    scoped_refptr<media::VideoFrame> source_frame;
#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
    switch (source->GetContentType()) {
      case V8CanvasImageSource::ContentType::kVideoFrame:
        if (!init || (!init->hasTimestamp() && !init->hasDuration() &&
                      init->alpha() == kAlphaKeep)) {
          return source->GetAsVideoFrame()->clone(exception_state);
        }
        source_frame = source->GetAsVideoFrame()->frame();
        break;
      case V8CanvasImageSource::ContentType::kHTMLVideoElement:
        if (auto* wmp = source->GetAsHTMLVideoElement()->GetWebMediaPlayer())
          source_frame = wmp->GetCurrentFrame();
        break;
      default:
        NOTREACHED();
    }
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
    if (source.IsVideoFrame()) {
      if (!init || (!init->hasTimestamp() && !init->hasDuration() &&
                    init->alpha() == kAlphaKeep)) {
        return source.GetAsVideoFrame()->clone(exception_state);
      }
      source_frame = source.GetAsVideoFrame()->frame();
    } else if (source.IsHTMLVideoElement()) {
      if (auto* wmp = source.GetAsHTMLVideoElement()->GetWebMediaPlayer())
        source_frame = wmp->GetCurrentFrame();
    }
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)

    if (!source_frame) {
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        "Invalid source state");
      return nullptr;
    }

    const bool force_opaque = init && init->alpha() == kAlphaDiscard &&
                              !media::IsOpaque(source_frame->format());

    // We can't modify the timestamp or duration directly since there may be
    // other owners accessing these fields concurrently.
    if (init && (init->hasTimestamp() || init->hasDuration() || force_opaque)) {
      const auto wrapped_format =
          force_opaque ? ToOpaqueMediaPixelFormat(source_frame->format())
                       : source_frame->format();
      auto wrapped_frame = media::VideoFrame::WrapVideoFrame(
          source_frame, wrapped_format, source_frame->visible_rect(),
          source_frame->natural_size());
      wrapped_frame->set_color_space(source_frame->ColorSpace());
      if (init->hasTimestamp()) {
        wrapped_frame->set_timestamp(
            base::TimeDelta::FromMicroseconds(init->timestamp()));
      }
      if (init->hasDuration()) {
        wrapped_frame->metadata().frame_duration =
            base::TimeDelta::FromMicroseconds(init->duration());
      }
      source_frame = std::move(wrapped_frame);
    }

    return MakeGarbageCollected<VideoFrame>(
        std::move(source_frame), ExecutionContext::From(script_state));
  }

  // Some elements like OffscreenCanvas won't choose a default size, so we must
  // ask them what size they think they are first.
  auto source_size =
      image_source->ElementSize(FloatSize(), kRespectImageOrientation);

  SourceImageStatus status = kInvalidSourceImageStatus;
  auto image = image_source->GetSourceImageForCanvas(&status, source_size);
  if (!image || status != kNormalSourceImageStatus) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid source state");
    return nullptr;
  }

  const auto timestamp = base::TimeDelta::FromMicroseconds(
      (init && init->hasTimestamp()) ? init->timestamp() : 0);

  // Note: The current PaintImage may be lazy generated, for simplicity, we just
  // ask Skia to rasterize the image for us.
  //
  // A potential optimization could use PaintImage::DecodeYuv() to decode
  // directly into a media::VideoFrame. This would improve VideoFrame from <img>
  // creation, but probably such users should be using ImageDecoder directly.
  //
  // TODO(crbug.com/1031051): PaintImage::GetSkImage() is being deprecated as we
  // move to OOPR canvas2D. In OOPR mode it will return null so we fall back to
  // GetSwSkImage(). This area should be updated once VideoFrame can wrap
  // mailboxes.
  auto sk_image = image->PaintImageForCurrentFrame().GetSkImage();
  if (!sk_image)
    sk_image = image->PaintImageForCurrentFrame().GetSwSkImage();
  if (sk_image->isLazyGenerated())
    sk_image = sk_image->makeRasterImage();

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

    // TODO(crbug.com/1138681): This is currently wrong for alpha == keep, but
    // we're removing readback for this flow, so it's fine for now.

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

  const bool force_opaque =
      init && init->alpha() == kAlphaDiscard && !sk_image->isOpaque();

  frame = media::CreateFromSkImage(sk_image, visible_rect, natural_size,
                                   timestamp, force_opaque);
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

// TODO(crbug.com/1198324): Merge shared logic with VideoDecoderConfig.
// static
VideoFrame* VideoFrame::Create(ScriptState* script_state,
                               const HeapVector<Member<PlaneInit>>& planes,
                               const VideoFramePlaneInit* init,
                               ExceptionState& exception_state) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);

  // Handle format; the string was validated by the V8 binding.
  auto typed_fmt = V8VideoPixelFormat::Create(init->format());
  auto media_fmt = ToMediaPixelFormat(typed_fmt->AsEnum());

  // There's no I420A pixel format, so treat I420 + 4 planes as I420A.
  if (media_fmt == media::PIXEL_FORMAT_I420 && planes.size() == 4)
    media_fmt = media::PIXEL_FORMAT_I420A;

  // Validate coded size.
  uint32_t coded_width = init->codedWidth();
  uint32_t coded_height = init->codedHeight();
  if (coded_width == 0 || coded_width > media::limits::kMaxDimension ||
      coded_height == 0 || coded_height > media::limits::kMaxDimension ||
      coded_width * coded_height > media::limits::kMaxCanvas) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kConstraintError,
        String::Format("Invalid coded size (%u, %u).", coded_width,
                       coded_height));
    return nullptr;
  }

  const gfx::Size coded_size(coded_width, coded_height);

  // Validate visible rect.
  uint32_t visible_left = 0;
  uint32_t visible_top = 0;
  uint32_t visible_width = coded_width;
  uint32_t visible_height = coded_height;
  if (init->hasVisibleRegion()) {
    visible_left = init->visibleRegion()->left();
    visible_top = init->visibleRegion()->top();
    visible_width = init->visibleRegion()->width();
    visible_height = init->visibleRegion()->height();
  } else {
    if (init->hasCropLeft()) {
      WebCodecsLogger::From(*execution_context).LogCropDeprecation();
      visible_left = init->cropLeft();
      if (visible_left >= coded_width) {
        exception_state.ThrowDOMException(
            DOMExceptionCode::kConstraintError,
            String::Format("Invalid cropLeft %u for codedWidth %u.",
                           visible_left, coded_width));
        return nullptr;
      }
      visible_width = coded_width - visible_left;
    }
    if (init->hasCropTop()) {
      WebCodecsLogger::From(*execution_context).LogCropDeprecation();
      visible_top = init->cropTop();
      if (visible_top >= coded_height) {
        exception_state.ThrowDOMException(
            DOMExceptionCode::kConstraintError,
            String::Format("Invalid cropTop %u for codedHeight %u.",
                           visible_top, coded_height));
        return nullptr;
      }
      visible_height = coded_height - visible_top;
    }
    if (init->hasCropWidth()) {
      WebCodecsLogger::From(*execution_context).LogCropDeprecation();
      visible_width = init->cropWidth();
    }
    if (init->hasCropHeight()) {
      WebCodecsLogger::From(*execution_context).LogCropDeprecation();
      visible_height = init->cropHeight();
    }
  }
  if (visible_left >= coded_width || visible_top >= coded_height ||
      visible_width == 0 || visible_width > media::limits::kMaxDimension ||
      visible_height == 0 || visible_height > media::limits::kMaxDimension ||
      visible_left + visible_width > coded_width ||
      visible_top + visible_height > coded_height) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kConstraintError,
        String::Format("Invalid visible region {left: %u, top: %u, width: %u, "
                       "height: %u} for coded size (%u, %u).",
                       visible_left, visible_top, visible_width, visible_height,
                       coded_width, coded_height));
    return nullptr;
  }

  const gfx::Rect visible_rect(visible_left, visible_top, visible_width,
                               visible_height);

  // Validate natural size.
  uint32_t natural_width = visible_width;
  uint32_t natural_height = visible_height;
  if (init->hasDisplayWidth() || init->hasDisplayHeight()) {
    if (!init->hasDisplayWidth()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kConstraintError,
          String::Format("Invalid display size, displayHeight specified "
                         "without displayWidth."));
      return nullptr;
    }
    if (!init->hasDisplayHeight()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kConstraintError,
          String::Format("Invalid display size, displayWidth specified "
                         "without displayHeight."));
      return nullptr;
    }

    natural_width = init->displayWidth();
    natural_height = init->displayHeight();
    if (natural_width == 0 || natural_width > media::limits::kMaxDimension ||
        natural_height == 0 || natural_height > media::limits::kMaxDimension) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kConstraintError,
          String::Format("Invalid display size (%u, %u).", natural_width,
                         natural_height));
      return nullptr;
    }
  }

  const gfx::Size natural_size(natural_width, natural_height);

  // Validate planes.
  if (media::VideoFrame::NumPlanes(media_fmt) != planes.size()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kConstraintError,
        String::Format("Invalid number of planes for format %s; expected %zu, "
                       "received %u.",
                       init->format().Ascii().c_str(),
                       media::VideoFrame::NumPlanes(media_fmt), planes.size()));
    return nullptr;
  }

  for (wtf_size_t i = 0; i < planes.size(); ++i) {
    if (!planes[i]->hasData()) {
      if (planes[i]->hasSrc()) {
        WebCodecsLogger::From(*execution_context).LogPlaneInitSrcDeprecation();
      } else {
        // TODO(sandersd): Make |data| an actual required member.
        exception_state.ThrowTypeError(String::Format(
            "Required member 'data' is missing for plane %u.", i));
        return nullptr;
      }
    }
  }

  for (wtf_size_t i = 0; i < planes.size(); ++i) {
    DOMArrayPiece buffer(planes[i]->hasData() ? planes[i]->data()
                                              : planes[i]->src());

    size_t offset = 0;
    if (planes[i]->hasOffset())
      offset = planes[i]->offset();

    const size_t stride = planes[i]->stride();

    const gfx::Size plane_size =
        media::VideoFrame::PlaneSize(media_fmt, i, coded_size);
    const size_t minimum_stride = plane_size.width();
    const size_t rows = plane_size.height();
    if (stride < minimum_stride) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kConstraintError,
          String::Format(
              "The stride of plane %u is too small for the given coded size "
              "(%u, %u); expected at least %zu, received %zu",
              i, coded_width, coded_height, minimum_stride, stride));
      return nullptr;
    }

    // Note: This check requires the full stride to be provided for every row,
    // including the last.
    const auto end = base::CheckedNumeric<size_t>(stride) * rows + offset;
    if (!end.IsValid() || end.ValueOrDie() > buffer.ByteLength()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kConstraintError,
          String::Format(
              "Plane %u with %zu rows of stride %zu bytes does not fit at "
              "offset %zu in src buffer with length %zu.",
              i, rows, stride, offset, buffer.ByteLength()));
      return nullptr;
    }
  }

  // Create a frame.
  const auto timestamp = base::TimeDelta::FromMicroseconds(init->timestamp());
  auto& frame_pool = CachedVideoFramePool::From(*execution_context);
  auto frame = frame_pool.CreateFrame(media_fmt, coded_size, visible_rect,
                                      natural_size, timestamp);
  if (!frame) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kConstraintError,
        String::Format(
            "Failed to create a video frame with configuration {format: %s, "
            "coded_size: %s, visible_rect: %s, display_size: %s}",
            VideoPixelFormatToString(media_fmt).c_str(),
            coded_size.ToString().c_str(), visible_rect.ToString().c_str(),
            natural_size.ToString().c_str()));
    return nullptr;
  }

  if (init->hasDuration()) {
    frame->metadata().frame_duration =
        base::TimeDelta::FromMicroseconds(init->duration());
  }

  // Copy data.
  for (wtf_size_t i = 0; i < planes.size(); ++i) {
    DOMArrayPiece buffer(planes[i]->hasData() ? planes[i]->data()
                                              : planes[i]->src());
    size_t offset = 0;
    if (planes[i]->hasOffset())
      offset = planes[i]->offset();
    const size_t stride = planes[i]->stride();

    const gfx::Size plane_size =
        media::VideoFrame::PlaneSize(media_fmt, i, coded_size);
    const size_t minimum_stride = plane_size.width();
    const size_t rows = plane_size.height();

    uint8_t* src_ptr = reinterpret_cast<uint8_t*>(buffer.Data()) + offset;
    uint8_t* dst_ptr = frame->data(i);
    for (size_t row = 0; row < rows; ++row) {
      memcpy(dst_ptr, src_ptr, minimum_stride);
      src_ptr += stride;
      dst_ptr += frame->stride(i);
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
    case media::PIXEL_FORMAT_I422:
      return V8VideoPixelFormat(V8VideoPixelFormat::Enum::kI422);
    case media::PIXEL_FORMAT_I444:
      return V8VideoPixelFormat(V8VideoPixelFormat::Enum::kI444);
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

absl::optional<HeapVector<Member<Plane>>> VideoFrame::planes() {
  // Verify that |this| has not been invalidated, and that the format is
  // supported.
  auto local_frame = handle_->frame();
  if (!local_frame || !IsSupportedPlanarFormat(*local_frame))
    return absl::nullopt;

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

VideoFrameRegion* VideoFrame::codedRegion() const {
  auto local_frame = handle_->frame();
  auto* region = MakeGarbageCollected<VideoFrameRegion>();
  region->setLeft(0);
  region->setTop(0);
  if (local_frame) {
    region->setWidth(local_frame->coded_size().width());
    region->setHeight(local_frame->coded_size().height());
  } else {
    region->setWidth(0);
    region->setHeight(0);
  }
  return region;
}

VideoFrameRegion* VideoFrame::visibleRegion() const {
  auto local_frame = handle_->frame();
  auto* region = MakeGarbageCollected<VideoFrameRegion>();
  if (local_frame) {
    region->setLeft(local_frame->visible_rect().x());
    region->setTop(local_frame->visible_rect().y());
    region->setWidth(local_frame->visible_rect().width());
    region->setHeight(local_frame->visible_rect().height());
  } else {
    region->setLeft(0);
    region->setTop(0);
    region->setWidth(0);
    region->setHeight(0);
  }
  return region;
}

uint32_t VideoFrame::cropLeft(ExecutionContext* execution_context) const {
  WebCodecsLogger::From(*execution_context).LogCropDeprecation();
  auto local_frame = handle_->frame();
  if (!local_frame)
    return 0;
  return local_frame->visible_rect().x();
}

uint32_t VideoFrame::cropTop(ExecutionContext* execution_context) const {
  WebCodecsLogger::From(*execution_context).LogCropDeprecation();
  auto local_frame = handle_->frame();
  if (!local_frame)
    return 0;
  return local_frame->visible_rect().y();
}

uint32_t VideoFrame::cropWidth(ExecutionContext* execution_context) const {
  WebCodecsLogger::From(*execution_context).LogCropDeprecation();
  auto local_frame = handle_->frame();
  if (!local_frame)
    return 0;
  return local_frame->visible_rect().width();
}

uint32_t VideoFrame::cropHeight(ExecutionContext* execution_context) const {
  WebCodecsLogger::From(*execution_context).LogCropDeprecation();
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

absl::optional<int64_t> VideoFrame::timestamp() const {
  auto local_frame = handle_->frame();
  if (!local_frame || local_frame->timestamp() == media::kNoTimestamp)
    return absl::nullopt;
  return local_frame->timestamp().InMicroseconds();
}

absl::optional<uint64_t> VideoFrame::duration() const {
  auto local_frame = handle_->frame();
  // TODO(sandersd): Can a duration be kNoTimestamp?
  if (!local_frame || !local_frame->metadata().frame_duration.has_value())
    return absl::nullopt;
  return local_frame->metadata().frame_duration->InMicroseconds();
}

uint32_t VideoFrame::allocationSize(VideoFrameReadIntoOptions* options,
                                    ExceptionState& exception_state) {
  auto local_frame = handle_->frame();
  if (!local_frame)
    return 0;

  // TODO(crbug.com/1176464): Determine the format readback will occur in, use
  // that to compute the layout.
  if (!IsSupportedPlanarFormat(*local_frame)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "allocationSize() is not yet implemented when format is null.");
    return 0;
  }

  ParsedReadIntoOptions layout(options, local_frame->format(),
                               local_frame->coded_size(),
                               local_frame->visible_rect(), exception_state);
  if (exception_state.HadException())
    return 0;

  return layout.min_buffer_size;
}

ScriptPromise VideoFrame::readInto(ScriptState* script_state,
#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
                                   const V8BufferSource* destination,
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
                                   const ArrayBufferOrArrayBufferView&
                                       destination,
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
                                   VideoFrameReadIntoOptions* options,
                                   ExceptionState& exception_state) {
  auto local_frame = handle_->frame();
  if (!local_frame) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot read closed VideoFrame.");
    return ScriptPromise();
  }

  // TODO(crbug.com/1176464): Use async texture readback.
  if (!IsSupportedPlanarFormat(*local_frame)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "readInto() is not yet implemented when format is null.");
    return ScriptPromise();
  }

  // Compute layout.
  ParsedReadIntoOptions layout(options, local_frame->format(),
                               local_frame->coded_size(),
                               local_frame->visible_rect(), exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  // Validate destination buffer.
  DOMArrayPiece buffer(destination);
  if (buffer.ByteLength() < static_cast<size_t>(layout.min_buffer_size)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kConstraintError,
                                      "destination is not large enough.");
    return ScriptPromise();
  }

  // Map buffers if necessary.
  if (!local_frame->IsMappable()) {
    DCHECK(local_frame->HasGpuMemoryBuffer());
    local_frame = media::ConvertToMemoryMappedFrame(local_frame);
    if (!local_frame) {
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        "Failed to read VideoFrame data.");
      return ScriptPromise();
    }
  }

  // Copy data.
  for (wtf_size_t i = 0; i < layout.num_planes; i++) {
    size_t src_stride = local_frame->stride(i);
    size_t dest_stride = layout.planes[i].stride;
    size_t row_bytes = layout.planes[i].width_bytes;

    uint8_t* src = local_frame->data(i) + layout.planes[i].top * src_stride +
                   layout.planes[i].left_bytes;
    uint8_t* dest = buffer.Bytes() + layout.planes[i].offset;

    // TODO(crbug.com/1205176): Use libyuv::CopyPlane(). The requirements to use
    // it are a bit strange though, it computes with ints and expects
    // intermediate values (like stride * height) to fit.
    for (size_t row = 0; row < layout.planes[i].height; row++) {
      // TODO(crbug.com/1205175): Spec what happens to the gaps between
      // |row_bytes| and |dest_stride|. Probably needs to be compatible with
      // whatever libyuv does.
      memcpy(dest, src, row_bytes);
      src += src_stride;
      dest += dest_stride;
    }
  }

  // Convert and return |layout|.
  HeapVector<Member<PlaneLayout>> result;
  for (wtf_size_t i = 0; i < layout.num_planes; i++) {
    auto* plane = MakeGarbageCollected<PlaneLayout>();
    plane->setOffset(layout.planes[i].offset);
    plane->setStride(layout.planes[i].stride);
    result.push_back(plane);
  }
  return ScriptPromise::Cast(script_state, ToV8(result, script_state));
}

void VideoFrame::close() {
  handle_->Invalidate();
}

VideoFrame* VideoFrame::clone(ExceptionState& exception_state) {
  auto handle = handle_->Clone();
  if (!handle) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot clone closed VideoFrame.");
    return nullptr;
  }

  return MakeGarbageCollected<VideoFrame>(std::move(handle));
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
                                            absl::optional<IntRect> crop_rect,
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
