// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/win/overlay_state_service_provider.h"

#include "gpu/ipc/client/gpu_channel_host.h"
#include "media/base/win/mf_helpers.h"

namespace content {

OverlayStateServiceProviderImpl::OverlayStateServiceProviderImpl(
    scoped_refptr<gpu::GpuChannelHost> channel)
    : channel_(std::move(channel)) {
  DVLOG_FUNC(1);
  DCHECK(channel_);
}

OverlayStateServiceProviderImpl::~OverlayStateServiceProviderImpl() {}

bool OverlayStateServiceProviderImpl::RegisterObserver(
    mojo::PendingRemote<gpu::mojom::OverlayStateObserver> pending_remote,
    const gpu::Mailbox& mailbox) {
  DVLOG_FUNC(1);
  bool succeeded = false;

  channel_->GetGpuChannel().RegisterOverlayStateObserver(
      std::move(pending_remote), std::move(mailbox), &succeeded);
  if (!succeeded)
    DLOG(ERROR) << "RegisterOverlayStateObserver failed";

  return succeeded;
}

bool OverlayStateServiceProviderImpl::IsLost() const {
  return channel_->IsLost();
}

}  // namespace content
