// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/ipc/service/picture_buffer_manager.h"

#include <map>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "media/base/video_util.h"

namespace media {

namespace {

// Generates nonnegative picture buffer IDs, which are assumed to be unique.
int32_t NextID(int32_t* counter) {
  int32_t value = *counter;
  *counter = (*counter + 1) & 0x3FFFFFFF;
  return value;
}

class PictureBufferManagerImpl : public PictureBufferManager {
 public:
  explicit PictureBufferManagerImpl(
      ReusePictureBufferCB reuse_picture_buffer_cb)
      : reuse_picture_buffer_cb_(std::move(reuse_picture_buffer_cb)) {
    DVLOG(1) << __func__;
  }

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

    // If there are no assigned picture buffers, predict that the VDA will
    // request some.
    if (picture_buffers_.empty())
      return true;

    // Predict that the VDA can output a picture if at least one picture buffer
    // is not in use as an output.
    for (const auto& it : picture_buffers_) {
      const auto& state = it.second.state;
      if (std::find(state.begin(), state.end(), PictureBufferState::OUTPUT) ==
          state.end())
        return true;
    }

    return false;
  }

  std::vector<PictureBuffer> CreatePictureBuffers(
      uint32_t count,
      VideoPixelFormat pixel_format,
      uint32_t planes,
      gfx::Size texture_size,
      uint32_t texture_target) override {
    DVLOG(2) << __func__;
    DCHECK(gpu_task_runner_);
    DCHECK(gpu_task_runner_->BelongsToCurrentThread());
    DCHECK(count);
    DCHECK(planes);
    DCHECK_LE(planes, static_cast<uint32_t>(VideoFrame::kMaxPlanes));

    // TODO(sandersd): Consider requiring that CreatePictureBuffers() is called
    // with the context current.
    if (!command_buffer_helper_->MakeContextCurrent()) {
      DVLOG(1) << "Failed to make context current";
      return std::vector<PictureBuffer>();
    }

    std::vector<PictureBuffer> picture_buffers;
    for (uint32_t i = 0; i < count; i++) {
      PictureBuffer::TextureIds service_ids;
      PictureBufferData picture_data = {pixel_format, texture_size};

      for (uint32_t j = 0; j < planes; j++) {
        // Create a texture for this plane.
        GLuint service_id = command_buffer_helper_->CreateTexture(
            texture_target, GL_RGBA, texture_size.width(),
            texture_size.height(), GL_RGBA, GL_UNSIGNED_BYTE);
        DCHECK(service_id);
        service_ids.push_back(service_id);

        // The texture is not cleared yet, but it will be before the VDA outputs
        // it. Rather than requiring output to happen on the GPU thread, mark
        // the texture as cleared immediately.
        command_buffer_helper_->SetCleared(service_id);

        // Generate a mailbox while we are still on the GPU thread.
        picture_data.mailbox_holders[j] = gpu::MailboxHolder(
            command_buffer_helper_->CreateMailbox(service_id), gpu::SyncToken(),
            texture_target);
      }

      // Generate a picture buffer ID and record the picture buffer.
      int32_t picture_buffer_id = NextID(&picture_buffer_id_);
      {
        base::AutoLock lock(picture_buffers_lock_);
        DCHECK(!picture_buffers_.count(picture_buffer_id));
        picture_buffers_[picture_buffer_id] = picture_data;
      }

      // Since our textures have no client IDs, we reuse the service IDs as
      // convenient unique identifiers.
      //
      // TODO(sandersd): Refactor the bind image callback to use service IDs so
      // that we can get rid of the client IDs altogether.
      picture_buffers.emplace_back(picture_buffer_id, texture_size, service_ids,
                                   service_ids, texture_target, pixel_format);

      // Record the textures used by the picture buffer.
      picture_buffer_textures_[picture_buffer_id] = std::move(service_ids);
    }
    return picture_buffers;
  }

  bool DismissPictureBuffer(int32_t picture_buffer_id) override {
    DVLOG(2) << __func__ << "(" << picture_buffer_id << ")";
    DCHECK(gpu_task_runner_);
    DCHECK(gpu_task_runner_->BelongsToCurrentThread());

    base::AutoLock lock(picture_buffers_lock_);

    // Check the state of the picture buffer.
    const auto& it = picture_buffers_.find(picture_buffer_id);
    if (it == picture_buffers_.end()) {
      DVLOG(1) << "Unknown picture buffer " << picture_buffer_id;
      return false;
    }

    bool is_available = it->second.IsAvailable();

    // Destroy the picture buffer data.
    picture_buffers_.erase(it);

    // If the picture was not bound to any VideoFrame, we can destroy its
    // textures immediately.
    if (is_available) {
      gpu_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(
              &PictureBufferManagerImpl::DestroyPictureBufferTextures, this,
              picture_buffer_id));
    }

    return true;
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
    if (it == picture_buffers_.end()) {
      DVLOG(1) << "Unknown picture buffer " << picture_buffer_id;
      return nullptr;
    }

    PictureBufferData& picture_buffer_data = it->second;
    // Ensure that the picture buffer is large enough.
    if (!gfx::Rect(picture_buffer_data.texture_size).Contains(visible_rect)) {
      DLOG(WARNING) << "visible_rect " << visible_rect.ToString()
                    << " exceeds coded_size "
                    << picture_buffer_data.texture_size.ToString();
      double pixel_aspect_ratio =
          GetPixelAspectRatio(visible_rect, natural_size);
      visible_rect.Intersect(gfx::Rect(picture_buffer_data.texture_size));
      natural_size = GetNaturalSize(visible_rect, pixel_aspect_ratio);
    }

    // Mark the picture as an output.
    picture_buffer_data.state.push_back(PictureBufferState::OUTPUT);

    // Create and return a VideoFrame for the picture buffer.
    scoped_refptr<VideoFrame> frame = VideoFrame::WrapNativeTextures(
        picture_buffer_data.pixel_format, picture_buffer_data.mailbox_holders,
        base::BindRepeating(&PictureBufferManagerImpl::OnVideoFrameDestroyed,
                            this, picture_buffer_id),
        picture_buffer_data.texture_size, visible_rect, natural_size,
        timestamp);

    frame->set_color_space(picture.color_space());

    if (picture.allow_overlay())
      frame->metadata()->SetBoolean(VideoFrameMetadata::ALLOW_OVERLAY, true);

    // TODO(sandersd): Provide an API for VDAs to control this.
    frame->metadata()->SetBoolean(VideoFrameMetadata::POWER_EFFICIENT, true);

    return frame;
  }

 private:
  ~PictureBufferManagerImpl() override { DVLOG(1) << __func__; }

  void OnVideoFrameDestroyed(int32_t picture_buffer_id,
                             const gpu::SyncToken& sync_token) {
    DVLOG(3) << __func__ << "(" << picture_buffer_id << ")";

    base::AutoLock lock(picture_buffers_lock_);

    // If the picture buffer is still assigned, mark it as unreleased.
    const auto& it = picture_buffers_.find(picture_buffer_id);
    if (it != picture_buffers_.end()) {
      auto& state = it->second.state;
      auto state_it =
          std::find(state.begin(), state.end(), PictureBufferState::OUTPUT);
      if (state_it != state.end())
        state.erase(state_it);
      state.push_back(PictureBufferState::WAITING_FOR_SYNCTOKEN);
    }

    // Wait for the SyncToken release.
    gpu_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &CommandBufferHelper::WaitForSyncToken, command_buffer_helper_,
            sync_token,
            base::BindOnce(&PictureBufferManagerImpl::OnSyncTokenReleased, this,
                           picture_buffer_id)));
  }

  void OnSyncTokenReleased(int32_t picture_buffer_id) {
    DVLOG(3) << __func__ << "(" << picture_buffer_id << ")";
    DCHECK(gpu_task_runner_);
    DCHECK(gpu_task_runner_->BelongsToCurrentThread());

    // If the picture buffer is still assigned, mark it as available.
    bool is_assigned = false;
    {
      base::AutoLock lock(picture_buffers_lock_);
      const auto& it = picture_buffers_.find(picture_buffer_id);
      if (it != picture_buffers_.end()) {
        auto& state = it->second.state;
        auto state_it = std::find(state.begin(), state.end(),
                                  PictureBufferState::WAITING_FOR_SYNCTOKEN);
        if (state_it != state.end())
          state.erase(state_it);
        is_assigned = true;
      }
    }

    // If the picture buffer is still assigned, it is ready to be reused.
    // Otherwise it has been dismissed and we can now delete its textures.
    // Neither of these operations should be done while holding the lock.
    if (is_assigned) {
      reuse_picture_buffer_cb_.Run(picture_buffer_id);
    } else {
      DestroyPictureBufferTextures(picture_buffer_id);
    }
  }

  void DestroyPictureBufferTextures(int32_t picture_buffer_id) {
    DVLOG(3) << __func__ << "(" << picture_buffer_id << ")";
    DCHECK(gpu_task_runner_);
    DCHECK(gpu_task_runner_->BelongsToCurrentThread());

    if (!command_buffer_helper_->MakeContextCurrent())
      return;

    const auto& it = picture_buffer_textures_.find(picture_buffer_id);
    DCHECK(it != picture_buffer_textures_.end());
    for (GLuint service_id : it->second)
      command_buffer_helper_->DestroyTexture(service_id);
    picture_buffer_textures_.erase(it);
  }

  ReusePictureBufferCB reuse_picture_buffer_cb_;

  scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner_;
  scoped_refptr<CommandBufferHelper> command_buffer_helper_;

  int32_t picture_buffer_id_ = 0;
  // Includes picture puffers that have been dismissed if their textures have
  // not been deleted yet.
  std::map<int32_t, std::vector<GLuint>> picture_buffer_textures_;

  base::Lock picture_buffers_lock_;
  enum class PictureBufferState {
    // Output by the VDA, still bound to a VideoFrame.
    OUTPUT,
    // Waiting on a SyncToken before being reused.
    WAITING_FOR_SYNCTOKEN,
  };
  struct PictureBufferData {
    VideoPixelFormat pixel_format;
    gfx::Size texture_size;
    // The picture buffer might be sent from VDA multiple times. Therefore we
    // use vector to track the status. The state is empty when the picture
    // buffer is not bound to any VideoFrame.
    std::vector<PictureBufferState> state;
    gpu::MailboxHolder mailbox_holders[VideoFrame::kMaxPlanes];

    // Available for use by the VDA.
    bool IsAvailable() const { return state.empty(); }
  };
  // Pictures buffers that are assigned to the VDA.
  std::map<int32_t, PictureBufferData> picture_buffers_
      GUARDED_BY(picture_buffers_lock_);

  DISALLOW_COPY_AND_ASSIGN(PictureBufferManagerImpl);
};

}  // namespace

// static
scoped_refptr<PictureBufferManager> PictureBufferManager::Create(
    ReusePictureBufferCB reuse_picture_buffer_cb) {
  return base::MakeRefCounted<PictureBufferManagerImpl>(
      std::move(reuse_picture_buffer_cb));
}

}  // namespace media
