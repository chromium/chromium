// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/android/stream_texture_wrapper_impl.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/task/bind_post_task.h"
#include "cc/layers/video_frame_provider.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "media/base/bind_to_current_loop.h"

namespace {
// Non-member function to allow it to run even after this class is deleted.
void OnReleaseVideoFrame(scoped_refptr<content::StreamTextureFactory> factories,
                         gpu::Mailbox mailbox,
                         const gpu::SyncToken& sync_token) {
  gpu::SharedImageInterface* sii = factories->SharedImageInterface();
  sii->DestroySharedImage(sync_token, mailbox);
  sii->Flush();
}
}

namespace content {

StreamTextureWrapperImpl::StreamTextureWrapperImpl(
    bool enable_texture_copy,
    scoped_refptr<StreamTextureFactory> factory,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner)
    : enable_texture_copy_(enable_texture_copy),
      factory_(factory),
      main_task_runner_(main_task_runner) {}

StreamTextureWrapperImpl::~StreamTextureWrapperImpl() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // Clears create video frame callback so it couldn't be called from compositor
  // thread after |this| is being destroyed.
  if (stream_texture_proxy_)
    stream_texture_proxy_->ClearCreateVideoFrameCB();

  SetCurrentFrameInternal(nullptr);
}

media::ScopedStreamTextureWrapper StreamTextureWrapperImpl::Create(
    bool enable_texture_copy,
    scoped_refptr<StreamTextureFactory> factory,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner) {
  return media::ScopedStreamTextureWrapper(new StreamTextureWrapperImpl(
      enable_texture_copy, factory, main_task_runner));
}

scoped_refptr<media::VideoFrame> StreamTextureWrapperImpl::GetCurrentFrame() {
  base::AutoLock auto_lock(current_frame_lock_);
  return current_frame_;
}

void StreamTextureWrapperImpl::CreateVideoFrame(
    const gpu::Mailbox& mailbox,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const absl::optional<gpu::VulkanYCbCrInfo>& ycbcr_info) {
  // This message comes from GPU process when the SharedImage is already
  // created, so we don't need to wait on any synctoken, mailbox is ready to
  // use.
  gpu::MailboxHolder holders[media::VideoFrame::kMaxPlanes] = {
      gpu::MailboxHolder(mailbox, gpu::SyncToken(), GL_TEXTURE_EXTERNAL_OES)};

  gpu::SharedImageInterface* sii = factory_->SharedImageInterface();
  sii->NotifyMailboxAdded(mailbox, gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
                                       gpu::SHARED_IMAGE_USAGE_GLES2 |
                                       gpu::SHARED_IMAGE_USAGE_RASTER);

  // The pixel format doesn't matter here as long as it's valid for texture
  // frames. But SkiaRenderer wants to ensure that the format of the resource
  // used here which will eventually create a promise image must match the
  // format of the resource(SharedImageVideo) used to create fulfill image.
  // crbug.com/1028746. Since we create all the textures/abstract textures as
  // well as shared images for video to be of format RGBA, we need to use the
  // pixel format as ABGR here(which corresponds to 32bpp RGBA).
  scoped_refptr<media::VideoFrame> new_frame =
      media::VideoFrame::WrapNativeTextures(
          media::PIXEL_FORMAT_ABGR, holders,
          base::BindPostTask(
              main_task_runner_,
              base::BindOnce(&OnReleaseVideoFrame, factory_, mailbox)),
          coded_size, visible_rect, visible_rect.size(), base::TimeDelta());
  new_frame->set_ycbcr_info(ycbcr_info);

  if (enable_texture_copy_)
    new_frame->metadata().copy_required = true;

  SetCurrentFrameInternal(new_frame);
}

void StreamTextureWrapperImpl::ForwardStreamTextureForSurfaceRequest(
    const base::UnguessableToken& request_token) {
  stream_texture_proxy_->ForwardStreamTextureForSurfaceRequest(request_token);
}

void StreamTextureWrapperImpl::ClearReceivedFrameCBOnAnyThread() {
  // Safely stop StreamTextureProxy from signaling the arrival of new frames.
  if (stream_texture_proxy_)
    stream_texture_proxy_->ClearReceivedFrameCB();
}

void StreamTextureWrapperImpl::SetCurrentFrameInternal(
    scoped_refptr<media::VideoFrame> video_frame) {
  base::AutoLock auto_lock(current_frame_lock_);
  current_frame_ = std::move(video_frame);
}

void StreamTextureWrapperImpl::UpdateTextureSize(const gfx::Size& new_size) {
  DVLOG(2) << __func__;

  if (!main_task_runner_->BelongsToCurrentThread()) {
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&StreamTextureWrapperImpl::UpdateTextureSize,
                                  weak_factory_.GetWeakPtr(), new_size));
    return;
  }

  // InitializeOnMainThread() hasn't run, or failed.
  if (!stream_texture_proxy_)
    return;

  if (rotated_visible_size_ == new_size)
    return;

  rotated_visible_size_ = new_size;
  stream_texture_proxy_->UpdateRotatedVisibleSize(rotated_visible_size_);
}

void StreamTextureWrapperImpl::Initialize(
    const base::RepeatingClosure& received_frame_cb,
    scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
    StreamTextureWrapperInitCB init_cb) {
  DVLOG(2) << __func__;

  compositor_task_runner_ = compositor_task_runner;

  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&StreamTextureWrapperImpl::InitializeOnMainThread,
                     weak_factory_.GetWeakPtr(), received_frame_cb,
                     media::BindToCurrentLoop(std::move(init_cb))));
}

void StreamTextureWrapperImpl::InitializeOnMainThread(
    const base::RepeatingClosure& received_frame_cb,
    StreamTextureWrapperInitCB init_cb) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DVLOG(2) << __func__;

  // Normally, we have a factory.  However, if the gpu process is restarting,
  // then we might not.
  if (!factory_) {
    std::move(init_cb).Run(false);
    return;
  }

  stream_texture_proxy_ = factory_->CreateProxy();
  if (!stream_texture_proxy_) {
    std::move(init_cb).Run(false);
    return;
  }

  // Unretained is safe here since |stream_texture_proxy_| is a scoped member of
  // the this StreamTextureWrapperImpl class which clears/resets this callback
  // before |this| is destroyed.
  stream_texture_proxy_->BindToTaskRunner(
      received_frame_cb,
      base::BindRepeating(&StreamTextureWrapperImpl::CreateVideoFrame,
                          base::Unretained(this)),
      compositor_task_runner_);

  std::move(init_cb).Run(true);
}

void StreamTextureWrapperImpl::Destroy() {
  if (!main_task_runner_->BelongsToCurrentThread()) {
    // base::Unretained is safe here because this function is the only one that
    // can call delete.
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&StreamTextureWrapperImpl::Destroy,
                                  base::Unretained(this)));
    return;
  }

  delete this;
}

}  // namespace content
