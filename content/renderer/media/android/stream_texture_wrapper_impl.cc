// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/android/stream_texture_wrapper_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "cc/layers/video_frame_provider.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
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

void StreamTextureWrapperImpl::ReallocateVideoFrame() {
  DVLOG(2) << __func__;
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  gpu::Mailbox mailbox;
  gpu::SyncToken texture_mailbox_sync_token;
  stream_texture_proxy_->CreateSharedImage(natural_size_, &mailbox,
                                           &texture_mailbox_sync_token);
  gpu::MailboxHolder holders[media::VideoFrame::kMaxPlanes] = {
      gpu::MailboxHolder(mailbox, texture_mailbox_sync_token,
                         GL_TEXTURE_EXTERNAL_OES)};

  scoped_refptr<media::VideoFrame> new_frame =
      media::VideoFrame::WrapNativeTextures(
          media::PIXEL_FORMAT_ARGB, holders,
          media::BindToCurrentLoop(
              base::BindOnce(&OnReleaseVideoFrame, factory_, mailbox)),
          natural_size_, gfx::Rect(natural_size_), natural_size_,
          base::TimeDelta());
  new_frame->set_ycbcr_info(ycbcr_info_);

  if (enable_texture_copy_) {
    new_frame->metadata()->SetBoolean(media::VideoFrameMetadata::COPY_REQUIRED,
                                      true);
  }

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

void StreamTextureWrapperImpl::SetYcbcrInfo(
    base::Optional<gpu::VulkanYCbCrInfo> ycbcr_info) {
  DCHECK(!ycbcr_info_);

  current_frame_->set_ycbcr_info(ycbcr_info);
  ycbcr_info_ = std::move(ycbcr_info);
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

  if (natural_size_ == new_size)
    return;

  natural_size_ = new_size;
  ReallocateVideoFrame();
}

void StreamTextureWrapperImpl::Initialize(
    const base::RepeatingClosure& received_frame_cb,
    const gfx::Size& natural_size,
    scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
    const StreamTextureWrapperInitCB& init_cb) {
  DVLOG(2) << __func__;

  compositor_task_runner_ = compositor_task_runner;
  natural_size_ = natural_size;

  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&StreamTextureWrapperImpl::InitializeOnMainThread,
                     weak_factory_.GetWeakPtr(), received_frame_cb,
                     media::BindToCurrentLoop(init_cb)));
}

void StreamTextureWrapperImpl::InitializeOnMainThread(
    const base::RepeatingClosure& received_frame_cb,
    const StreamTextureWrapperInitCB& init_cb) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DVLOG(2) << __func__;

  // Normally, we have a factory.  However, if the gpu process is restarting,
  // then we might not.
  if (!factory_) {
    init_cb.Run(false);
    return;
  }

  stream_texture_proxy_ = factory_->CreateProxy();
  if (!stream_texture_proxy_) {
    init_cb.Run(false);
    return;
  }

  ReallocateVideoFrame();

  // Unretained is safe here since |stream_texture_proxy_| is a scoped member of
  // the this StreamTextureWrapperImpl class which clears/resets this callback
  // before |this| is destroyed.
  stream_texture_proxy_->BindToTaskRunner(
      received_frame_cb,
      base::BindOnce(&StreamTextureWrapperImpl::SetYcbcrInfo,
                     base::Unretained(this)),
      compositor_task_runner_);

  init_cb.Run(true);
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
