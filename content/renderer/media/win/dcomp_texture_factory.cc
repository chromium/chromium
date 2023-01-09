// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/win/dcomp_texture_factory.h"

#include "base/task/sequenced_task_runner.h"
#include "gpu/ipc/client/client_shared_image_interface.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "media/base/win/mf_helpers.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "ui/gfx/geometry/size.h"

namespace content {

// static
scoped_refptr<DCOMPTextureFactory> DCOMPTextureFactory::Create(
    scoped_refptr<gpu::GpuChannelHost> channel,
    scoped_refptr<base::SequencedTaskRunner> media_task_runner) {
  DVLOG(1) << __func__;
  return WrapRefCounted(
      new DCOMPTextureFactory(std::move(channel), media_task_runner));
}

DCOMPTextureFactory::DCOMPTextureFactory(
    scoped_refptr<gpu::GpuChannelHost> channel,
    scoped_refptr<base::SequencedTaskRunner> media_task_runner)
    : channel_(std::move(channel)), media_task_runner_(media_task_runner) {
  DVLOG_FUNC(1);
  DCHECK(channel_);
}

DCOMPTextureFactory::~DCOMPTextureFactory() = default;

std::unique_ptr<DCOMPTextureHost> DCOMPTextureFactory::CreateDCOMPTextureHost(
    DCOMPTextureHost::Listener* listener) {
  DVLOG_FUNC(1);
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  int32_t route_id = channel_->GenerateRouteID();
  mojo::PendingAssociatedRemote<gpu::mojom::DCOMPTexture> remote;
  bool succeeded = false;

  mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync;
  // Creates a DCOMPTexture in the GPU process.
  channel_->GetGpuChannel().CreateDCOMPTexture(
      route_id, remote.InitWithNewEndpointAndPassReceiver(), &succeeded);
  if (!succeeded) {
    DLOG(ERROR) << "CreateDCOMPTexture failed";
    return nullptr;
  }

  return std::make_unique<DCOMPTextureHost>(
      channel_, route_id, media_task_runner_, std::move(remote), listener);
}

bool DCOMPTextureFactory::IsLost() const {
  return channel_->IsLost();
}

gpu::SharedImageInterface* DCOMPTextureFactory::SharedImageInterface() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  if (!shared_image_interface_)
    shared_image_interface_ = channel_->CreateClientSharedImageInterface();

  DCHECK(shared_image_interface_);
  return shared_image_interface_.get();
}

}  // namespace content
