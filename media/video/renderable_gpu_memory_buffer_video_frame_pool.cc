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
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "media/base/format_utils.h"
#include "media/base/media_switches.h"
#include "media/base/video_frame.h"

namespace media {

namespace {

class InternalRefCountedPool;

// The VideoFrame-backing resources that are reused by the pool, namely, a
// GpuMemoryBuffer and a SharedImage. This retains a reference to the
// InternalRefCountedPool that created it. Not safe for concurrent use.
class FrameResources {
 public:
  FrameResources(scoped_refptr<InternalRefCountedPool> pool,
                 const gfx::Size& visible_size);
  ~FrameResources();
  FrameResources(const FrameResources& other) = delete;
  FrameResources& operator=(const FrameResources& other) = delete;

  // Allocate GpuMemoryBuffer and create SharedImage. Returns false on failure
  // to do so. The |requires_cpu_access| parameter indicates whether CPU access
  // to the video frames is needed. If true, linear buffers that are mappable by
  // CPU will be used; otherwise, GPU optimized buffers may be preferred.
  bool Initialize(VideoPixelFormat format,
                  const gfx::ColorSpace& color_space,
                  bool requires_cpu_access);

  // Return true if these resources can be reused for a frame with the specified
  // parameters.
  bool IsCompatibleWith(const gfx::Size& visible_size,
                        const gfx::ColorSpace& color_space) const;

  // Create a VideoFrame using these resources.
  scoped_refptr<VideoFrame> CreateVideoFrame();

  // Update the |sync_token_| to |sync_token|. The |shared_image_| can be
  // re-used or destroyed after this |sync_token_| has passed.
  void SetSharedImageReleaseSyncToken(const gpu::SyncToken& sync_token);

 private:
  // This reference ensures that the creating InternalRefCountedPool (and,
  // critically, its interface through which `this` can destroy its
  // SharedImage) will not be destroyed until after `this` is destroyed.
  const scoped_refptr<InternalRefCountedPool> pool_;

  const gfx::Size visible_size_;
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
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

  explicit InternalRefCountedPool(
      std::unique_ptr<RenderableGpuMemoryBufferVideoFramePool::Context> context,
      VideoPixelFormat format,
      bool requires_cpu_access);

  // Create a VideoFrame with the specified parameters, reusing the resources
  // of a previous frame, if possible.
  scoped_refptr<VideoFrame> MaybeCreateVideoFrame(
      const gfx::Size& visible_size,
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

  // Callback made when the VideoFrame is destroyed. This callback then either
  // returns |frame_resources| to |available_frame_resources_| or destroys it.
  void OnVideoFrameDestroyed(std::unique_ptr<FrameResources> frame_resources,
                             const gpu::SyncToken& sync_token);

  const VideoPixelFormat format_;
  const std::unique_ptr<RenderableGpuMemoryBufferVideoFramePool::Context>
      context_;
  std::list<std::unique_ptr<FrameResources>> available_frame_resources_;
  bool shutting_down_ = false;
  // Indicates whether the capture pipeline requires CPU mapping of captured
  // frames. If true, linear CPU mappable buffers will be used.
  bool requires_cpu_access_ = true;
};

// Implementation of the RenderableGpuMemoryBufferVideoFramePool abstract
// class.
class RenderableGpuMemoryBufferVideoFramePoolImpl
    : public RenderableGpuMemoryBufferVideoFramePool {
 public:
  explicit RenderableGpuMemoryBufferVideoFramePoolImpl(
      std::unique_ptr<RenderableGpuMemoryBufferVideoFramePool::Context> context,
      VideoPixelFormat format,
      bool requires_cpu_access);

  scoped_refptr<VideoFrame> MaybeCreateVideoFrame(
      const gfx::Size& visible_size,
      const gfx::ColorSpace& color_space) override;

  ~RenderableGpuMemoryBufferVideoFramePoolImpl() override;

  const VideoPixelFormat format_;
  const scoped_refptr<InternalRefCountedPool> pool_internal_;
};

////////////////////////////////////////////////////////////////////////////////
// FrameResources

FrameResources::FrameResources(scoped_refptr<InternalRefCountedPool> pool,
                               const gfx::Size& visible_size)
    : pool_(std::move(pool)), visible_size_(visible_size) {}

FrameResources::~FrameResources() {
  if (shared_image_) {
    pool_->GetContext()->DestroySharedImage(sync_token_,
                                            std::move(shared_image_));
  }
}

gfx::Size GetCodedSizeForVideoPixelFormat(VideoPixelFormat format,
                                          const gfx::Size& visible_size) {
  switch (format) {
    case PIXEL_FORMAT_ARGB:
    case PIXEL_FORMAT_ABGR:
    case PIXEL_FORMAT_RGBAF16:
      return visible_size;
    case PIXEL_FORMAT_NV12:
      // Align number of rows to 2, because it's required by YUV_420_BIPLANAR
      // buffer allocation code.
      // Align buffer stride to 4, because our SharedImage shared memory backing
      // code requires it, since it sometimes treats Y-planes are 4 bytes per
      // pixel textures.
      return {cc::MathUtil::CheckedRoundUp(visible_size.width(), 4),
              cc::MathUtil::CheckedRoundUp(visible_size.height(), 2)};
    default:
      NOTREACHED();
  }
}

bool FrameResources::Initialize(VideoPixelFormat format,
                                const gfx::ColorSpace& color_space,
                                bool requires_cpu_access) {
  // Currently only support ARGB, ABGR and NV12.
  CHECK(format == PIXEL_FORMAT_ARGB || format == PIXEL_FORMAT_ABGR ||
        format == PIXEL_FORMAT_NV12 || format == PIXEL_FORMAT_RGBAF16)
      << format;

  auto* context = pool_->GetContext();

  gfx::BufferUsage buffer_usage = gfx::BufferUsage::SCANOUT_CPU_READ_WRITE;

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
  buffer_usage = gfx::BufferUsage::SCANOUT_VEA_CPU_READ;
#elif BUILDFLAG(IS_LINUX)
  // On Linux, GBM_BO_USE_LINEAR (implied by SCANOUT_CPU_READ_WRITE) can
  // prevent GPU rendering on some drivers, notably NVIDIA's GBM driver,
  // because it disables GBM_BO_USE_RENDERING. Use SCANOUT instead if
  // linear buffers are not supported to ensure GPU rendering compatibility.
  if (!requires_cpu_access) {
    buffer_usage = gfx::BufferUsage::SCANOUT;
  }
#endif

  const gfx::Size coded_size =
      GetCodedSizeForVideoPixelFormat(format, visible_size_);

  gpu::SharedImageUsageSet usage =
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
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ;

  auto si_caps = context->GetCapabilities();
#if BUILDFLAG(IS_WIN)
  // On Windows, overlays are in general not supported. However, in some
  // cases they are supported for the software video frame use case in
  // particular. This cap details whether that support is present.
  const bool add_scanout_usage =
      si_caps.supports_scanout_shared_images_for_software_video_frames;
#else
  // On all other platforms, whether scanout for SharedImages is supported
  // for this particular use case is no different than the general case.
  const bool add_scanout_usage = si_caps.supports_scanout_shared_images;
#endif
  if (add_scanout_usage) {
    usage |= gpu::SHARED_IMAGE_USAGE_SCANOUT;
  }

  const viz::SharedImageFormat si_format =
      VideoPixelFormatToSharedImageFormat(format).value();

  shared_image_ = context->CreateSharedImage(
      coded_size, buffer_usage, si_format, color_space, usage, sync_token_);
  if (!shared_image_) {
    DLOG(ERROR) << "Failed to allocate shared image for frame: visible_size="
                << visible_size_.ToString()
                << ", si_format=" << si_format.ToString();
    return false;
  }
  return true;
}

bool FrameResources::IsCompatibleWith(
    const gfx::Size& visible_size,
    const gfx::ColorSpace& color_space) const {
  return visible_size_ == visible_size &&
         shared_image_->color_space() == color_space;
}

scoped_refptr<VideoFrame> FrameResources::CreateVideoFrame() {
  const gfx::Rect visible_rect(visible_size_);
  const gfx::Size natural_size = visible_size_;

  CHECK(shared_image_);
  auto video_frame = VideoFrame::WrapMappableSharedImage(
      shared_image_, sync_token_, VideoFrame::ReleaseMailboxCB(), visible_rect,
      natural_size, base::TimeDelta());
  if (!video_frame) {
    return nullptr;
  }

  video_frame->set_color_space(shared_image_->color_space());
  video_frame->metadata().allow_overlay =
      shared_image_->usage().Has(gpu::SHARED_IMAGE_USAGE_SCANOUT);

  // Only native (non shared memory) GMBs require waiting on GPU fences.
  const bool has_native_gmb = video_frame->HasNativeGpuMemoryBuffer();
  video_frame->metadata().read_lock_fences_enabled = has_native_gmb;

  return video_frame;
}

void FrameResources::SetSharedImageReleaseSyncToken(
    const gpu::SyncToken& sync_token) {
  sync_token_ = sync_token;
}

////////////////////////////////////////////////////////////////////////////////
// InternalRefCountedPool

InternalRefCountedPool::InternalRefCountedPool(
    std::unique_ptr<RenderableGpuMemoryBufferVideoFramePool::Context> context,
    const VideoPixelFormat format,
    bool requires_cpu_access)
    : format_(format),
      context_(std::move(context)),
      requires_cpu_access_(requires_cpu_access) {}

scoped_refptr<VideoFrame> InternalRefCountedPool::MaybeCreateVideoFrame(
    const gfx::Size& visible_size,
    const gfx::ColorSpace& color_space) {
  // Find or create a suitable FrameResources.
  std::unique_ptr<FrameResources> frame_resources;
  while (!available_frame_resources_.empty()) {
    frame_resources = std::move(available_frame_resources_.front());
    available_frame_resources_.pop_front();
    if (!frame_resources->IsCompatibleWith(visible_size, color_space)) {
      frame_resources = nullptr;
      continue;
    }
    break;
  }
  if (!frame_resources) {
    frame_resources = std::make_unique<FrameResources>(this, visible_size);
    if (!frame_resources->Initialize(format_, color_space,
                                     requires_cpu_access_)) {
      DLOG(ERROR) << "Failed to initialize frame resources.";
      return nullptr;
    }
  }
  DCHECK(frame_resources);

  // Create a VideoFrame from the FrameResources.
  auto video_frame = frame_resources->CreateVideoFrame();
  if (!video_frame) {
    DLOG(ERROR) << "Failed to create VideoFrame from FrameResources.";
    return nullptr;
  }

  // Set the ReleaseMailboxCB to return the FrameResources to the available
  // pool. Do this on the calling thread.
  auto callback = base::BindOnce(&InternalRefCountedPool::OnVideoFrameDestroyed,
                                 this, std::move(frame_resources));
  video_frame->SetReleaseMailboxCB(
      base::BindPostTaskToCurrentDefault(std::move(callback), FROM_HERE));
  return video_frame;
}

void InternalRefCountedPool::OnVideoFrameDestroyed(
    std::unique_ptr<FrameResources> frame_resources,
    const gpu::SyncToken& sync_token) {
  frame_resources->SetSharedImageReleaseSyncToken(sync_token);

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
        bool requires_cpu_access)
    : format_(format),
      pool_internal_(
          base::MakeRefCounted<InternalRefCountedPool>(std::move(context),
                                                       format,
                                                       requires_cpu_access)) {}

scoped_refptr<VideoFrame>
RenderableGpuMemoryBufferVideoFramePoolImpl::MaybeCreateVideoFrame(
    const gfx::Size& visible_size,
    const gfx::ColorSpace& color_space) {
  return pool_internal_->MaybeCreateVideoFrame(visible_size, color_space);
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
    VideoPixelFormat format,
    bool requires_cpu_access) {
  return std::make_unique<RenderableGpuMemoryBufferVideoFramePoolImpl>(
      std::move(context), format, requires_cpu_access);
}

}  // namespace media
