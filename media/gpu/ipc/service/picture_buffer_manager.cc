// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/ipc/service/picture_buffer_manager.h"

#include <map>
#include <set>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "ui/gfx/gpu_memory_buffer.h"

#if BUILDFLAG(USE_VAAPI) || BUILDFLAG(USE_V4L2_CODEC)
#include "media/gpu/chromeos/platform_video_frame_utils.h"
#endif  // BUILDFLAG(USE_VAAPI) || BUILDFLAG(USE_V4L2_CODEC)

namespace media {

namespace {

// Generates nonnegative picture buffer IDs, which are assumed to be unique.
int32_t NextID(int32_t* counter) {
  int32_t value = *counter;
  *counter = (*counter + 1) & 0x3FFFFFFF;
  return value;
}

}  // namespace

class PictureBufferManagerImpl : public PictureBufferManager {
 public:
  PictureBufferManagerImpl(bool allocate_gpu_memory_buffers,
                           ReusePictureBufferCB reuse_picture_buffer_cb)
      : allocate_gpu_memory_buffers_(allocate_gpu_memory_buffers),
        reuse_picture_buffer_cb_(std::move(reuse_picture_buffer_cb)) {
    DVLOG(1) << __func__;
  }
  PictureBufferManagerImpl(const PictureBufferManagerImpl&) = delete;
  PictureBufferManagerImpl& operator=(const PictureBufferManagerImpl&) = delete;

  void Initialize(
      scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
      scoped_refptr<CommandBufferHelper> command_buffer_helper) override {
    DVLOG(1) << __func__;
    DCHECK(!gpu_task_runner_);

    gpu_task_runner_ = std::move(gpu_task_runner);
    command_buffer_helper_ = std::move(command_buffer_helper);
  }

  bool CanReadWithoutStalling() override {
    DVLOG(3) << __func__;

    base::AutoLock lock(picture_buffers_lock_);

    // If at least one picture buffer is not in use, predict that the VDA can
    // use it to output another picture.
    bool has_assigned_picture_buffer = false;
    for (const auto& it : picture_buffers_) {
      if (!it.second.dismissed) {
        // Note: If a picture buffer is waiting for SyncToken release, that
        // release is already in some command buffer (or the wait is invalid).
        // The wait will complete without further interaction from the client.
        if (it.second.output_count == 0)
          return true;
        has_assigned_picture_buffer = true;
      }
    }

    // If there are no assigned picture buffers, predict that the VDA will
    // request some.
    return !has_assigned_picture_buffer;
  }

  std::vector<std::pair<PictureBuffer, gfx::GpuMemoryBufferHandle>>
  CreatePictureBuffers(
      uint32_t count,
      VideoPixelFormat pixel_format,
      uint32_t planes,
      gfx::Size texture_size,
      uint32_t texture_target,
      VideoDecodeAccelerator::TextureAllocationMode mode) override {
    DVLOG(2) << __func__;
    DCHECK(gpu_task_runner_);
    DCHECK(gpu_task_runner_->BelongsToCurrentThread());
    DCHECK(count);
    DCHECK(planes);
    DCHECK_LE(planes, static_cast<uint32_t>(VideoFrame::kMaxPlanes));
    DCHECK(!allocate_gpu_memory_buffers_ ||
           mode == VideoDecodeAccelerator::TextureAllocationMode::
                       kDoNotAllocateGLTextures);
#if BUILDFLAG(IS_APPLE)
    // GL texture allocation is not supported (and never requested) on Apple
    // platforms: see
    // VideoDecodeAccelerator::GetSharedImageTextureAllocationMode(), which is
    // where the value of `mode` that is passed in to this method comes from.
    CHECK_EQ(mode, VideoDecodeAccelerator::TextureAllocationMode::
                       kDoNotAllocateGLTextures);
#endif

    // TODO(sandersd): Consider requiring that CreatePictureBuffers() is
    // called with the context current.
    if (mode ==
        VideoDecodeAccelerator::TextureAllocationMode::kAllocateGLTextures) {
      if (!command_buffer_helper_->MakeContextCurrent()) {
        DVLOG(1) << "Failed to make context current";
        return {};
      }
    }

    std::vector<std::pair<PictureBuffer, gfx::GpuMemoryBufferHandle>>
        picture_buffers_and_gmbs;
    for (uint32_t i = 0; i < count; i++) {
      PictureBufferData picture_data = {pixel_format, texture_size};
#if !BUILDFLAG(IS_APPLE)
      if (mode ==
          VideoDecodeAccelerator::TextureAllocationMode::kAllocateGLTextures) {
        for (uint32_t j = 0; j < planes; j++) {
          // Use the plane size for texture-backed shared and non-shared images.
          // Adjust the size by the subsampling factor.
          const size_t width =
              VideoFrame::Columns(j, pixel_format, texture_size.width());
          const size_t height =
              VideoFrame::Rows(j, pixel_format, texture_size.height());

          picture_data.texture_sizes.emplace_back(width, height);

          // Create a texture for this plane.
          // When using shared images, the VDA might not require GL textures to
          // exist.
          // TODO(crbug.com/1011555): Do not allocate GL textures when unused.
          GLuint service_id = command_buffer_helper_->CreateTexture(
              texture_target, GL_RGBA, width, height, GL_RGBA,
              GL_UNSIGNED_BYTE);
          DCHECK(service_id);
          picture_data.service_ids.push_back(service_id);

          // The texture is not cleared yet, but it will be before the VDA
          // outputs it. Rather than requiring output to happen on the GPU
          // thread, mark the texture as cleared immediately.
          command_buffer_helper_->SetCleared(service_id);

          // Generate a mailbox while we are still on the GPU thread.
          picture_data.mailbox_holders[j] = gpu::MailboxHolder(
              command_buffer_helper_->CreateLegacyMailbox(service_id),
              gpu::SyncToken(), texture_target);
        }
      }
#endif  // !BUILDFLAG(IS_APPLE)

      gfx::GpuMemoryBufferHandle gmb_handle;
      if (allocate_gpu_memory_buffers_) {
#if BUILDFLAG(USE_VAAPI) || BUILDFLAG(USE_V4L2_CODEC)
        scoped_refptr<VideoFrame> gpu_memory_buffer_video_frame =
            CreateGpuMemoryBufferVideoFrame(
                pixel_format, texture_size, gfx::Rect(texture_size),
                texture_size, base::TimeDelta(),
                gfx::BufferUsage::SCANOUT_VDA_WRITE);
        if (!gpu_memory_buffer_video_frame)
          return {};
        if (gpu_memory_buffer_video_frame->format() != pixel_format) {
          // There is a mismatch (maybe deliberate) between
          // VideoPixelFormatToGfxBufferFormat() and
          // GfxBufferFormatToVideoPixelFormat(). For PIXEL_FORMAT_XBGR, the
          // former returns gfx::BufferFormat::RGBX_8888, but for
          // gfx::BufferFormat::RGBX_8888, the latter returns PIXEL_FORMAT_XRGB.
          // Just fail if the allocated format doesn't match the requested
          // format.
          //
          // TODO(andrescj): does this mismatch need to be fixed or is it
          // intentional?
          return {};
        }

        gfx::GpuMemoryBuffer* gmb =
            gpu_memory_buffer_video_frame->GetGpuMemoryBuffer();
        DCHECK(gmb);
        gmb_handle = gmb->CloneHandle();
        if (gmb_handle.type != gfx::NATIVE_PIXMAP ||
            gmb_handle.native_pixmap_handle.planes.empty()) {
          return {};
        }
        picture_data.gpu_memory_buffer_video_frame =
            std::move(gpu_memory_buffer_video_frame);
#else
        NOTREACHED_NORETURN();
#endif  // BUILDFLAG(USE_VAAPI) || BUILDFLAG(USE_V4L2_CODEC)
      }

      // Generate a picture buffer ID and record the picture buffer.
      int32_t picture_buffer_id = NextID(&picture_buffer_id_);
      {
        base::AutoLock lock(picture_buffers_lock_);
        DCHECK(!picture_buffers_.count(picture_buffer_id));
        picture_buffers_[picture_buffer_id] = picture_data;
      }

      // `picture_data.service_ids()` should be non-empty only when allocating
      // GL textures.
      CHECK(picture_data.service_ids.empty() ||
            mode == VideoDecodeAccelerator::TextureAllocationMode::
                        kAllocateGLTextures);

      // Since our textures have no client IDs, we reuse the service IDs as
      // convenient unique identifiers.
      //
      // TODO(sandersd): Refactor the bind image callback to use service IDs so
      // that we can get rid of the client IDs altogether.
      picture_buffers_and_gmbs.emplace_back(
          PictureBuffer{picture_buffer_id, texture_size,
                        picture_data.texture_sizes, picture_data.service_ids,
                        picture_data.service_ids, texture_target, pixel_format},
          std::move(gmb_handle));
    }
    return picture_buffers_and_gmbs;
  }

  bool DismissPictureBuffer(int32_t picture_buffer_id) override {
    DVLOG(2) << __func__ << "(" << picture_buffer_id << ")";

    base::AutoLock lock(picture_buffers_lock_);

    const auto& it = picture_buffers_.find(picture_buffer_id);
    if (it == picture_buffers_.end() || it->second.dismissed) {
      DVLOG(1) << "Unknown picture buffer " << picture_buffer_id;
      return false;
    }

    it->second.dismissed = true;

    // If the picture buffer is not in use, it should be destroyed immediately.
    if (!it->second.IsInUse()) {
      gpu_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&PictureBufferManagerImpl::DestroyPictureBuffer, this,
                         picture_buffer_id));
    }

    return true;
  }

  void DismissAllPictureBuffers() override {
    DVLOG(2) << __func__;

    std::vector<int32_t> assigned_picture_buffer_ids;
    {
      base::AutoLock lock(picture_buffers_lock_);

      for (const auto& it : picture_buffers_) {
        if (!it.second.dismissed)
          assigned_picture_buffer_ids.push_back(it.first);
      }
    }

    for (int32_t picture_buffer_id : assigned_picture_buffer_ids)
      DismissPictureBuffer(picture_buffer_id);
  }

  scoped_refptr<VideoFrame> CreateVideoFrame(Picture picture,
                                             base::TimeDelta timestamp,
                                             gfx::Rect visible_rect,
                                             gfx::Size natural_size) override {
    DVLOG(2) << __func__ << "(" << picture.picture_buffer_id() << ")";
    DCHECK(!picture.size_changed());
    DCHECK(!picture.texture_owner());
    DCHECK(!picture.wants_promotion_hint());

    base::AutoLock lock(picture_buffers_lock_);

    int32_t picture_buffer_id = picture.picture_buffer_id();

    // Verify that the picture buffer is available.
    const auto& it = picture_buffers_.find(picture_buffer_id);
    if (it == picture_buffers_.end() || it->second.dismissed) {
      DVLOG(1) << "Unknown picture buffer " << picture_buffer_id;
      return nullptr;
    }

    // Ensure that the picture buffer is large enough.
    PictureBufferData& picture_buffer_data = it->second;
    if (!gfx::Rect(picture_buffer_data.texture_size).Contains(visible_rect)) {
      DLOG(WARNING) << "visible_rect " << visible_rect.ToString()
                    << " exceeds coded_size "
                    << picture_buffer_data.texture_size.ToString();
      visible_rect.Intersect(gfx::Rect(picture_buffer_data.texture_size));
    }

    // Record the output.
    picture_buffer_data.output_count++;

    // If this |picture| has a SharedImage, then keep a reference to the
    // SharedImage in |picture_buffer_data| and update the gpu::MailboxHolder.
    for (int i = 0; i < VideoFrame::kMaxPlanes; i++) {
      auto image = picture.scoped_shared_image(i);
      if (image)
        picture_buffer_data.mailbox_holders[i] = image->GetMailboxHolder();
      picture_buffer_data.scoped_shared_images[i] = std::move(image);
    }

    // Create and return a VideoFrame for the picture buffer.
    scoped_refptr<VideoFrame> frame;
    if (picture_buffer_data.gpu_memory_buffer_video_frame) {
      frame = VideoFrame::WrapVideoFrame(
          picture_buffer_data.gpu_memory_buffer_video_frame,
          picture_buffer_data.gpu_memory_buffer_video_frame->format(),
          visible_rect, natural_size);
      if (!frame) {
        DLOG(ERROR) << "Failed to create VideoFrame for picture.";
        return nullptr;
      }
      frame->set_timestamp(timestamp);
      frame->AddDestructionObserver(
          base::BindOnce(&PictureBufferManagerImpl::OnVideoFrameDestroyed, this,
                         picture_buffer_id, gpu::SyncToken()));
    } else {
      frame = VideoFrame::WrapNativeTextures(
          picture_buffer_data.pixel_format, picture_buffer_data.mailbox_holders,
          base::BindOnce(&PictureBufferManagerImpl::OnVideoFrameDestroyed, this,
                         picture_buffer_id),
          picture_buffer_data.texture_size, visible_rect, natural_size,
          timestamp);
      if (!frame) {
        DLOG(ERROR) << "Failed to create VideoFrame for picture.";
        return nullptr;
      }
    }

    frame->set_color_space(picture.color_space());

    frame->set_shared_image_format_type(picture.shared_image_format_type());

    frame->metadata().allow_overlay = picture.allow_overlay();
    frame->metadata().read_lock_fences_enabled =
        picture.read_lock_fences_enabled();
    frame->metadata().is_webgpu_compatible = picture.is_webgpu_compatible();

    // TODO(sandersd): Provide an API for VDAs to control this.
    frame->metadata().power_efficient = true;

    return frame;
  }

 private:
  ~PictureBufferManagerImpl() override {
    DVLOG(1) << __func__;
    DCHECK(picture_buffers_.empty() ||
           (!command_buffer_helper_ || !command_buffer_helper_->HasStub()));
  }

  void OnVideoFrameDestroyed(int32_t picture_buffer_id,
                             const gpu::SyncToken& sync_token) {
    DVLOG(3) << __func__ << "(" << picture_buffer_id << ")";

    base::AutoLock lock(picture_buffers_lock_);

    // Record the picture buffer as waiting for SyncToken release (even if it
    // has been dismissed already).
    const auto& it = picture_buffers_.find(picture_buffer_id);
    DCHECK(it != picture_buffers_.end());
    DCHECK_GT(it->second.output_count, 0);
    it->second.output_count--;
    it->second.waiting_for_synctoken_count++;

    if (command_buffer_helper_) {
      // Wait for the SyncToken release.
      gpu_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(
              &CommandBufferHelper::WaitForSyncToken, command_buffer_helper_,
              sync_token,
              base::BindOnce(&PictureBufferManagerImpl::OnSyncTokenReleased,
                             this, picture_buffer_id)));
    } else {
      gpu_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&PictureBufferManagerImpl::OnSyncTokenReleased, this,
                         picture_buffer_id));
    }
  }

  void OnSyncTokenReleased(int32_t picture_buffer_id) {
    DVLOG(3) << __func__ << "(" << picture_buffer_id << ")";
    DCHECK(gpu_task_runner_);
    DCHECK(gpu_task_runner_->BelongsToCurrentThread());

    // Remove the pending wait.
    bool is_assigned = false;
    bool is_in_use = true;
    {
      base::AutoLock lock(picture_buffers_lock_);
      const auto& it = picture_buffers_.find(picture_buffer_id);
      DCHECK(it != picture_buffers_.end());

      DCHECK_GT(it->second.waiting_for_synctoken_count, 0);
      it->second.waiting_for_synctoken_count--;

      is_assigned = !it->second.dismissed;
      is_in_use = it->second.IsInUse();
    }

    // If the picture buffer is still assigned, it is ready to be reused.
    // Otherwise it should be destroyed if it is no longer in use. Neither of
    // these operations should be done while holding the lock.
    if (is_assigned) {
      // The callback is called even if the picture buffer is still in use; the
      // client is expected to wait for all copies of a picture buffer to be
      // returned before reusing any textures.
      reuse_picture_buffer_cb_.Run(picture_buffer_id);
    } else if (!is_in_use) {
      DestroyPictureBuffer(picture_buffer_id);
    }
  }

  void DestroyPictureBuffer(int32_t picture_buffer_id) {
    DVLOG(3) << __func__ << "(" << picture_buffer_id << ")";
    DCHECK(gpu_task_runner_);
    DCHECK(gpu_task_runner_->BelongsToCurrentThread());

    std::vector<GLuint> service_ids;
    std::array<scoped_refptr<Picture::ScopedSharedImage>,
               VideoFrame::kMaxPlanes>
        scoped_shared_images;
    {
      base::AutoLock lock(picture_buffers_lock_);
      const auto& it = picture_buffers_.find(picture_buffer_id);
      DCHECK(it != picture_buffers_.end());
      DCHECK(it->second.dismissed);
      DCHECK(!it->second.IsInUse());
      service_ids = std::move(it->second.service_ids);
      scoped_shared_images = std::move(it->second.scoped_shared_images);
      picture_buffers_.erase(it);
    }

    if (service_ids.empty())
      return;

    if (!command_buffer_helper_->MakeContextCurrent())
      return;

    for (GLuint service_id : service_ids)
      command_buffer_helper_->DestroyTexture(service_id);
  }

  const bool allocate_gpu_memory_buffers_;
  ReusePictureBufferCB reuse_picture_buffer_cb_;

  scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner_;
  scoped_refptr<CommandBufferHelper> command_buffer_helper_;

  int32_t picture_buffer_id_ = 0;

  struct PictureBufferData {
    VideoPixelFormat pixel_format;
    gfx::Size texture_size;
    std::vector<GLuint> service_ids;
    gpu::MailboxHolder mailbox_holders[VideoFrame::kMaxPlanes];
    std::vector<gfx::Size> texture_sizes;
    std::array<scoped_refptr<Picture::ScopedSharedImage>,
               VideoFrame::kMaxPlanes>
        scoped_shared_images;
    scoped_refptr<VideoFrame> gpu_memory_buffer_video_frame;
    bool dismissed = false;

    // The same picture buffer can be output from the VDA multiple times
    // concurrently, so the state is tracked using counts.
    //   |output_count|: Number of VideoFrames this picture buffer is bound to.
    //   |waiting_for_synctoken_count|: Number of returned frames that are
    //       waiting for SyncToken release.
    int output_count = 0;
    int waiting_for_synctoken_count = 0;

    bool IsInUse() const {
      return output_count > 0 || waiting_for_synctoken_count > 0;
    }
  };

  base::Lock picture_buffers_lock_;
  std::map<int32_t, PictureBufferData> picture_buffers_
      GUARDED_BY(picture_buffers_lock_);
};

// static
scoped_refptr<PictureBufferManager> PictureBufferManager::Create(
    bool allocate_gpu_memory_buffers,
    ReusePictureBufferCB reuse_picture_buffer_cb) {
  return base::MakeRefCounted<PictureBufferManagerImpl>(
      allocate_gpu_memory_buffers, std::move(reuse_picture_buffer_cb));
}

}  // namespace media
