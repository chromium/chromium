// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"

#include <utility>

#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/resources/single_release_callback.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_metadata.h"
#include "media/base/video_frame_pool.h"
#include "media/base/wait_and_replace_sync_token_client.h"
#include "media/renderers/paint_canvas_video_renderer.h"
#include "media/renderers/video_frame_yuv_converter.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_plane_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_frame_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_pixel_format.h"
#include "third_party/blink/renderer/core/html/canvas/image_data.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap_factories.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_piece.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/libyuv/include/libyuv.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"

namespace blink {

namespace {

bool IsValidSkColorSpace(sk_sp<SkColorSpace> sk_color_space) {
  // Refer to CanvasColorSpaceToGfxColorSpace in CanvasColorParams.
  sk_sp<SkColorSpace> valid_sk_color_spaces[] = {
      gfx::ColorSpace::CreateSRGB().ToSkColorSpace(),
      gfx::ColorSpace::CreateDisplayP3D65().ToSkColorSpace(),
      gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT2020,
                      gfx::ColorSpace::TransferID::GAMMA24)
          .ToSkColorSpace()};
  for (auto& valid_sk_color_space : valid_sk_color_spaces) {
    if (SkColorSpace::Equals(sk_color_space.get(),
                             valid_sk_color_space.get())) {
      return true;
    }
  }
  return false;
}

bool IsValidSkColorType(SkColorType sk_color_type) {
  SkColorType valid_sk_color_types[] = {
      kBGRA_8888_SkColorType, kRGBA_8888_SkColorType,
      // TODO(jie.a.chen@intel.com): Add F16 support.
      // kRGBA_F16_SkColorType
  };
  for (auto& valid_sk_color_type : valid_sk_color_types) {
    if (sk_color_type == valid_sk_color_type) {
      return true;
    }
  }
  return false;
}

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

}  // namespace

VideoFrame::VideoFrame(scoped_refptr<media::VideoFrame> frame,
                       ExecutionContext* context)
    : handle_(
          base::MakeRefCounted<VideoFrameHandle>(std::move(frame), context)) {
  DCHECK(handle_->frame());
}

VideoFrame::VideoFrame(scoped_refptr<VideoFrameHandle> handle)
    : handle_(std::move(handle)) {
  DCHECK(handle_);
}

// static
VideoFrame* VideoFrame::Create(ScriptState* script_state,
                               ImageBitmap* source,
                               VideoFrameInit* init,
                               ExceptionState& exception_state) {
  if (!source) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotFoundError,
                                      "No source was provided");
    return nullptr;
  }

  if (!source->BitmapImage()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid source state");
    return nullptr;
  }

  const gfx::Size coded_size(source->width(), source->height());
  const gfx::Rect visible_rect(coded_size);
  const gfx::Size natural_size = coded_size;
  const auto timestamp = base::TimeDelta::FromMicroseconds(init->timestamp());
  const auto& paint_image = source->BitmapImage()->PaintImageForCurrentFrame();
  const auto sk_image_info = paint_image.GetSkImageInfo();

  auto sk_color_space = sk_image_info.refColorSpace();
  if (!sk_color_space)
    sk_color_space = SkColorSpace::MakeSRGB();
  if (!IsValidSkColorSpace(sk_color_space)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid color space");
    return nullptr;
  }

  const bool is_texture = paint_image.IsTextureBacked();
  const auto sk_image = paint_image.GetSkImage();

  scoped_refptr<media::VideoFrame> frame;

  // Now only SkImage_Gpu implemented the readbackYUV420 method, so for
  // non-texture image, still use libyuv do the csc until SkImage_Base
  // implement asyncRescaleAndReadPixelsYUV420.
  if (is_texture) {
    YUVReadbackContext result;
    result.coded_size = coded_size;
    result.visible_rect = visible_rect;
    result.natural_size = natural_size;
    result.timestamp = timestamp;

    // While this function indicates it's asynchronous, the flushAndSubmit()
    // call below ensures it completes synchronously.
    const auto src_rect = SkIRect::MakeWH(source->width(), source->height());
    sk_image->asyncRescaleAndReadPixelsYUV420(
        kRec709_SkYUVColorSpace, sk_color_space, src_rect,
        {source->width(), source->height()}, SkImage::RescaleGamma::kSrc,
        SkImage::RescaleMode::kRepeatedCubic, &OnYUVReadbackDone, &result);
    GrDirectContext* gr_context =
        source->BitmapImage()->ContextProvider()->GetGrContext();
    DCHECK(gr_context);
    gr_context->flushAndSubmit(/*syncCpu=*/true);

    if (!result.frame) {
      exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                        "YUV conversion error during readback");
      return nullptr;
    }

    frame = std::move(result.frame);
  } else {
    DCHECK(!sk_image->isTextureBacked());

    auto sk_color_type = sk_image_info.colorType();
    if (!IsValidSkColorType(sk_color_type)) {
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        "Invalid pixel format");
      return nullptr;
    }

    DCHECK(sk_color_type == kRGBA_8888_SkColorType ||
           sk_color_type == kBGRA_8888_SkColorType);

    SkPixmap pm;
    const bool peek_result = sk_image->peekPixels(&pm);
    DCHECK(peek_result);

    const auto format = sk_image->isOpaque()
                            ? (sk_color_type == kRGBA_8888_SkColorType
                                   ? media::PIXEL_FORMAT_XBGR
                                   : media::PIXEL_FORMAT_XRGB)
                            : (sk_color_type == kRGBA_8888_SkColorType
                                   ? media::PIXEL_FORMAT_ABGR
                                   : media::PIXEL_FORMAT_ARGB);

    frame = media::VideoFrame::WrapExternalData(
        format, coded_size, visible_rect, natural_size,
        // TODO(crbug.com/1161304): We should be able to wrap readonly memory in
        // a VideoFrame instead of using writable_addr() here.
        reinterpret_cast<uint8_t*>(pm.writable_addr()), pm.computeByteSize(),
        timestamp);
    if (!frame) {
      exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                        "Failed to create video frame");
      return nullptr;
    }
    frame->set_color_space(gfx::ColorSpace(*sk_color_space));
    frame->AddDestructionObserver(ConvertToBaseOnceCallback(CrossThreadBindOnce(
        base::DoNothing::Once<sk_sp<SkImage>>(), std::move(sk_image))));
  }
  auto* result = MakeGarbageCollected<VideoFrame>(
      std::move(frame), ExecutionContext::From(script_state));
  return result;
}

// static
VideoFrame* VideoFrame::Create(ScriptState* script_state,
                               const String& format,
                               const HeapVector<Member<PlaneInit>>& planes,
                               VideoFrameInit* init,
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

  for (size_t i = 0; i < planes.size(); ++i) {
    const auto minimum_size =
        media::VideoFrame::PlaneSize(media_fmt, i, coded_size);
    if (planes[i]->stride() < uint32_t{minimum_size.width()}) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kConstraintError,
          String::Format(
              "The stride of plane %zu is too small for the given coded size "
              "(%s); expected at least %d, received %u",
              i, coded_size.ToString().c_str(), minimum_size.width(),
              planes[i]->stride()));
      return nullptr;
    }
    if (planes[i]->rows() != uint32_t{minimum_size.height()}) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kConstraintError,
          String::Format(
              "The row count for plane %zu is incorrect for the given coded "
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
          String::Format("The size of plane %zu is too large", i));
      return nullptr;
    }

    DOMArrayPiece buffer(planes[i]->src());
    if (buffer.ByteLength() < required_byte_size.ValueOrDie()) {
      // Note: We use GetArea() below instead of area.ValueOrDie() since the
      // base::StrictNumeric seems to confuse the printf() format checks.
      exception_state.ThrowDOMException(
          DOMExceptionCode::kConstraintError,
          String::Format(
              "The size of plane %zu is too small for the given coded "
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

  for (size_t i = 0; i < planes.size(); ++i) {
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

// static
bool VideoFrame::IsSupportedPlanarFormat(media::VideoFrame* frame) {
  if (!frame)
    return false;

  if (!frame->IsMappable() && !frame->HasGpuMemoryBuffer())
    return false;

  const size_t num_planes = frame->layout().num_planes();
  switch (frame->format()) {
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

String VideoFrame::format() const {
  auto local_frame = handle_->frame();
  if (!local_frame || !IsSupportedPlanarFormat(local_frame.get()))
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
  if (!local_frame || !IsSupportedPlanarFormat(local_frame.get()))
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
  return local_frame->natural_size().width();
}

uint32_t VideoFrame::displayHeight() const {
  auto local_frame = handle_->frame();
  if (!local_frame)
    return 0;
  return local_frame->natural_size().height();
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
  // TODO(tguilbert): Add a warning when closing already closed frames?
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
  auto frame = handle_->frame();

  // The handle was already invalidated.
  if (!frame)
    return nullptr;

  return MakeGarbageCollected<VideoFrame>(std::move(frame), context);
}

scoped_refptr<VideoFrameHandle> VideoFrame::handle() {
  return handle_;
}

scoped_refptr<media::VideoFrame> VideoFrame::frame() {
  return handle_->frame();
}

scoped_refptr<const media::VideoFrame> VideoFrame::frame() const {
  return handle_->frame();
}

ScriptPromise VideoFrame::createImageBitmap(ScriptState* script_state,
                                            const ImageBitmapOptions* options,
                                            ExceptionState& exception_state) {
  base::Optional<IntRect> crop_rect;

  if (auto local_frame = handle_->frame())
    crop_rect = IntRect(local_frame->visible_rect());

  return ImageBitmapFactories::CreateImageBitmap(script_state, this, crop_rect,
                                                 options, exception_state);
}

IntSize VideoFrame::BitmapSourceSize() const {
  // TODO(crbug.com/1096724): Should be scaled to display size.
  return IntSize(cropWidth(), cropHeight());
}

ScriptPromise VideoFrame::CreateImageBitmap(ScriptState* script_state,
                                            base::Optional<IntRect> crop_rect,
                                            const ImageBitmapOptions* options,
                                            ExceptionState& exception_state) {
  auto local_frame = frame();

  if (!local_frame) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Cannot create ImageBitmap from destroyed VideoFrame.");
    return ScriptPromise();
  }

  // SharedImage optimization: create AcceleratedStaticBitmapImage directly.
  // Disabled on Android because the hardware decode implementation may neuter
  // frames, which would violate ImageBitmap requirements.
  // TODO(sandersd): Handle YUV pixel formats.
  // TODO(sandersd): Handle high bit depth formats.
#if !defined(OS_ANDROID)
  if (local_frame->NumTextures() == 1 &&
      local_frame->mailbox_holder(0).mailbox.IsSharedImage() &&
      (local_frame->format() == media::PIXEL_FORMAT_ARGB ||
       local_frame->format() == media::PIXEL_FORMAT_XRGB ||
       local_frame->format() == media::PIXEL_FORMAT_ABGR ||
       local_frame->format() == media::PIXEL_FORMAT_XBGR ||
       local_frame->format() == media::PIXEL_FORMAT_BGRA)) {
    // TODO(sandersd): Do we need to be able to handle limited-range RGB? It
    // may never happen, and SkColorSpace doesn't know about it.
    auto sk_color_space =
        local_frame->ColorSpace().GetAsFullRangeRGB().ToSkColorSpace();
    if (!sk_color_space)
      sk_color_space = SkColorSpace::MakeSRGB();

    const SkImageInfo sk_image_info = SkImageInfo::Make(
        local_frame->coded_size().width(), local_frame->coded_size().height(),
        kN32_SkColorType, kUnpremul_SkAlphaType, std::move(sk_color_space));

    // Hold a ref by storing it in the release callback.
    auto release_callback = viz::SingleReleaseCallback::Create(
        WTF::Bind([](scoped_refptr<media::VideoFrame> frame,
                     const gpu::SyncToken& sync_token, bool is_lost) {},
                  local_frame));

    scoped_refptr<StaticBitmapImage> image =
        AcceleratedStaticBitmapImage::CreateFromCanvasMailbox(
            local_frame->mailbox_holder(0).mailbox,
            local_frame->mailbox_holder(0).sync_token, 0u, sk_image_info,
            local_frame->mailbox_holder(0).texture_target, true,
            // Pass nullptr for |context_provider_wrapper|, because we don't
            // know which context the mailbox came from. It is used only to
            // detect when the mailbox is invalid due to context loss, and is
            // ignored when |is_cross_thread|.
            base::WeakPtr<WebGraphicsContext3DProviderWrapper>(),
            // Pass null |context_thread_ref|, again because we don't know
            // which context the mailbox came from. This should always trigger
            // |is_cross_thread|.
            base::PlatformThreadRef(),
            // The task runner is only used for |release_callback|.
            Thread::Current()->GetTaskRunner(), std::move(release_callback));
    ImageBitmap* image_bitmap =
        MakeGarbageCollected<ImageBitmap>(image, crop_rect, options);
    return ImageBitmapSource::FulfillImageBitmap(script_state, image_bitmap,
                                                 exception_state);
  }
#endif  // !defined(OS_ANDROID)

  const bool is_rgb = local_frame->format() == media::PIXEL_FORMAT_ARGB ||
                      local_frame->format() == media::PIXEL_FORMAT_XRGB ||
                      local_frame->format() == media::PIXEL_FORMAT_ABGR ||
                      local_frame->format() == media::PIXEL_FORMAT_XBGR;

  if ((local_frame->IsMappable() &&
       (local_frame->format() == media::PIXEL_FORMAT_I420 ||
        local_frame->format() == media::PIXEL_FORMAT_I420A)) ||
      (local_frame->HasTextures() &&
       (local_frame->format() == media::PIXEL_FORMAT_I420 ||
        local_frame->format() == media::PIXEL_FORMAT_I420A ||
        local_frame->format() == media::PIXEL_FORMAT_NV12)) ||
      is_rgb) {
    scoped_refptr<StaticBitmapImage> image;
    gfx::ColorSpace gfx_color_space = local_frame->ColorSpace();
    gfx_color_space = gfx_color_space.GetWithMatrixAndRange(
        gfx::ColorSpace::MatrixID::RGB, gfx::ColorSpace::RangeID::FULL);
    auto sk_color_space = gfx_color_space.ToSkColorSpace();
    if (!sk_color_space)
      sk_color_space = SkColorSpace::MakeSRGB();

    const bool prefer_accelerated_image_bitmap =
        local_frame->format() != media::PIXEL_FORMAT_I420A &&
        (BitmapSourceSize().Area() > kCpuEfficientFrameSize ||
         local_frame->HasTextures()) &&
        (!is_rgb || local_frame->HasTextures());

    if (!prefer_accelerated_image_bitmap) {
      size_t bytes_per_row = sizeof(SkColor) * cropWidth();
      size_t image_pixels_size = bytes_per_row * cropHeight();

      sk_sp<SkData> image_pixels = TryAllocateSkData(image_pixels_size);
      if (!image_pixels) {
        exception_state.ThrowDOMException(DOMExceptionCode::kBufferOverrunError,
                                          "Out of memory.");
        return ScriptPromise();
      }
      media::PaintCanvasVideoRenderer::ConvertVideoFrameToRGBPixels(
          local_frame.get(), image_pixels->writable_data(), bytes_per_row);

      SkImageInfo info =
          SkImageInfo::Make(cropWidth(), cropHeight(), kN32_SkColorType,
                            kUnpremul_SkAlphaType, std::move(sk_color_space));
      sk_sp<SkImage> skImage =
          SkImage::MakeRasterData(info, image_pixels, bytes_per_row);
      image = UnacceleratedStaticBitmapImage::Create(std::move(skImage));
    } else {
      scoped_refptr<viz::RasterContextProvider> raster_context_provider;
      base::WeakPtr<WebGraphicsContext3DProviderWrapper> wrapper =
          SharedGpuContext::ContextProviderWrapper();
      if (wrapper && wrapper->ContextProvider()) {
        raster_context_provider = base::WrapRefCounted(
            wrapper->ContextProvider()->RasterContextProvider());
      }
      if (!raster_context_provider) {
        exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                          "Graphics context unavailable.");
        return ScriptPromise();
      }

      auto* ri = raster_context_provider->RasterInterface();
      gpu::SharedImageInterface* shared_image_interface =
          raster_context_provider->SharedImageInterface();
      uint32_t usage =
          gpu::SHARED_IMAGE_USAGE_GLES2 | gpu::SHARED_IMAGE_USAGE_DISPLAY;
      if (raster_context_provider->ContextCapabilities().supports_oop_raster) {
        usage |= gpu::SHARED_IMAGE_USAGE_RASTER |
                 gpu::SHARED_IMAGE_USAGE_OOP_RASTERIZATION;
      }

      gpu::MailboxHolder dest_holder;
      // Use coded_size() to comply with media::ConvertFromVideoFrameYUV.
      dest_holder.mailbox = shared_image_interface->CreateSharedImage(
          viz::ResourceFormat::RGBA_8888, local_frame->coded_size(),
          gfx::ColorSpace(), kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType,
          usage, gpu::kNullSurfaceHandle);
      dest_holder.sync_token = shared_image_interface->GenUnverifiedSyncToken();
      dest_holder.texture_target = GL_TEXTURE_2D;

      if (local_frame->NumTextures() == 1) {
        ri->WaitSyncTokenCHROMIUM(dest_holder.sync_token.GetConstData());
        ri->CopySubTexture(
            local_frame->mailbox_holder(0).mailbox, dest_holder.mailbox,
            GL_TEXTURE_2D, 0, 0, 0, 0, local_frame->coded_size().width(),
            local_frame->coded_size().height(), GL_FALSE, GL_FALSE);
      } else {
        media::VideoFrameYUVConverter::ConvertYUVVideoFrameNoCaching(
            local_frame.get(), raster_context_provider.get(), dest_holder);
      }

      gpu::SyncToken sync_token;
      ri->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());

      auto release_callback = viz::SingleReleaseCallback::Create(base::BindOnce(
          [](scoped_refptr<viz::RasterContextProvider> provider,
             gpu::Mailbox mailbox, const gpu::SyncToken& sync_token,
             bool is_lost) {
            provider->SharedImageInterface()->DestroySharedImage(sync_token,
                                                                 mailbox);
          },
          raster_context_provider, dest_holder.mailbox));

      const SkImageInfo sk_image_info =
          SkImageInfo::Make(codedWidth(), codedHeight(), kN32_SkColorType,
                            kUnpremul_SkAlphaType, std::move(sk_color_space));

      image = AcceleratedStaticBitmapImage::CreateFromCanvasMailbox(
          dest_holder.mailbox, sync_token, 0u, sk_image_info,
          dest_holder.texture_target, true,
          SharedGpuContext::ContextProviderWrapper(),
          base::PlatformThread::CurrentRef(),
          Thread::Current()->GetTaskRunner(), std::move(release_callback));

      if (local_frame->HasTextures()) {
        // Attach a new sync token to |local_frame|, so it's not destroyed
        // before |image| is fully created.
        media::WaitAndReplaceSyncTokenClient client(ri);
        local_frame->UpdateReleaseSyncToken(&client);
      }
    }

    ImageBitmap* image_bitmap =
        MakeGarbageCollected<ImageBitmap>(image, crop_rect, options);
    return ImageBitmapSource::FulfillImageBitmap(script_state, image_bitmap,
                                                 exception_state);
  }

  exception_state.ThrowDOMException(
      DOMExceptionCode::kNotSupportedError,
      String(("Unsupported VideoFrame: " + local_frame->AsHumanReadableString())
                 .c_str()));
  return ScriptPromise();
}

void VideoFrame::Trace(Visitor* visitor) const {
  visitor->Trace(planes_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
