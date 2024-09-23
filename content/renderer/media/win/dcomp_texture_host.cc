// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/win/dcomp_texture_host.h"

#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/unguessable_token.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "gpu/ipc/common/gpu_channel.mojom.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_mojo_bootstrap.h"
#include "media/base/win/mf_helpers.h"

namespace content {

DCOMPTextureHost::DCOMPTextureHost(
    scoped_refptr<gpu::GpuChannelHost> channel,
    int32_t route_id,
    scoped_refptr<base::SequencedTaskRunner> media_task_runner,
    mojo::PendingAssociatedRemote<gpu::mojom::DCOMPTexture> texture,
    Listener* listener)
    : channel_(std::move(channel)), route_id_(route_id), listener_(listener) {
  DVLOG_FUNC(1);
  DCHECK(media_task_runner->RunsTasksInCurrentSequence());
  DCHECK(channel_);
  DCHECK(route_id_);
  DCHECK(listener_);

  // `allow_binding` is needed to make sure `texture_remote_` and `receiver_`
  // are both bound on `media_task_runner`. See crbug.com/1229833.
  IPC::ScopedAllowOffSequenceChannelAssociatedBindings allow_binding;
  texture_remote_.Bind(std::move(texture), media_task_runner);
  texture_remote_->StartListening(
      receiver_.BindNewEndpointAndPassRemote(media_task_runner));
  texture_remote_.set_disconnect_handler(base::BindOnce(
      &DCOMPTextureHost::OnDisconnectedFromGpuProcess, base::Unretained(this)));
}

DCOMPTextureHost::~DCOMPTextureHost() {
  DVLOG_FUNC(1);

  if (!channel_)
    return;

  // We destroy the DCOMPTexture as a deferred message followed by a flush
  // to ensure this is ordered correctly with regards to previous deferred
  // messages, such as CreateSharedImage.
  uint32_t flush_id = channel_->EnqueueDeferredMessage(
      gpu::mojom::DeferredRequestParams::NewDestroyDcompTexture(route_id_),
      /*sync_token_fences=*/{}, /*release_count=*/0);
  channel_->EnsureFlush(flush_id);
}

void DCOMPTextureHost::SetTextureSize(const gfx::Size& size) {
  DVLOG_FUNC(3);
  if (texture_remote_)
    texture_remote_->SetTextureSize(size);
}

void DCOMPTextureHost::SetDCOMPSurfaceHandle(
    const base::UnguessableToken& token,
    SetDCOMPSurfaceHandleCB set_dcomp_surface_handle_cb) {
  DVLOG_FUNC(1);
  if (texture_remote_) {
    texture_remote_->SetDCOMPSurfaceHandle(
        token, std::move(set_dcomp_surface_handle_cb));
  }
}

void DCOMPTextureHost::OnDisconnectedFromGpuProcess() {
  DVLOG_FUNC(1);
  channel_ = nullptr;
  texture_remote_.reset();
  receiver_.reset();
}

void DCOMPTextureHost::OnSharedImageMailboxBound(const gpu::Mailbox& mailbox) {
  DVLOG_FUNC(1);
  listener_->OnSharedImageMailboxBound(mailbox);
}

void DCOMPTextureHost::OnOutputRectChange(const gfx::Rect& output_rect) {
  DVLOG_FUNC(3);
  listener_->OnOutputRectChange(output_rect);
}

}  // namespace content
