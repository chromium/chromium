// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/webrtc/webrtc_video_frame_adapter.h"

#include <cmath>
#include <vector>

#include "base/containers/contains.h"
#include "base/dcheck_is_on.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_restrictions.h"
#include "cc/trees/raster_context_provider_wrapper.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "media/base/video_util.h"
#include "media/base/wait_and_replace_sync_token_client.h"
#include "media/renderers/video_frame_rgba_to_yuva_converter.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_video_frame_pool.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/webrtc/convert_to_webrtc_video_frame_buffer.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/webrtc/rtc_base/ref_counted_object.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

namespace {

bool IsApproxEquals(int a, int b) {
  return std::abs(a - b) <= 4;
}

bool IsApproxEquals(const gfx::Rect& a, const gfx::Rect& b) {
  return IsApproxEquals(a.x(), b.x()) && IsApproxEquals(a.y(), b.y()) &&
         IsApproxEquals(a.width(), b.width()) &&
         IsApproxEquals(a.height(), b.height());
}

static void CreateContextProviderOnMainThread(
    scoped_refptr<viz::RasterContextProvider>* result,
    base::WaitableEvent* waitable_event) {
  scoped_refptr<cc::RasterContextProviderWrapper> worker_context_provider =
      blink::Platform::Current()->SharedCompositorWorkerContextProvider(
          nullptr);
  if (worker_context_provider)
    *result = worker_context_provider->GetContext();
  waitable_event->Signal();
}

class Context : public media::RenderableGpuMemoryBufferVideoFramePool::Context {
 public:
  Context(media::GpuVideoAcceleratorFactories* gpu_factories,
          scoped_refptr<viz::RasterContextProvider> raster_context_provider)
      : gpu_factories_(gpu_factories),
        raster_context_provider_(std::move(raster_context_provider)) {}

  std::unique_ptr<gfx::GpuMemoryBuffer> CreateGpuMemoryBuffer(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage) override {
    return GpuMemoryBufferManager()->CreateGpuMemoryBuffer(
        size, format, usage, gpu::kNullSurfaceHandle, nullptr);
  }

  void CreateSharedImage(gfx::GpuMemoryBuffer* gpu_memory_buffer,
                         const viz::SharedImageFormat& si_format,
                         const gfx::ColorSpace& color_space,
                         GrSurfaceOrigin surface_origin,
                         SkAlphaType alpha_type,
                         uint32_t usage,
                         gpu::Mailbox& mailbox,
                         gpu::SyncToken& sync_token) override {
    auto* sii = SharedImageInterface();
    if (!sii) {
      return;
    }
    mailbox = sii->CreateSharedImage(si_format, gpu_memory_buffer->GetSize(),
                                     color_space, surface_origin, alpha_type,
                                     usage, "WebRTCVideoFramePool",
                                     gpu_memory_buffer->CloneHandle());
    sync_token = sii->GenVerifiedSyncToken();
  }

  void CreateSharedImage(gfx::GpuMemoryBuffer* gpu_memory_buffer,
                         gfx::BufferPlane plane,
                         const gfx::ColorSpace& color_space,
                         GrSurfaceOrigin surface_origin,
                         SkAlphaType alpha_type,
                         uint32_t usage,
                         gpu::Mailbox& mailbox,
                         gpu::SyncToken& sync_token) override {
    auto* sii = SharedImageInterface();
    if (!sii)
      return;
    mailbox = sii->CreateSharedImage(
        gpu_memory_buffer, GpuMemoryBufferManager(), plane, color_space,
        surface_origin, alpha_type, usage, "WebRTCVideoFramePool");
    sync_token = sii->GenVerifiedSyncToken();
  }

  void DestroySharedImage(const gpu::SyncToken& sync_token,
                          const gpu::Mailbox& mailbox) override {
    auto* sii = SharedImageInterface();
    if (!sii)
      return;
    sii->DestroySharedImage(sync_token, mailbox);
  }

 private:
  gpu::SharedImageInterface* SharedImageInterface() const {
    return raster_context_provider_->SharedImageInterface();
  }

  gpu::GpuMemoryBufferManager* GpuMemoryBufferManager() const {
    auto* manager = gpu_factories_->GpuMemoryBufferManager();
    DCHECK(manager);
    return manager;
  }

  raw_ptr<media::GpuVideoAcceleratorFactories, ExperimentalRenderer>
      gpu_factories_;
  scoped_refptr<viz::RasterContextProvider> raster_context_provider_;
};

}  // namespace

WebRtcVideoFrameAdapter::VectorBufferPool::VectorBufferPool()
    : tick_clock_(base::DefaultTickClock::GetInstance()) {}

std::unique_ptr<std::vector<uint8_t>>
WebRtcVideoFrameAdapter::VectorBufferPool::Allocate() {
  base::AutoLock autolock(buffer_lock_);
  if (!free_buffers_.empty()) {
    auto buffer = std::move(free_buffers_.back().buffer);
    free_buffers_.pop_back();
    return buffer;
  }

  return std::make_unique<std::vector<uint8_t>>();
}

void WebRtcVideoFrameAdapter::VectorBufferPool::Return(
    std::unique_ptr<std::vector<uint8_t>> buffer) {
  base::AutoLock autolock(buffer_lock_);
  const base::TimeTicks now = tick_clock_->NowTicks();
  free_buffers_.push_back(BufferEntry{now, std::move(buffer)});

  // After this loop, |stale_index| is pointing to the first non-stale buffer.
  // Such an index must exist because |buffer| is never stale.
  constexpr base::TimeDelta kStaleBufferLimit = base::Seconds(10);
  for (wtf_size_t stale_index = 0; stale_index < free_buffers_.size();
       ++stale_index) {
    if (now - free_buffers_[stale_index].last_use_time < kStaleBufferLimit) {
      DCHECK_LT(stale_index, free_buffers_.size());
      if (stale_index > 0) {
        free_buffers_.erase(free_buffers_.begin(),
                            free_buffers_.begin() + stale_index);
      }
      break;
    }
  }
}

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

std::unique_ptr<std::vector<uint8_t>>
WebRtcVideoFrameAdapter::SharedResources::CreateTemporaryVectorBuffer() {
  return pool_for_tmp_vectors_.Allocate();
}

void WebRtcVideoFrameAdapter::SharedResources::ReleaseTemporaryVectorBuffer(
    std::unique_ptr<std::vector<uint8_t>> buffer) {
  pool_for_tmp_vectors_.Return(std::move(buffer));
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

  // Since the accelerated frame pool is attached to the old provider, we need
  // to release it here.
  accelerated_frame_pool_.reset();

  // Recreate the context provider.
  base::WaitableEvent waitable_event;
  PostCrossThreadTask(
      *Thread::MainThread()->GetTaskRunner(MainThreadTaskRunnerRestricted()),
      FROM_HERE,
      CrossThreadBindOnce(&CreateContextProviderOnMainThread,
                          CrossThreadUnretained(&raster_context_provider_),
                          CrossThreadUnretained(&waitable_event)));

  // This wait is necessary because this task is completed via main thread
  // asynchronously but WebRTC API is synchronous.
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
  waitable_event.Wait();

  return raster_context_provider_;
}

bool CanUseGpuMemoryBufferReadback(
    media::VideoPixelFormat format,
    media::GpuVideoAcceleratorFactories* gpu_factories) {
  // Since ConvertToWebRtcVideoFrameBuffer will always produce an opaque frame
  // (unless the input is already I420A), we allow using GMB readback from
  // ABGR/ARGB to NV12.
  return gpu_factories &&
         (format == media::PIXEL_FORMAT_XBGR ||
          format == media::PIXEL_FORMAT_XRGB ||
          format == media::PIXEL_FORMAT_ABGR ||
          format == media::PIXEL_FORMAT_ARGB) &&
         WebGraphicsContext3DVideoFramePool::
             IsGpuMemoryBufferReadbackFromTextureEnabled();
}

scoped_refptr<media::VideoFrame>
WebRtcVideoFrameAdapter::SharedResources::ConstructVideoFrameFromTexture(
    scoped_refptr<media::VideoFrame> source_frame) {
  RTC_DCHECK(source_frame->HasTextures());

  auto raster_context_provider = GetRasterContextProvider();
  if (!raster_context_provider) {
    return nullptr;
  }

  viz::RasterContextProvider::ScopedRasterContextLock scoped_context(
      raster_context_provider.get());

  if (!disable_gmb_frames_ &&
      CanUseGpuMemoryBufferReadback(source_frame->format(), gpu_factories_)) {
    if (!accelerated_frame_pool_) {
      accelerated_frame_pool_ =
          media::RenderableGpuMemoryBufferVideoFramePool::Create(
              std::make_unique<Context>(gpu_factories_,
                                        raster_context_provider));
    }

    auto origin = source_frame->metadata().texture_origin_is_top_left
                      ? kTopLeft_GrSurfaceOrigin
                      : kBottomLeft_GrSurfaceOrigin;

    // TODO(crbug.com/1224279): This assumes that all frames are 8-bit sRGB.
    // Expose the color space and pixel format that is backing
    // `image->GetMailboxHolder()`, or, alternatively, expose an accelerated
    // SkImage.
    auto format = (source_frame->format() == media::PIXEL_FORMAT_XBGR ||
                   source_frame->format() == media::PIXEL_FORMAT_ABGR)
                      ? viz::SinglePlaneFormat::kRGBA_8888
                      : viz::SinglePlaneFormat::kBGRA_8888;

    scoped_refptr<media::VideoFrame> dst_frame;
    {
      // Blocking is necessary to create the GpuMemoryBuffer from this thread.
      base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
      dst_frame = accelerated_frame_pool_->MaybeCreateVideoFrame(
          source_frame->coded_size(), gfx::ColorSpace::CreateREC709());
    }

    if (dst_frame) {
      const bool copy_succeeded = media::CopyRGBATextureToVideoFrame(
          raster_context_provider.get(), format, source_frame->coded_size(),
          source_frame->ColorSpace(), origin, source_frame->mailbox_holder(0),
          dst_frame.get());
      if (copy_succeeded) {
        // CopyRGBATextureToVideoFrame() operates on mailboxes and not frames,
        // so we must manually copy over properties relevant to the encoder.
        // TODO(https://crbug.com/1272852): Consider bailing out of this path if
        // visible_rect or natural_size is much smaller than coded_size, or
        // copying only the necessary part.
        if (dst_frame->visible_rect() != source_frame->visible_rect() ||
            dst_frame->natural_size() != source_frame->natural_size()) {
          const auto dst_format = dst_frame->format();
          dst_frame = media::VideoFrame::WrapVideoFrame(
              std::move(dst_frame), dst_format, source_frame->visible_rect(),
              source_frame->natural_size());
          DCHECK(dst_frame);
        }
        dst_frame->set_timestamp(source_frame->timestamp());
        dst_frame->set_metadata(source_frame->metadata());

        // RI::Finish() makes sure that CopyRGBATextureToVideoFrame() finished
        // texture copy before we call ConstructVideoFrameFromGpu(). It's not
        // the best way to wait for completion, but it's the only sync way
        // to wait, and making this function async is currently impractical.
        raster_context_provider->RasterInterface()->Finish();
        auto vf = ConstructVideoFrameFromGpu(std::move(dst_frame));
        return vf;
      }
    }

    DLOG(WARNING) << "Disabling GpuMemoryBuffer based readback due to failure.";
    disable_gmb_frames_ = true;
    accelerated_frame_pool_.reset();
  }

  auto* ri = scoped_context.RasterInterface();
  auto* gr_context = raster_context_provider->GrContext();

  if (!ri) {
    return nullptr;
  }

  return media::ReadbackTextureBackedFrameToMemorySync(
      *source_frame, ri, gr_context,
      raster_context_provider->ContextCapabilities(), &pool_for_mapped_frames_);
}

scoped_refptr<media::VideoFrame>
WebRtcVideoFrameAdapter::SharedResources::ConstructVideoFrameFromGpu(
    scoped_refptr<media::VideoFrame> source_frame) {
  CHECK(source_frame);
  // NV12 is the only supported format.
  DCHECK_EQ(source_frame->format(), media::PIXEL_FORMAT_NV12);
  DCHECK_EQ(source_frame->storage_type(),
            media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER);

  // This is necessary because mapping may require waiting on IO thread,
  // but webrtc API is synchronous.
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;

  return media::ConvertToMemoryMappedFrame(std::move(source_frame));
}

void WebRtcVideoFrameAdapter::SharedResources::SetFeedback(
    const media::VideoCaptureFeedback& feedback) {
  base::AutoLock auto_lock(feedback_lock_);
  last_feedback_ = feedback;
}

media::VideoCaptureFeedback
WebRtcVideoFrameAdapter::SharedResources::GetFeedback() {
  base::AutoLock auto_lock(feedback_lock_);
  return last_feedback_;
}

WebRtcVideoFrameAdapter::SharedResources::SharedResources(
    media::GpuVideoAcceleratorFactories* gpu_factories)
    : gpu_factories_(gpu_factories) {}

WebRtcVideoFrameAdapter::SharedResources::~SharedResources() = default;

WebRtcVideoFrameAdapter::ScaledBufferSize::ScaledBufferSize(
    gfx::Rect visible_rect,
    gfx::Size natural_size)
    : visible_rect(std::move(visible_rect)),
      natural_size(std::move(natural_size)) {}

bool WebRtcVideoFrameAdapter::ScaledBufferSize::operator==(
    const ScaledBufferSize& rhs) const {
  return visible_rect == rhs.visible_rect && natural_size == rhs.natural_size;
}

bool WebRtcVideoFrameAdapter::ScaledBufferSize::operator!=(
    const ScaledBufferSize& rhs) const {
  return !(*this == rhs);
}

WebRtcVideoFrameAdapter::ScaledBufferSize
WebRtcVideoFrameAdapter::ScaledBufferSize::CropAndScale(
    int offset_x,
    int offset_y,
    int crop_width,
    int crop_height,
    int scaled_width,
    int scaled_height) const {
  DCHECK_LT(offset_x, natural_size.width());
  DCHECK_LT(offset_y, natural_size.height());
  DCHECK_LE(offset_x + crop_width, natural_size.width());
  DCHECK_LE(offset_y + crop_height, natural_size.height());
  DCHECK_LE(scaled_width, crop_width);
  DCHECK_LE(scaled_height, crop_height);
  // Used to convert requested visible rect to the natural size, i.e. undo
  // scaling.
  double horizontal_scale =
      static_cast<double>(visible_rect.width()) / natural_size.width();
  double vertical_scale =
      static_cast<double>(visible_rect.height()) / natural_size.height();
  return ScaledBufferSize(
      gfx::Rect(visible_rect.x() + offset_x * horizontal_scale,
                visible_rect.y() + offset_y * vertical_scale,
                crop_width * horizontal_scale, crop_height * vertical_scale),
      gfx::Size(scaled_width, scaled_height));
}

WebRtcVideoFrameAdapter::ScaledBuffer::ScaledBuffer(
    scoped_refptr<WebRtcVideoFrameAdapter> parent,
    ScaledBufferSize size)
    : parent_(std::move(parent)), size_(std::move(size)) {}

rtc::scoped_refptr<webrtc::I420BufferInterface>
WebRtcVideoFrameAdapter::ScaledBuffer::ToI420() {
  return parent_->GetOrCreateFrameBufferForSize(size_)->ToI420();
}

rtc::scoped_refptr<webrtc::VideoFrameBuffer>
WebRtcVideoFrameAdapter::ScaledBuffer::GetMappedFrameBuffer(
    rtc::ArrayView<webrtc::VideoFrameBuffer::Type> types) {
  auto frame_buffer = parent_->GetOrCreateFrameBufferForSize(size_);
  return base::Contains(types, frame_buffer->type()) ? frame_buffer : nullptr;
}

rtc::scoped_refptr<webrtc::VideoFrameBuffer>
WebRtcVideoFrameAdapter::ScaledBuffer::CropAndScale(int offset_x,
                                                    int offset_y,
                                                    int crop_width,
                                                    int crop_height,
                                                    int scaled_width,
                                                    int scaled_height) {
  return rtc::scoped_refptr<webrtc::VideoFrameBuffer>(
      new rtc::RefCountedObject<ScaledBuffer>(
          parent_,
          size_.CropAndScale(offset_x, offset_y, crop_width, crop_height,
                             scaled_width, scaled_height)));
}

WebRtcVideoFrameAdapter::WebRtcVideoFrameAdapter(
    scoped_refptr<media::VideoFrame> frame)
    : WebRtcVideoFrameAdapter(std::move(frame), {}, nullptr) {}

WebRtcVideoFrameAdapter::WebRtcVideoFrameAdapter(
    scoped_refptr<media::VideoFrame> frame,
    std::vector<scoped_refptr<media::VideoFrame>> scaled_frames,
    scoped_refptr<SharedResources> shared_resources)
    : frame_(std::move(frame)),
      scaled_frames_(std::move(scaled_frames)),
      shared_resources_(std::move(shared_resources)),
      full_size_(frame_->visible_rect(), frame_->natural_size()) {
#if DCHECK_IS_ON()
  double frame_aspect_ratio =
      static_cast<double>(frame_->coded_size().width()) /
      frame_->coded_size().height();
  for (const auto& scaled_frame : scaled_frames_) {
    DCHECK_LT(scaled_frame->coded_size().width(), frame_->coded_size().width());
    DCHECK_LT(scaled_frame->coded_size().height(),
              frame_->coded_size().height());
    double scaled_frame_aspect_ratio =
        static_cast<double>(scaled_frame->coded_size().width()) /
        scaled_frame->coded_size().height();
    DCHECK_LE(std::abs(scaled_frame_aspect_ratio - frame_aspect_ratio), 0.05);
  }
#endif
}

WebRtcVideoFrameAdapter::~WebRtcVideoFrameAdapter() {
  // Mapping is always required when WebRTC uses software encoding.  If hardware
  // encoding is used, we may not always need to do mapping; however, if scaling
  // is needed we may do mapping and downscaling here anyway.  Therefore, notify
  // the capturer that premapped frames are required.
  if (shared_resources_) {
    shared_resources_->SetFeedback(
        media::VideoCaptureFeedback().RequireMapped(!adapted_frames_.empty()));
  }
}

rtc::scoped_refptr<webrtc::I420BufferInterface>
WebRtcVideoFrameAdapter::ToI420() {
  return GetOrCreateFrameBufferForSize(full_size_)->ToI420();
}

rtc::scoped_refptr<webrtc::VideoFrameBuffer>
WebRtcVideoFrameAdapter::GetMappedFrameBuffer(
    rtc::ArrayView<webrtc::VideoFrameBuffer::Type> types) {
  auto frame_buffer = GetOrCreateFrameBufferForSize(full_size_);
  return base::Contains(types, frame_buffer->type()) ? frame_buffer : nullptr;
}

// Soft-applies cropping and scaling. The result is a ScaledBuffer.
rtc::scoped_refptr<webrtc::VideoFrameBuffer>
WebRtcVideoFrameAdapter::CropAndScale(int offset_x,
                                      int offset_y,
                                      int crop_width,
                                      int crop_height,
                                      int scaled_width,
                                      int scaled_height) {
  return rtc::scoped_refptr<webrtc::VideoFrameBuffer>(
      new rtc::RefCountedObject<ScaledBuffer>(
          this,
          full_size_.CropAndScale(offset_x, offset_y, crop_width, crop_height,
                                  scaled_width, scaled_height)));
}

rtc::scoped_refptr<webrtc::VideoFrameBuffer>
WebRtcVideoFrameAdapter::GetOrCreateFrameBufferForSize(
    const ScaledBufferSize& size) {
  base::AutoLock auto_lock(adapted_frames_lock_);
  // Does this buffer already exist?
  for (const auto& adapted_frame : adapted_frames_) {
    if (adapted_frame.size == size)
      return adapted_frame.frame_buffer;
  }
  // Adapt the frame for this size.
  adapted_frames_.push_back(AdaptBestFrame(size));
  return adapted_frames_.back().frame_buffer;
}

WebRtcVideoFrameAdapter::AdaptedFrame WebRtcVideoFrameAdapter::AdaptBestFrame(
    const ScaledBufferSize& size) const {
  double requested_scale_factor =
      static_cast<double>(size.natural_size.width()) /
      size.visible_rect.width();
  // Ideally we have a frame that is in the same scale as |size|. Otherwise, the
  // best frame is the smallest frame that is greater than |size|.
  //
  // Search for the "best frame" amongst media::VideoFrames (pre-scaled frames).
  // The "best frame" can either be a media::VideoFrame (a pre-scaled frame) or
  // a webrtc::VideoFrameBuffer (a previously hard-applied frame).
  scoped_refptr<media::VideoFrame> best_media_frame = frame_;
  double best_frame_scale_factor = 1.0;
  for (const auto& scaled_frame : scaled_frames_) {
    double scale_factor =
        static_cast<double>(scaled_frame->coded_size().width()) /
        frame_->coded_size().width();
    if (scale_factor >= requested_scale_factor &&
        scale_factor < best_frame_scale_factor) {
      best_media_frame = scaled_frame;
      best_frame_scale_factor = scale_factor;
      if (scale_factor == requested_scale_factor) {
        break;
      }
    }
  }
  if (best_frame_scale_factor != requested_scale_factor) {
    // Scaling is needed. Consider if the "best frame" is in fact a previously
    // adapted frame. Search amongst webrtc::VideoFrameBuffers (previously
    // hard-applied frames).
    rtc::scoped_refptr<webrtc::VideoFrameBuffer> best_webrtc_frame;
    for (const auto& adapted_frame : adapted_frames_) {
      // For simplicity, ignore frames where the cropping is not identical to a
      // previous mapping.
      if (size.visible_rect != adapted_frame.size.visible_rect) {
        continue;
      }
      double scale_factor =
          static_cast<double>(adapted_frame.size.natural_size.width()) /
          adapted_frame.size.visible_rect.width();
      if (scale_factor >= requested_scale_factor &&
          scale_factor < best_frame_scale_factor) {
        best_webrtc_frame = adapted_frame.frame_buffer;
        best_frame_scale_factor = scale_factor;
      }
    }
    if (best_webrtc_frame) {
      rtc::scoped_refptr<webrtc::VideoFrameBuffer> adapted_webrtc_frame =
          best_webrtc_frame->Scale(size.natural_size.width(),
                                   size.natural_size.height());
      return AdaptedFrame(size, nullptr, adapted_webrtc_frame);
    }
  }
  // Because |size| is expressed relative to the full size'd frame, we need to
  // adjust the visible rect for the scale of the best frame.
  gfx::Rect visible_rect(size.visible_rect.x() * best_frame_scale_factor,
                         size.visible_rect.y() * best_frame_scale_factor,
                         size.visible_rect.width() * best_frame_scale_factor,
                         size.visible_rect.height() * best_frame_scale_factor);
  if (IsApproxEquals(visible_rect, best_media_frame->visible_rect())) {
    // Due to rounding errors it is possible for |visible_rect| to be slightly
    // off, which could either cause unnecessary cropping/scaling or cause
    // crashes if |visible_rect| is not contained within
    // |best_media_frame->visible_rect()|, so we adjust it.
    visible_rect = best_media_frame->visible_rect();
  }
  CHECK(best_media_frame->visible_rect().Contains(visible_rect))
      << visible_rect.ToString() << " is not contained within "
      << best_media_frame->visible_rect().ToString();
  // Wrapping is only needed if we need to crop or scale the best frame.
  if (best_media_frame->visible_rect() != visible_rect ||
      best_media_frame->natural_size() != size.natural_size) {
    best_media_frame = media::VideoFrame::WrapVideoFrame(
        best_media_frame, best_media_frame->format(), visible_rect,
        size.natural_size);
  }
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> adapted_webrtc_frame =
      ConvertToWebRtcVideoFrameBuffer(best_media_frame, shared_resources_);
  return AdaptedFrame(size, best_media_frame, adapted_webrtc_frame);
}

scoped_refptr<media::VideoFrame>
WebRtcVideoFrameAdapter::GetAdaptedVideoBufferForTesting(
    const ScaledBufferSize& size) {
  base::AutoLock auto_lock(adapted_frames_lock_);
  for (const auto& adapted_frame : adapted_frames_) {
    if (adapted_frame.size == size)
      return adapted_frame.video_frame;
  }
  return nullptr;
}

}  // namespace blink
