// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/stream_texture_host_android.h"

#include "base/unguessable_token.h"
#include "content/renderer/render_thread_impl.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/common/gpu_channel.mojom.h"
#include "gpu/ipc/common/vulkan_ycbcr_info.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_mojo_bootstrap.h"

namespace content {

StreamTextureHost::StreamTextureHost(
    scoped_refptr<gpu::GpuChannelHost> channel,
    int32_t route_id,
    mojo::PendingAssociatedRemote<gpu::mojom::StreamTexture> texture)
    : route_id_(route_id),
      listener_(nullptr),
      channel_(std::move(channel)),
      pending_texture_(std::move(texture)) {
  DCHECK(channel_);
  DCHECK(route_id_);
}

StreamTextureHost::~StreamTextureHost() {
  if (channel_) {
    // We destroy the StreamTexture as a deferred message followed by a flush
    // to ensure this is ordered correctly with regards to previous deferred
    // messages, such as CreateSharedImage.
    uint32_t flush_id = channel_->EnqueueDeferredMessage(
        gpu::mojom::DeferredRequestParams::NewDestroyStreamTexture(route_id_),
        /*sync_token_fences=*/{}, /*release_count=*/0);
    channel_->EnsureFlush(flush_id);
  }
}

bool StreamTextureHost::BindToCurrentThread(Listener* listener) {
  listener_ = listener;
  if (!pending_texture_)
    return false;

  // Disable artificial limitations of Channel-associated interface bindings.
  // Normally such interfaces can only be bound on the main or IO threads to
  // prevent various classes of bugs that can arise from a Channel-associated
  // endpoint being bound too late to receive messages already sent to it.
  //
  // Without this scoped allowance, binding from the compositor thread will
  // actually still bind to the main thread, leading to potential memory bugs
  // and other racy behavior since `this` is still destroyed on the compositor
  // thread.
  //
  // The allowance is safe because binding these endpoints to the compositor
  // thread cannot cause such late-binding bugs: remotes can only receive
  // replies, but `texture_remote_` can't have made any calls yet; and
  // `receiver_` doesn't even have a corresponding remote to call into it until
  // the StartListening() line below.
  //
  // SUBTLE: If any message on StreamTextureClient (or any reply on
  // StreamTexture) were to be added which accepts another associated interface
  // endpoint, *that* could render this all unsafe since the new endpoint might
  // get bound too late.
  IPC::ScopedAllowOffSequenceChannelAssociatedBindings allow_off_thread_binding;

  texture_remote_.Bind(std::move(pending_texture_));
  texture_remote_->StartListening(receiver_.BindNewEndpointAndPassRemote());
  texture_remote_.set_disconnect_handler(
      base::BindOnce(&StreamTextureHost::OnDisconnectedFromGpuProcess,
                     base::Unretained(this)));
  return true;
}

void StreamTextureHost::OnDisconnectedFromGpuProcess() {
  channel_ = nullptr;
  texture_remote_.reset();
  receiver_.reset();
}

void StreamTextureHost::OnFrameWithInfoAvailable(
    const gpu::Mailbox& mailbox,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    std::optional<gpu::VulkanYCbCrInfo> ycbcr_info) {
  if (listener_) {
    listener_->OnFrameWithInfoAvailable(mailbox, coded_size, visible_rect,
                                        ycbcr_info);
  }
}

void StreamTextureHost::OnFrameAvailable() {
  if (listener_)
    listener_->OnFrameAvailable();
}

void StreamTextureHost::ForwardStreamTextureForSurfaceRequest(
    const base::UnguessableToken& request_token) {
  if (texture_remote_)
    texture_remote_->ForwardForSurfaceRequest(request_token);
}

void StreamTextureHost::UpdateRotatedVisibleSize(const gfx::Size& size) {
  if (texture_remote_)
    texture_remote_->UpdateRotatedVisibleSize(size);
}

}  // namespace content
