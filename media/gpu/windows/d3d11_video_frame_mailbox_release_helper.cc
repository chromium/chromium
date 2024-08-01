// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d11_video_frame_mailbox_release_helper.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/task/bind_post_task.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "media/base/media_log.h"

namespace media {

D3D11VideoFrameMailboxReleaseHelper::D3D11VideoFrameMailboxReleaseHelper(
    std::unique_ptr<MediaLog> media_log,
    base::OnceCallback<scoped_refptr<CommandBufferHelper>()> get_helper_cb)
    : media_log_(std::move(media_log)),
      get_helper_cb_(std::move(get_helper_cb)) {
  // May be called from any thread.
  DCHECK(get_helper_cb_);
  DETACH_FROM_THREAD(thread_checker_);
}

// Note: It doesn't matter which thread this is destructed on since `helper_`
// will always destruct on the GPU thread.
D3D11VideoFrameMailboxReleaseHelper::~D3D11VideoFrameMailboxReleaseHelper() =
    default;

void D3D11VideoFrameMailboxReleaseHelper::Initialize(InitCB init_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  helper_ = std::move(get_helper_cb_).Run();

  // Get the stub, register, and generally do stuff.
  if (!helper_) {
    if (media_log_) {
      MEDIA_LOG(ERROR, media_log_) << "Failed to get command buffer helper.";
    }

    std::move(init_cb).Run(false, ReleaseMailboxCB());
    return;
  }

  // Note: Since this is a RefCounted class it can't own any callbacks that are
  // bound to itself or the ref count will never reach zero and thus leak.
  std::move(init_cb).Run(
      true,
      base::BindPostTaskToCurrentDefault(base::BindRepeating(
          &D3D11VideoFrameMailboxReleaseHelper::OnMailboxReleased, this)));
}

void D3D11VideoFrameMailboxReleaseHelper::OnMailboxReleased(
    base::OnceClosure wait_complete_cb,
    const gpu::SyncToken& sync_token) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(helper_);
  helper_->WaitForSyncToken(
      sync_token,
      base::BindOnce(&D3D11VideoFrameMailboxReleaseHelper::OnSyncTokenReleased,
                     this, std::move(wait_complete_cb)));
}

void D3D11VideoFrameMailboxReleaseHelper::OnSyncTokenReleased(
    base::OnceClosure wait_complete_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  std::move(wait_complete_cb).Run();
}

}  // namespace media
