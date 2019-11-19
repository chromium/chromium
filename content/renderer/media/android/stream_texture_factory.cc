// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/android/stream_texture_factory.h"

#include "base/bind.h"
#include "base/macros.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "ui/gfx/geometry/size.h"

namespace content {

StreamTextureProxy::StreamTextureProxy(std::unique_ptr<StreamTextureHost> host)
    : host_(std::move(host)) {}

StreamTextureProxy::~StreamTextureProxy() {}

void StreamTextureProxy::Release() {
  // Cannot call |received_frame_cb_| after returning from here.
  ClearReceivedFrameCB();

  // |this| can be deleted by the |task_runner_| on the compositor thread by
  // posting task to that thread. So we need to clear the |set_ycbcr_info_cb_|
  // here first so that its not called on the compositor thread before |this| is
  // deleted. The problem is that |set_ycbcr_info_cb_| is provided by the owner
  // of StreamTextureProxy, which is being destroyed and is releasing
  // StreamTextureProxy.
  ClearSetYcbcrInfoCB();

  // Release is analogous to the destructor, so there should be no more external
  // calls to this object in Release. Therefore there is no need to acquire the
  // lock to access |task_runner_|.
  if (!task_runner_.get() || task_runner_->BelongsToCurrentThread() ||
      !task_runner_->DeleteSoon(FROM_HERE, this)) {
    delete this;
  }
}

void StreamTextureProxy::ClearReceivedFrameCB() {
  base::AutoLock lock(lock_);
  received_frame_cb_.Reset();
}

void StreamTextureProxy::ClearSetYcbcrInfoCB() {
  base::AutoLock lock(lock_);
  set_ycbcr_info_cb_.Reset();
}

void StreamTextureProxy::BindToTaskRunner(
    const base::RepeatingClosure& received_frame_cb,
    SetYcbcrInfoCb set_ycbcr_info_cb,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(task_runner.get());

  {
    base::AutoLock lock(lock_);
    DCHECK(!task_runner_.get() || (task_runner.get() == task_runner_.get()));
    task_runner_ = task_runner;
    received_frame_cb_ = received_frame_cb;
    set_ycbcr_info_cb_ = std::move(set_ycbcr_info_cb);
  }

  if (task_runner->BelongsToCurrentThread()) {
    BindOnThread();
    return;
  }
  // Unretained is safe here only because the object is deleted on |loop_|
  // thread.
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(&StreamTextureProxy::BindOnThread,
                                       base::Unretained(this)));
}

void StreamTextureProxy::BindOnThread() {
  host_->BindToCurrentThread(this);
}

void StreamTextureProxy::OnFrameAvailable() {
  base::AutoLock lock(lock_);
  if (!received_frame_cb_.is_null())
    received_frame_cb_.Run();
}

void StreamTextureProxy::OnFrameWithYcbcrInfoAvailable(
    base::Optional<gpu::VulkanYCbCrInfo> ycbcr_info) {
  base::AutoLock lock(lock_);
  // Set the ycbcr info before running the received frame callback so that the
  // first frame has it.
  if (!set_ycbcr_info_cb_.is_null())
    std::move(set_ycbcr_info_cb_).Run(std::move(ycbcr_info));
  if (!received_frame_cb_.is_null())
    received_frame_cb_.Run();
}

void StreamTextureProxy::ForwardStreamTextureForSurfaceRequest(
    const base::UnguessableToken& request_token) {
  host_->ForwardStreamTextureForSurfaceRequest(request_token);
}

void StreamTextureProxy::CreateSharedImage(
    const gfx::Size& size,
    gpu::Mailbox* mailbox,
    gpu::SyncToken* unverified_sync_token) {
  *mailbox = host_->CreateSharedImage(size);
  if (mailbox->IsZero())
    return;
  *unverified_sync_token = host_->GenUnverifiedSyncToken();
}

// static
scoped_refptr<StreamTextureFactory> StreamTextureFactory::Create(
    scoped_refptr<gpu::GpuChannelHost> channel) {
  return new StreamTextureFactory(std::move(channel));
}

StreamTextureFactory::StreamTextureFactory(
    scoped_refptr<gpu::GpuChannelHost> channel)
    : channel_(std::move(channel)) {
  DCHECK(channel_);
}

StreamTextureFactory::~StreamTextureFactory() = default;

ScopedStreamTextureProxy StreamTextureFactory::CreateProxy() {
  int32_t route_id = CreateStreamTexture();
  if (!route_id)
    return ScopedStreamTextureProxy();
  return ScopedStreamTextureProxy(new StreamTextureProxy(
      std::make_unique<StreamTextureHost>(channel_, route_id)));
}

bool StreamTextureFactory::IsLost() const {
  return channel_->IsLost();
}

unsigned StreamTextureFactory::CreateStreamTexture() {
  int32_t stream_id = channel_->GenerateRouteID();
  bool succeeded = false;
  channel_->Send(new GpuChannelMsg_CreateStreamTexture(stream_id, &succeeded));
  if (!succeeded) {
    DLOG(ERROR) << "GpuChannelMsg_CreateStreamTexture returned failure";
    return 0;
  }
  return stream_id;
}

gpu::SharedImageInterface* StreamTextureFactory::SharedImageInterface() {
  return channel_->shared_image_interface();
}

}  // namespace content
