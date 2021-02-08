// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/webrtc/webrtc_video_frame_adapter.h"

#include "base/callback_helpers.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "media/base/video_util.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include "third_party/libyuv/include/libyuv/convert_from_argb.h"
#include "third_party/libyuv/include/libyuv/scale.h"
#include "third_party/webrtc/api/video/i420_buffer.h"
#include "third_party/webrtc/common_video/include/video_frame_buffer.h"
#include "third_party/webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "third_party/webrtc/rtc_base/ref_counted_object.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace {

class I420FrameAdapter : public webrtc::I420BufferInterface {
 public:
  explicit I420FrameAdapter(scoped_refptr<media::VideoFrame> frame)
      : frame_(std::move(frame)) {
    DCHECK_EQ(frame_->format(), media::PIXEL_FORMAT_I420);
    DCHECK_EQ(frame_->visible_rect().size(), frame_->natural_size());
  }

  int width() const override { return frame_->visible_rect().width(); }
  int height() const override { return frame_->visible_rect().height(); }

  const uint8_t* DataY() const override {
    return frame_->visible_data(media::VideoFrame::kYPlane);
  }

  const uint8_t* DataU() const override {
    return frame_->visible_data(media::VideoFrame::kUPlane);
  }

  const uint8_t* DataV() const override {
    return frame_->visible_data(media::VideoFrame::kVPlane);
  }

  int StrideY() const override {
    return frame_->stride(media::VideoFrame::kYPlane);
  }

  int StrideU() const override {
    return frame_->stride(media::VideoFrame::kUPlane);
  }

  int StrideV() const override {
    return frame_->stride(media::VideoFrame::kVPlane);
  }

 protected:
  scoped_refptr<media::VideoFrame> frame_;
};

class I420AFrameAdapter : public webrtc::I420ABufferInterface {
 public:
  explicit I420AFrameAdapter(scoped_refptr<media::VideoFrame> frame)
      : frame_(std::move(frame)) {
    DCHECK_EQ(frame_->format(), media::PIXEL_FORMAT_I420A);
    DCHECK_EQ(frame_->visible_rect().size(), frame_->natural_size());
  }

  int width() const override { return frame_->visible_rect().width(); }
  int height() const override { return frame_->visible_rect().height(); }

  const uint8_t* DataY() const override {
    return frame_->visible_data(media::VideoFrame::kYPlane);
  }

  const uint8_t* DataU() const override {
    return frame_->visible_data(media::VideoFrame::kUPlane);
  }

  const uint8_t* DataV() const override {
    return frame_->visible_data(media::VideoFrame::kVPlane);
  }

  const uint8_t* DataA() const override {
    return frame_->visible_data(media::VideoFrame::kAPlane);
  }

  int StrideY() const override {
    return frame_->stride(media::VideoFrame::kYPlane);
  }

  int StrideU() const override {
    return frame_->stride(media::VideoFrame::kUPlane);
  }

  int StrideV() const override {
    return frame_->stride(media::VideoFrame::kVPlane);
  }

  int StrideA() const override {
    return frame_->stride(media::VideoFrame::kAPlane);
  }

 protected:
  scoped_refptr<media::VideoFrame> frame_;
};

class NV12FrameAdapter : public webrtc::NV12BufferInterface {
 public:
  explicit NV12FrameAdapter(scoped_refptr<media::VideoFrame> frame)
      : frame_(std::move(frame)) {
    DCHECK_EQ(frame_->format(), media::PIXEL_FORMAT_NV12);
    DCHECK_EQ(frame_->visible_rect().size(), frame_->natural_size());
  }

  int width() const override { return frame_->visible_rect().width(); }
  int height() const override { return frame_->visible_rect().height(); }

  const uint8_t* DataY() const override {
    return frame_->visible_data(media::VideoFrame::kYPlane);
  }

  const uint8_t* DataUV() const override {
    return frame_->visible_data(media::VideoFrame::kUVPlane);
  }

  int StrideY() const override {
    return frame_->stride(media::VideoFrame::kYPlane);
  }

  int StrideUV() const override {
    return frame_->stride(media::VideoFrame::kUVPlane);
  }

  rtc::scoped_refptr<webrtc::I420BufferInterface> ToI420() override {
    rtc::scoped_refptr<webrtc::I420Buffer> i420_buffer;
    i420_buffer = webrtc::I420Buffer::Create(width(), height());
    libyuv::NV12ToI420(DataY(), StrideY(), DataUV(), StrideUV(),
                       i420_buffer->MutableDataY(), i420_buffer->StrideY(),
                       i420_buffer->MutableDataU(), i420_buffer->StrideU(),
                       i420_buffer->MutableDataV(), i420_buffer->StrideV(),
                       width(), height());
    return i420_buffer;
  }

 protected:
  scoped_refptr<media::VideoFrame> frame_;
};

rtc::scoped_refptr<webrtc::VideoFrameBuffer> MakeFrameAdapter(
    scoped_refptr<media::VideoFrame> video_frame) {
  switch (video_frame->format()) {
    case media::PIXEL_FORMAT_I420:
      return new rtc::RefCountedObject<I420FrameAdapter>(
          std::move(video_frame));
    case media::PIXEL_FORMAT_I420A:
      return new rtc::RefCountedObject<I420AFrameAdapter>(
          std::move(video_frame));
    case media::PIXEL_FORMAT_NV12:
      return new rtc::RefCountedObject<NV12FrameAdapter>(
          std::move(video_frame));
    default:
      NOTREACHED();
      return nullptr;
  }
}

scoped_refptr<media::VideoFrame> MakeScaledI420VideoFrame(
    scoped_refptr<media::VideoFrame> source_frame,
    scoped_refptr<blink::WebRtcVideoFrameAdapter::SharedResources>
        shared_resources) {
  // ARGB pixel format may be produced by readback of texture backed frames.
  DCHECK(source_frame->format() == media::PIXEL_FORMAT_NV12 ||
         source_frame->format() == media::PIXEL_FORMAT_I420 ||
         source_frame->format() == media::PIXEL_FORMAT_I420A ||
         source_frame->format() == media::PIXEL_FORMAT_ARGB);
  const bool has_alpha = source_frame->format() == media::PIXEL_FORMAT_I420A;
  // Convert to I420 and scale to the natural size specified in
  // |source_frame|.
  auto dst_frame = shared_resources->CreateFrame(
      has_alpha ? media::PIXEL_FORMAT_I420A : media::PIXEL_FORMAT_I420,
      source_frame->natural_size(), gfx::Rect(source_frame->natural_size()),
      source_frame->natural_size(), source_frame->timestamp());
  if (!dst_frame) {
    LOG(ERROR) << "Failed to create I420 frame from pool.";
    return nullptr;
  }
  dst_frame->metadata().MergeMetadataFrom(source_frame->metadata());

  switch (source_frame->format()) {
    case media::PIXEL_FORMAT_I420A:
      libyuv::ScalePlane(source_frame->visible_data(media::VideoFrame::kAPlane),
                         source_frame->stride(media::VideoFrame::kAPlane),
                         source_frame->visible_rect().width(),
                         source_frame->visible_rect().height(),
                         dst_frame->data(media::VideoFrame::kAPlane),
                         dst_frame->stride(media::VideoFrame::kAPlane),
                         dst_frame->coded_size().width(),
                         dst_frame->coded_size().height(),
                         libyuv::kFilterBilinear);
      // Fallthrough to I420 in order to scale the YUV planes as well.
      ABSL_FALLTHROUGH_INTENDED;
    case media::PIXEL_FORMAT_I420:
      libyuv::I420Scale(source_frame->visible_data(media::VideoFrame::kYPlane),
                        source_frame->stride(media::VideoFrame::kYPlane),
                        source_frame->visible_data(media::VideoFrame::kUPlane),
                        source_frame->stride(media::VideoFrame::kUPlane),
                        source_frame->visible_data(media::VideoFrame::kVPlane),
                        source_frame->stride(media::VideoFrame::kVPlane),
                        source_frame->visible_rect().width(),
                        source_frame->visible_rect().height(),
                        dst_frame->data(media::VideoFrame::kYPlane),
                        dst_frame->stride(media::VideoFrame::kYPlane),
                        dst_frame->data(media::VideoFrame::kUPlane),
                        dst_frame->stride(media::VideoFrame::kUPlane),
                        dst_frame->data(media::VideoFrame::kVPlane),
                        dst_frame->stride(media::VideoFrame::kVPlane),
                        dst_frame->coded_size().width(),
                        dst_frame->coded_size().height(),
                        libyuv::kFilterBilinear);
      break;
    case media::PIXEL_FORMAT_NV12: {
      webrtc::NV12ToI420Scaler scaler;
      scaler.NV12ToI420Scale(
          source_frame->visible_data(media::VideoFrame::kYPlane),
          source_frame->stride(media::VideoFrame::kYPlane),
          source_frame->visible_data(media::VideoFrame::kUVPlane),
          source_frame->stride(media::VideoFrame::kUVPlane),
          source_frame->visible_rect().width(),
          source_frame->visible_rect().height(),
          dst_frame->data(media::VideoFrame::kYPlane),
          dst_frame->stride(media::VideoFrame::kYPlane),
          dst_frame->data(media::VideoFrame::kUPlane),
          dst_frame->stride(media::VideoFrame::kUPlane),
          dst_frame->data(media::VideoFrame::kVPlane),
          dst_frame->stride(media::VideoFrame::kVPlane),
          dst_frame->coded_size().width(), dst_frame->coded_size().height());
    } break;
    case media::PIXEL_FORMAT_ARGB: {
      auto visible_size = source_frame->visible_rect().size();
      if (visible_size == dst_frame->coded_size()) {
        // Direct conversion to dst_frame with no scaling.
        libyuv::ARGBToI420(
            source_frame->visible_data(media::VideoFrame::kARGBPlane),
            source_frame->stride(media::VideoFrame::kARGBPlane),
            dst_frame->data(media::VideoFrame::kYPlane),
            dst_frame->stride(media::VideoFrame::kYPlane),
            dst_frame->data(media::VideoFrame::kUPlane),
            dst_frame->stride(media::VideoFrame::kUPlane),
            dst_frame->data(media::VideoFrame::kVPlane),
            dst_frame->stride(media::VideoFrame::kVPlane), visible_size.width(),
            visible_size.height());
      } else {
        // Convert to I420 tmp image and then scale to the dst_frame.
        auto tmp_frame = shared_resources->CreateTemporaryFrame(
            media::PIXEL_FORMAT_I420, visible_size, gfx::Rect(visible_size),
            visible_size, source_frame->timestamp());
        libyuv::ARGBToI420(
            source_frame->visible_data(media::VideoFrame::kARGBPlane),
            source_frame->stride(media::VideoFrame::kARGBPlane),
            tmp_frame->data(media::VideoFrame::kYPlane),
            tmp_frame->stride(media::VideoFrame::kYPlane),
            tmp_frame->data(media::VideoFrame::kUPlane),
            tmp_frame->stride(media::VideoFrame::kUPlane),
            tmp_frame->data(media::VideoFrame::kVPlane),
            tmp_frame->stride(media::VideoFrame::kVPlane), visible_size.width(),
            visible_size.height());
        libyuv::I420Scale(
            tmp_frame->data(media::VideoFrame::kYPlane),
            tmp_frame->stride(media::VideoFrame::kYPlane),
            tmp_frame->data(media::VideoFrame::kUPlane),
            tmp_frame->stride(media::VideoFrame::kUPlane),
            tmp_frame->data(media::VideoFrame::kVPlane),
            tmp_frame->stride(media::VideoFrame::kVPlane), visible_size.width(),
            visible_size.height(), dst_frame->data(media::VideoFrame::kYPlane),
            dst_frame->stride(media::VideoFrame::kYPlane),
            dst_frame->data(media::VideoFrame::kUPlane),
            dst_frame->stride(media::VideoFrame::kUPlane),
            dst_frame->data(media::VideoFrame::kVPlane),
            dst_frame->stride(media::VideoFrame::kVPlane),
            dst_frame->coded_size().width(), dst_frame->coded_size().height(),
            libyuv::kFilterBilinear);
      }
    } break;
    default:
      NOTREACHED();
  }
  return dst_frame;
}

scoped_refptr<media::VideoFrame> MakeScaledNV12VideoFrame(
    scoped_refptr<media::VideoFrame> source_frame,
    scoped_refptr<blink::WebRtcVideoFrameAdapter::SharedResources>
        shared_resources) {
  DCHECK_EQ(source_frame->format(), media::PIXEL_FORMAT_NV12);
  auto dst_frame = shared_resources->CreateFrame(
      media::PIXEL_FORMAT_NV12, source_frame->natural_size(),
      gfx::Rect(source_frame->natural_size()), source_frame->natural_size(),
      source_frame->timestamp());
  dst_frame->metadata().MergeMetadataFrom(source_frame->metadata());
  const auto& nv12_planes = dst_frame->layout().planes();
  libyuv::NV12Scale(source_frame->visible_data(media::VideoFrame::kYPlane),
                    source_frame->stride(media::VideoFrame::kYPlane),
                    source_frame->visible_data(media::VideoFrame::kUVPlane),
                    source_frame->stride(media::VideoFrame::kUVPlane),
                    source_frame->visible_rect().width(),
                    source_frame->visible_rect().height(),
                    dst_frame->data(media::VideoFrame::kYPlane),
                    nv12_planes[media::VideoFrame::kYPlane].stride,
                    dst_frame->data(media::VideoFrame::kUVPlane),
                    nv12_planes[media::VideoFrame::kUVPlane].stride,
                    dst_frame->coded_size().width(),
                    dst_frame->coded_size().height(), libyuv::kFilterBox);
  return dst_frame;
}

scoped_refptr<media::VideoFrame> MaybeConvertAndScaleFrame(
    scoped_refptr<media::VideoFrame> source_frame,
    scoped_refptr<blink::WebRtcVideoFrameAdapter::SharedResources>
        shared_resources) {
  if (!source_frame)
    return nullptr;
  // Texture frames may be readback in ARGB format.
  RTC_DCHECK(source_frame->format() == media::PIXEL_FORMAT_I420 ||
             source_frame->format() == media::PIXEL_FORMAT_I420A ||
             source_frame->format() == media::PIXEL_FORMAT_NV12 ||
             source_frame->format() == media::PIXEL_FORMAT_ARGB);
  RTC_DCHECK(shared_resources);

  const bool allow_nv12_output =
      base::FeatureList::IsEnabled(blink::features::kWebRtcLibvpxEncodeNV12);
  const bool source_is_i420 =
      source_frame->format() == media::PIXEL_FORMAT_I420 ||
      source_frame->format() == media::PIXEL_FORMAT_I420A;
  const bool source_is_nv12 =
      source_frame->format() == media::PIXEL_FORMAT_NV12;
  const bool no_scaling_needed =
      source_frame->natural_size() == source_frame->visible_rect().size();

  if (((source_is_nv12 && allow_nv12_output) || source_is_i420) &&
      no_scaling_needed) {
    // |source_frame| already has correct pixel format and resolution.
    return source_frame;
  } else if (source_is_nv12 && allow_nv12_output) {
    // Output NV12 only if it is allowed and no conversion is needed.
    return MakeScaledNV12VideoFrame(std::move(source_frame),
                                    std::move(shared_resources));
  } else {
    return MakeScaledI420VideoFrame(std::move(source_frame),
                                    std::move(shared_resources));
  }
}

static void CreateContextProviderOnMainThread(
    scoped_refptr<viz::RasterContextProvider>* result,
    base::WaitableEvent* waitable_event) {
  *result = blink::Platform::Current()->SharedCompositorWorkerContextProvider();
  waitable_event->Signal();
}

}  // anonymous namespace

namespace blink {

scoped_refptr<media::VideoFrame>
WebRtcVideoFrameAdapter::SharedResources::CreateFrame(
    media::VideoPixelFormat format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    base::TimeDelta timestamp) {
  return pool_.CreateFrame(format, coded_size, visible_rect, natural_size,
                           timestamp);
}

scoped_refptr<media::VideoFrame>
WebRtcVideoFrameAdapter::SharedResources::CreateTemporaryFrame(
    media::VideoPixelFormat format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    base::TimeDelta timestamp) {
  return pool_for_tmp_frames_.CreateFrame(format, coded_size, visible_rect,
                                          natural_size, timestamp);
}

scoped_refptr<viz::RasterContextProvider>
WebRtcVideoFrameAdapter::SharedResources::GetRasterContextProvider() {
  base::AutoLock auto_lock(context_provider_lock_);
  if (raster_context_provider_) {
    // Reuse created context provider if it's alive.
    viz::RasterContextProvider::ScopedRasterContextLock lock(
        raster_context_provider_.get());
    if (lock.RasterInterface()->GetGraphicsResetStatusKHR() == GL_NO_ERROR)
      return raster_context_provider_;
  }

  // Recreate the context provider.
  base::WaitableEvent waitable_event;
  PostCrossThreadTask(
      *Thread::MainThread()->GetTaskRunner(), FROM_HERE,
      CrossThreadBindOnce(&CreateContextProviderOnMainThread,
                          CrossThreadUnretained(&raster_context_provider_),
                          CrossThreadUnretained(&waitable_event)));

  // This wait is necessary because this task is completed via main thread
  // asynchronously but WebRTC API is synchronous.
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
  waitable_event.Wait();

  return raster_context_provider_;
}

scoped_refptr<media::VideoFrame>
WebRtcVideoFrameAdapter::SharedResources::ConstructVideoFrameFromTexture(
    scoped_refptr<media::VideoFrame> source_frame) {
  RTC_DCHECK(source_frame->HasTextures());

  scoped_refptr<viz::RasterContextProvider> raster_context_provider =
      GetRasterContextProvider();
  if (!raster_context_provider) {
    return nullptr;
  }
  viz::RasterContextProvider::ScopedRasterContextLock scoped_context(
      raster_context_provider.get());

  auto* ri = scoped_context.RasterInterface();
  auto* gr_context = raster_context_provider->GrContext();

  if (!ri || !gr_context) {
    return nullptr;
  }

  return media::ReadbackTextureBackedFrameToMemorySync(
      *source_frame, ri, gr_context, &pool_for_mapped_frames_);
}

scoped_refptr<media::VideoFrame>
WebRtcVideoFrameAdapter::SharedResources::ConstructVideoFrameFromGpu(
    scoped_refptr<media::VideoFrame> source_frame) {
  CHECK(source_frame);
  // NV12 is the only supported format.
  DCHECK_EQ(source_frame->format(), media::PIXEL_FORMAT_NV12);
  DCHECK_EQ(source_frame->storage_type(),
            media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER);

  return media::ConvertToMemoryMappedFrame(std::move(source_frame));
}

WebRtcVideoFrameAdapter::SharedResources::SharedResources(
    media::GpuVideoAcceleratorFactories* gpu_factories)
    : gpu_factories_(gpu_factories) {}

WebRtcVideoFrameAdapter::SharedResources::~SharedResources() = default;

WebRtcVideoFrameAdapter::WebRtcVideoFrameAdapter(
    scoped_refptr<media::VideoFrame> frame)
    : WebRtcVideoFrameAdapter(frame, nullptr) {}

WebRtcVideoFrameAdapter::WebRtcVideoFrameAdapter(
    scoped_refptr<media::VideoFrame> frame,
    scoped_refptr<SharedResources> shared_resources)
    : frame_(std::move(frame)), shared_resources_(shared_resources) {}

WebRtcVideoFrameAdapter::~WebRtcVideoFrameAdapter() {}

webrtc::VideoFrameBuffer::Type WebRtcVideoFrameAdapter::type() const {
  return Type::kNative;
}

int WebRtcVideoFrameAdapter::width() const {
  return frame_->natural_size().width();
}

int WebRtcVideoFrameAdapter::height() const {
  return frame_->natural_size().height();
}

// static
bool WebRtcVideoFrameAdapter::IsFrameAdaptable(const media::VideoFrame* frame) {
  // Currently accept I420, I420A, NV12 formats in a mapped frame,
  // or a texture or GPU memory buffer frame.
  return (frame->IsMappable() &&
          base::Contains(AdaptableMappablePixelFormats(), frame->format())) ||
         frame->storage_type() ==
             media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER ||
         frame->HasTextures();
}

// static
const base::span<const media::VideoPixelFormat>
WebRtcVideoFrameAdapter::AdaptableMappablePixelFormats() {
  static constexpr const media::VideoPixelFormat
      kAdaptableMappablePixelFormats[] = {media::PIXEL_FORMAT_I420,
                                          media::PIXEL_FORMAT_I420A,
                                          media::PIXEL_FORMAT_NV12};
  return base::make_span(kAdaptableMappablePixelFormats);
}

rtc::scoped_refptr<webrtc::VideoFrameBuffer>
WebRtcVideoFrameAdapter::CreateFrameAdapter() const {
  DCHECK(IsFrameAdaptable(frame_.get()))
      << "Can not create WebRTC frame adapter for frame "
      << frame_->AsHumanReadableString();

  if (frame_->storage_type() ==
      media::VideoFrame::StorageType::STORAGE_GPU_MEMORY_BUFFER) {
    auto video_frame =
        shared_resources_
            ? shared_resources_->ConstructVideoFrameFromGpu(frame_)
            : nullptr;
    video_frame = MaybeConvertAndScaleFrame(video_frame, shared_resources_);
    if (!video_frame) {
      return MakeFrameAdapter(media::VideoFrame::CreateColorFrame(
          frame_->natural_size(), 0u, 0x80, 0x80, frame_->timestamp()));
    }
    return MakeFrameAdapter(std::move(video_frame));
  } else if (frame_->HasTextures()) {
    auto video_frame =
        shared_resources_
            ? shared_resources_->ConstructVideoFrameFromTexture(frame_)
            : nullptr;
    video_frame = MaybeConvertAndScaleFrame(video_frame, shared_resources_);
    if (!video_frame) {
      DLOG(ERROR) << "Texture backed frame cannot be accessed.";
      return MakeFrameAdapter(media::VideoFrame::CreateColorFrame(
          frame_->natural_size(), 0u, 0x80, 0x80, frame_->timestamp()));
    }
    return MakeFrameAdapter(std::move(video_frame));
  }

  // Since scaling is required, hard-apply both the cropping and scaling
  // before we hand the frame over to WebRTC.
  scoped_refptr<media::VideoFrame> scaled_frame =
      MaybeConvertAndScaleFrame(frame_, shared_resources_);
  return MakeFrameAdapter(std::move(scaled_frame));
}

rtc::scoped_refptr<webrtc::I420BufferInterface>
WebRtcVideoFrameAdapter::ToI420() {
  base::AutoLock auto_lock(adapter_lock_);
  if (!frame_adapter_) {
    frame_adapter_ = CreateFrameAdapter();
  }
  return frame_adapter_->ToI420();
}

rtc::scoped_refptr<webrtc::VideoFrameBuffer>
WebRtcVideoFrameAdapter::GetMappedFrameBuffer(
    rtc::ArrayView<webrtc::VideoFrameBuffer::Type> types) {
  base::AutoLock auto_lock(adapter_lock_);
  if (!frame_adapter_) {
    frame_adapter_ = CreateFrameAdapter();
  }
  if (base::Contains(types, frame_adapter_->type())) {
    return frame_adapter_;
  }
  return nullptr;
}

const webrtc::I420BufferInterface* WebRtcVideoFrameAdapter::GetI420() const {
  base::AutoLock auto_lock(adapter_lock_);
  if (!frame_adapter_) {
    frame_adapter_ = CreateFrameAdapter();
  }
  if (frame_adapter_->type() == webrtc::VideoFrameBuffer::Type::kI420) {
    return frame_adapter_->GetI420();
  }
  if (frame_adapter_->type() == webrtc::VideoFrameBuffer::Type::kI420A) {
    return frame_adapter_->GetI420A();
  }
  return nullptr;
}

}  // namespace blink
