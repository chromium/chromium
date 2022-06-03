// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d11_video_decoder_impl.h"

#include "base/bind.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "media/base/bind_to_current_loop.h"
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

void D3D11VideoDecoderImpl::Initialize(InitCB init_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // If have a helper, then we're as initialized as we need to be.
  if (helper_) {
    std::move(init_cb).Run(true, release_mailbox_cb_);
    return;
  }
  helper_ = get_helper_cb_.Run();

  // Get the stub, register, and generally do stuff.
  if (!helper_ || !helper_->MakeContextCurrent()) {
    const char* reason = "Failed to make context current.";
    DLOG(ERROR) << reason;
    if (media_log_)
      MEDIA_LOG(ERROR, media_log_) << reason;

    std::move(init_cb).Run(false, ReleaseMailboxCB());
    return;
  }

  release_mailbox_cb_ = BindToCurrentLoop(base::BindRepeating(
      &D3D11VideoDecoderImpl::OnMailboxReleased, GetWeakPtr()));

  std::move(init_cb).Run(true, release_mailbox_cb_);
}

void D3D11VideoDecoderImpl::OnMailboxReleased(
    base::OnceClosure wait_complete_cb,
    const gpu::SyncToken& sync_token) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!helper_) {
    std::move(wait_complete_cb).Run();
    return;
  }

  helper_->WaitForSyncToken(
      sync_token, base::BindOnce(&D3D11VideoDecoderImpl::OnSyncTokenReleased,
                                 GetWeakPtr(), std::move(wait_complete_cb)));
}

void D3D11VideoDecoderImpl::OnSyncTokenReleased(
    base::OnceClosure wait_complete_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  std::move(wait_complete_cb).Run();
}

base::WeakPtr<D3D11VideoDecoderImpl> D3D11VideoDecoderImpl::GetWeakPtr() {
  // May be called from any thread.
  return weak_factory_.GetWeakPtr();
}

}  // namespace media
