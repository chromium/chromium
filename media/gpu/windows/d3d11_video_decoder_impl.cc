// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d11_video_decoder_impl.h"

#include "base/bind.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "media/base/media_log.h"
#include "media/gpu/windows/d3d11_picture_buffer.h"

namespace media {

D3D11VideoDecoderImpl::D3D11VideoDecoderImpl(
    std::unique_ptr<MediaLog> media_log,
    base::RepeatingCallback<scoped_refptr<CommandBufferHelper>()> get_helper_cb)
    : media_log_(std::move(media_log)),
      get_helper_cb_(std::move(get_helper_cb)) {
  // May be called from any thread.
}

D3D11VideoDecoderImpl::~D3D11VideoDecoderImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void D3D11VideoDecoderImpl::Initialize(
    InitCB init_cb,
    ReturnPictureBufferCB return_picture_buffer_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  return_picture_buffer_cb_ = std::move(return_picture_buffer_cb);

  // If have a helper, then we're as initialized as we need to be.
  if (helper_) {
    std::move(init_cb).Run(true);
    return;
  }
  helper_ = get_helper_cb_.Run();

  // Get the stub, register, and generally do stuff.
  if (!helper_ || !helper_->MakeContextCurrent()) {
    const char* reason = "Failed to make context current.";
    DLOG(ERROR) << reason;
    if (media_log_) {
      media_log_->AddEvent(media_log_->CreateStringEvent(
          MediaLogEvent::MEDIA_ERROR_LOG_ENTRY, "error", reason));
    }
    std::move(init_cb).Run(false);
    return;
  }

  std::move(init_cb).Run(true);
}

void D3D11VideoDecoderImpl::OnMailboxReleased(
    scoped_refptr<D3D11PictureBuffer> buffer,
    const gpu::SyncToken& sync_token) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!helper_)
    return;

  helper_->WaitForSyncToken(
      sync_token, base::BindOnce(&D3D11VideoDecoderImpl::OnSyncTokenReleased,
                                 GetWeakPtr(), std::move(buffer)));
}

void D3D11VideoDecoderImpl::OnSyncTokenReleased(
    scoped_refptr<D3D11PictureBuffer> buffer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  return_picture_buffer_cb_.Run(std::move(buffer));
}

base::WeakPtr<D3D11VideoDecoderImpl> D3D11VideoDecoderImpl::GetWeakPtr() {
  // May be called from any thread.
  return weak_factory_.GetWeakPtr();
}

}  // namespace media
