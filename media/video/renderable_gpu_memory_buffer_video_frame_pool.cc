// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/renderable_gpu_memory_buffer_video_frame_pool.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <list>
#include <utility>

#include "base/bits.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "cc/base/math_util.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "media/base/format_utils.h"
#include "media/base/media_switches.h"
#include "media/base/video_frame.h"

namespace media {

namespace {

// Allow MappableSI to be used for RenderableGpuMemoryBufferVideoFramePool.
BASE_FEATURE(kUseMappableSIForRenderableGpuMemoryBufferVideoFramePool,
             "UseMappableSIForRenderableGpuMemoryBufferVideoFramePool",
             base::FEATURE_ENABLED_BY_DEFAULT);

class InternalRefCountedPool;

// The VideoFrame-backing resources that are reused by the pool, namely, a
// GpuMemoryBuffer and a SharedImage. This retains a reference to the
// InternalRefCountedPool that created it. Not safe for concurrent use.
class FrameResources {
 public:
  FrameResources(scoped_refptr<InternalRefCountedPool> pool,
                 VideoPixelFormat format,
                 const gfx::Size& coded_size,
                 const gfx::ColorSpace& color_space);
  ~FrameResources();
  FrameResources(const FrameResources& other) = delete;
  FrameResources& operator=(const FrameResources& other) = delete;

  // Allocate GpuMemoryBuffer and create SharedImage. Returns false on failure
  // to do so.
  bool Initialize();

  // Return true if these resources can be reused for a frame with the specified
  // parameters.
  bool IsCompatibleWith(const gfx::Size& coded_size,
                        const gfx::ColorSpace& color_space) const;

  // Create a VideoFrame using these resources. This will transfer ownership of
  // the GpuMemoryBuffer to the VideoFrame.
  scoped_refptr<VideoFrame> CreateVideoFrameAndTakeGpuMemoryBuffer();

  // Return ownership of the GpuMemory to `this` and updates `sync_token_` to
  // `sync_token`.
  void ReturnGpuMemoryBufferFromFrame(
      std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer,
      const gpu::SyncToken& sync_token);

 private:
  // This reference ensures that the creating InternalRefCountedPool (and,
  // critically, its interface through which `this` can destroy its
  // SharedImage) will not be destroyed until after `this` is destroyed.
  const scoped_refptr<InternalRefCountedPool> pool_;

  const VideoPixelFormat format_;
  const gfx::Size coded_size_;
  const gfx::ColorSpace color_space_;
  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer_;
  scoped_refptr<gpu::ClientSharedImage> shared_image_;
  gpu::SyncToken sync_token_;
};

// The owner of the RenderableGpuMemoryBufferVideoFramePool::Client needs to be
// reference counted to ensure that not be destroyed while there still exist any
// FrameResources.
// Although this class is not generally safe for concurrent use, it extends
// RefCountedThreadSafe in order to allow destruction on a different thread.
// Specifically, blink::WebRtcVideoFrameAdapter::SharedResources lazily creates
// a RenderableGpuMemoryBufferVideoFramePool when it needs to convert a frame on
// the IO thread, but ends up destroying the object on the main thread.
class InternalRefCountedPool
    : public base::RefCountedThreadSafe<InternalRefCountedPool> {
 public:
  explicit InternalRefCountedPool(
      std::unique_ptr<RenderableGpuMemoryBufferVideoFramePool::Context> context,
      VideoPixelFormat format);

  // Create a VideoFrame with the specified parameters, reusing the resources
  // of a previous frame, if possible.
  scoped_refptr<VideoFrame> MaybeCreateVideoFrame(
      const gfx::Size& coded_size,
      const gfx::ColorSpace& color_space);

  // Indicate that the owner of `this` is being destroyed. This will eventually
  // cause `this` to be destroyed (once all of the FrameResources it created are
  // destroyed, which will happen only once all of the VideoFrames it created
  // are destroyed).
  void Shutdown();

  // Return the Context for accessing the GpuMemoryBufferManager and
  // SharedImageInterface. Never returns nullptr.
  RenderableGpuMemoryBufferVideoFramePool::Context* GetContext() const;

 private:
  friend class base::RefCountedThreadSafe<InternalRefCountedPool>;
  ~InternalRefCountedPool();

  // Callback made when a created VideoFrame is destroyed. Returns
  // `gpu_memory_buffer` to `frame_resources`, and then either returns
  // `frame_resources` to `available_frame_resources_` or destroys it.
  void OnVideoFrameDestroyed(
      std::unique_ptr<FrameResources> frame_resources,
      const gpu::SyncToken& sync_token,
      std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer);

  const VideoPixelFormat format_;
  const std::unique_ptr<RenderableGpuMemoryBufferVideoFramePool::Context>
      context_;
  std::list<std::unique_ptr<FrameResources>> available_frame_resources_;
  bool shutting_down_ = false;
};

// Implementation of the RenderableGpuMemoryBufferVideoFramePool abstract
// class.
class RenderableGpuMemoryBufferVideoFramePoolImpl
    : public RenderableGpuMemoryBufferVideoFramePool {
 public:
  explicit RenderableGpuMemoryBufferVideoFramePoolImpl(
      std::unique_ptr<RenderableGpuMemoryBufferVideoFramePool::Context> context,
      VideoPixelFormat format,
      const bool is_mappable_si_enabled);

  scoped_refptr<VideoFrame> MaybeCreateVideoFrame(
      const gfx::Size& coded_size,
      const gfx::ColorSpace& color_space) override;

  bool IsMappableSIEnabledForTesting() const override;  // IN-TEST

  ~RenderableGpuMemoryBufferVideoFramePoolImpl() override;

  const VideoPixelFormat format_;
  const scoped_refptr<InternalRefCountedPool> pool_internal_;
  const bool is_mappable_si_enabled_;
};

////////////////////////////////////////////////////////////////////////////////
// FrameResources

FrameResources::FrameResources(scoped_refptr<InternalRefCountedPool> pool,
                               const VideoPixelFormat format,
                               const gfx::Size& coded_size,
                               const gfx::ColorSpace& color_space)
    : pool_(std::move(pool)),
      format_(format),
      coded_size_(coded_size),
      color_space_(color_space) {
  // Currently only support ARGB, ABGR and NV12.
  CHECK(format == PIXEL_FORMAT_ARGB || format == PIXEL_FORMAT_ABGR ||
        format == PIXEL_FORMAT_NV12);
}

FrameResources::~FrameResources() {
  if (shared_image_) {
    pool_->GetContext()->DestroySharedImage(
        sync_token_, std::move(shared_image_),
        base::FeatureList::IsEnabled(
            kUseMappableSIForRenderableGpuMemoryBufferVideoFramePool));
  }
}

gfx::Size GetBufferSizeInPixelsForVideoPixelFormat(
    VideoPixelFormat format,
    const gfx::Size& coded_size) {
  switch (format) {
    case PIXEL_FORMAT_ARGB:
    case PIXEL_FORMAT_ABGR:
      return coded_size;
    case PIXEL_FORMAT_NV12:
      // Align number of rows to 2, because it's required by YUV_420_BIPLANAR
      // buffer allocation code.
      // Align buffer stride to 4, because our SharedImage shared memory backing
      // code requires it, since it sometimes treats Y-planes are 4 bytes per
      // pixel textures.
      return {cc::MathUtil::CheckedRoundUp(coded_size.width(), 4),
              cc::MathUtil::CheckedRoundUp(coded_size.height(), 2)};
    default:
      NOTREACHED();
  }
}

bool FrameResources::Initialize() {
  auto* context = pool_->GetContext();

  constexpr gfx::BufferUsage kBufferUsage =
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
      gfx::BufferUsage::SCANOUT_VEA_CPU_READ
#else
      gfx::BufferUsage::SCANOUT_CPU_READ_WRITE
#endif
      ;

  const gfx::BufferFormat buffer_format =
      VideoPixelFormatToGfxBufferFormat(format_).value();

  const gfx::Size buffer_size_in_pixels =
      GetBufferSizeInPixelsForVideoPixelFormat(format_, coded_size_);

  // Create the GpuMemoryBuffer if MappableSharedImages is not enabled. When its
  // enabled, clients only create a mappable shared image directly without
  // needing to create a GMB.
  const bool is_mappable_si_enabled = base::FeatureList::IsEnabled(
      kUseMappableSIForRenderableGpuMemoryBufferVideoFramePool);
  if (!is_mappable_si_enabled) {
    gpu_memory_buffer_ = context->CreateGpuMemoryBuffer(
        buffer_size_in_pixels, buffer_format, kBufferUsage);
    if (!gpu_memory_buffer_) {
      LOG(ERROR) << "Failed to allocate GpuMemoryBuffer for frame: coded_size="
                 << coded_size_.ToString()
                 << ", usage=" << static_cast<int>(kBufferUsage);
      return false;
    }

#if BUILDFLAG(IS_MAC)
  gpu_memory_buffer_->SetColorSpace(color_space_);
#endif
  }

  constexpr gpu::SharedImageUsageSet kSharedImageUsage =
#if BUILDFLAG(IS_MAC)
      gpu::SHARED_IMAGE_USAGE_MACOS_VIDEO_TOOLBOX |
#endif
      // These SharedImages, like most/all SharedImages created to back
      // VideoFrames, can be read both via the raster interface for import into
      // canvas and/or 2-copy import into WebGL and via the GLES2 interface for
      // 1-copy import into WebGL.
      // Unusually for such SharedImages, they are also *written* via raster for
      // WebGL and WebRTC use cases in which RGBA textures are imported into the
      // VideoFrames (this is what "renderable" means in this context). Hence,
      // GLES2_WRITE is required for raster-over-GLES.
      gpu::SHARED_IMAGE_USAGE_GLES2_READ | gpu::SHARED_IMAGE_USAGE_GLES2_WRITE |
      gpu::SHARED_IMAGE_USAGE_RASTER_READ |
      gpu::SHARED_IMAGE_USAGE_RASTER_WRITE |
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_SCANOUT;

  CHECK(format_ == PIXEL_FORMAT_NV12 || format_ == PIXEL_FORMAT_ABGR ||
        format_ == PIXEL_FORMAT_ARGB)
      << format_;
  const viz::SharedImageFormat si_format =
      viz::GetSharedImageFormat(buffer_format);

  if (is_mappable_si_enabled) {
    shared_image_ = context->CreateSharedImage(
        buffer_size_in_pixels, kBufferUsage, si_format, color_space_,
        kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, kSharedImageUsage,
        sync_token_);
  } else {
    shared_image_ = context->CreateSharedImage(
        gpu_memory_buffer_.get(), si_format, color_space_,
        kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, kSharedImageUsage,
        sync_token_);
  }
  if (!shared_image_) {
    DLOG(ERROR) << "Failed to allocate shared image for frame: coded_size="
                << coded_size_.ToString()
                << ", si_format=" << si_format.ToString();
    return false;
  }
  return true;
}

bool FrameResources::IsCompatibleWith(
    const gfx::Size& coded_size,
    const gfx::ColorSpace& color_space) const {
  return coded_size_ == coded_size && color_space_ == color_space;
}

scoped_refptr<VideoFrame>
FrameResources::CreateVideoFrameAndTakeGpuMemoryBuffer() {
  const gfx::Rect visible_rect(coded_size_);
  const gfx::Size natural_size = coded_size_;

  CHECK(shared_image_);
  scoped_refptr<VideoFrame> video_frame;
  if (base::FeatureList::IsEnabled(
          kUseMappableSIForRenderableGpuMemoryBufferVideoFramePool)) {
    video_frame = VideoFrame::WrapMappableSharedImage(
        shared_image_, sync_token_,
        VideoFrame::ReleaseMailboxAndGpuMemoryBufferCB(), visible_rect,
        natural_size, base::TimeDelta());
  } else {
    video_frame = VideoFrame::WrapExternalGpuMemoryBuffer(
        visible_rect, natural_size, std::move(gpu_memory_buffer_),
        shared_image_, sync_token_,
        VideoFrame::ReleaseMailboxAndGpuMemoryBufferCB(), base::TimeDelta());
  }
  if (!video_frame) {
    return nullptr;
  }

  video_frame->set_color_space(color_space_);

  // TODO(crbug.com/40174702): This should depend on the platform and
  // format.
  video_frame->metadata().allow_overlay = true;

  // Tag this frame as having used a single SharedImage for multiplanar
  // formats (by default it sets this field to `kLegacy`, which causes the
  // rest of the system to assume that this frame has been created with one
  // SharedImage per plane for multiplanar formats).
  video_frame->set_shared_image_format_type(
      SharedImageFormatType::kSharedImageFormat);

  // Only native (non shared memory) GMBs require waiting on GPU fences.
  const bool has_native_gmb = video_frame->HasNativeGpuMemoryBuffer();
  video_frame->metadata().read_lock_fences_enabled = has_native_gmb;

  return video_frame;
}

void FrameResources::ReturnGpuMemoryBufferFromFrame(
    std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer,
    const gpu::SyncToken& sync_token) {
  DCHECK(!gpu_memory_buffer_);
  gpu_memory_buffer_ = std::move(gpu_memory_buffer);
  sync_token_ = sync_token;
}

////////////////////////////////////////////////////////////////////////////////
// InternalRefCountedPool

InternalRefCountedPool::InternalRefCountedPool(
    std::unique_ptr<RenderableGpuMemoryBufferVideoFramePool::Context> context,
    const VideoPixelFormat format)
    : format_(format), context_(std::move(context)) {}

scoped_refptr<VideoFrame> InternalRefCountedPool::MaybeCreateVideoFrame(
    const gfx::Size& coded_size,
    const gfx::ColorSpace& color_space) {
  // Find or create a suitable FrameResources.
  std::unique_ptr<FrameResources> frame_resources;
  while (!available_frame_resources_.empty()) {
    frame_resources = std::move(available_frame_resources_.front());
    available_frame_resources_.pop_front();
    if (!frame_resources->IsCompatibleWith(coded_size, color_space)) {
      frame_resources = nullptr;
      continue;
    }
  }
  if (!frame_resources) {
    frame_resources = std::make_unique<FrameResources>(this, format_,
                                                       coded_size, color_space);
    if (!frame_resources->Initialize()) {
      DLOG(ERROR) << "Failed to initialize frame resources.";
      return nullptr;
    }
  }
  DCHECK(frame_resources);

  // Create a VideoFrame from the FrameResources.
  auto video_frame = frame_resources->CreateVideoFrameAndTakeGpuMemoryBuffer();
  if (!video_frame) {
    DLOG(ERROR) << "Failed to create VideoFrame from FrameResources.";
    return nullptr;
  }

  // Set the ReleaseMailboxAndGpuMemoryBufferCB to return the GpuMemoryBuffer to
  // the FrameResources, and return the FrameResources to the available pool. Do
  // this on the calling thread.
  auto callback = base::BindOnce(&InternalRefCountedPool::OnVideoFrameDestroyed,
                                 this, std::move(frame_resources));
  video_frame->SetReleaseMailboxAndGpuMemoryBufferCB(
      base::BindPostTaskToCurrentDefault(std::move(callback), FROM_HERE));
  return video_frame;
}

void InternalRefCountedPool::OnVideoFrameDestroyed(
    std::unique_ptr<FrameResources> frame_resources,
    const gpu::SyncToken& sync_token,
    std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer) {
  frame_resources->ReturnGpuMemoryBufferFromFrame(std::move(gpu_memory_buffer),
                                                  sync_token);

  if (shutting_down_) {
    return;
  }

  // TODO(crbug.com/40174702): Determine if we can get away with just
  // having 1 available frame, or if that will cause flakey underruns.
  constexpr size_t kMaxAvailableFrames = 2;
  available_frame_resources_.push_back(std::move(frame_resources));
  while (available_frame_resources_.size() > kMaxAvailableFrames) {
    available_frame_resources_.pop_front();
  }
}

void InternalRefCountedPool::Shutdown() {
  shutting_down_ = true;
  available_frame_resources_.clear();
}

RenderableGpuMemoryBufferVideoFramePool::Context*
InternalRefCountedPool::GetContext() const {
  return context_.get();
}

InternalRefCountedPool::~InternalRefCountedPool() {
  DCHECK(shutting_down_);
  DCHECK(available_frame_resources_.empty());
}

////////////////////////////////////////////////////////////////////////////////
// RenderableGpuMemoryBufferVideoFramePoolImpl

RenderableGpuMemoryBufferVideoFramePoolImpl::
    RenderableGpuMemoryBufferVideoFramePoolImpl(
        std::unique_ptr<RenderableGpuMemoryBufferVideoFramePool::Context>
            context,
        const VideoPixelFormat format,
        const bool is_mappable_si_enabled)
    : format_(format),
      pool_internal_(
          base::MakeRefCounted<InternalRefCountedPool>(std::move(context),
                                                       format)),
      is_mappable_si_enabled_(is_mappable_si_enabled) {}

scoped_refptr<VideoFrame>
RenderableGpuMemoryBufferVideoFramePoolImpl::MaybeCreateVideoFrame(
    const gfx::Size& coded_size,
    const gfx::ColorSpace& color_space) {
  return pool_internal_->MaybeCreateVideoFrame(coded_size, color_space);
}

bool RenderableGpuMemoryBufferVideoFramePoolImpl::
    IsMappableSIEnabledForTesting() const {
  return is_mappable_si_enabled_;
}

RenderableGpuMemoryBufferVideoFramePoolImpl::
    ~RenderableGpuMemoryBufferVideoFramePoolImpl() {
  pool_internal_->Shutdown();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// media::RenderableGpuMemoryBufferVideoFramePool

// static
std::unique_ptr<RenderableGpuMemoryBufferVideoFramePool>
RenderableGpuMemoryBufferVideoFramePool::Create(
    std::unique_ptr<Context> context,
    VideoPixelFormat format) {
  return std::make_unique<RenderableGpuMemoryBufferVideoFramePoolImpl>(
      std::move(context), format,
      base::FeatureList::IsEnabled(
          kUseMappableSIForRenderableGpuMemoryBufferVideoFramePool));
}

}  // namespace media
