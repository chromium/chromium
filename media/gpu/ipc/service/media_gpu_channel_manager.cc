// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/ipc/service/media_gpu_channel_manager.h"

#include <memory>
#include <utility>

#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/param_traits_macros.h"
#include "media/base/media_switches.h"
#include "media/gpu/ipc/service/media_gpu_channel.h"

#if BUILDFLAG(IS_WIN)
#include "media/gpu/windows/d3d12_helpers.h"
#endif

namespace media {

MediaGpuChannelManager::MediaGpuChannelManager(
    gpu::GpuChannelManager* channel_manager)
    : channel_manager_(channel_manager) {}

MediaGpuChannelManager::~MediaGpuChannelManager() = default;

void MediaGpuChannelManager::AddChannel(
    int32_t client_id,
    const base::UnguessableToken& channel_token) {
  gpu::GpuChannel* gpu_channel = channel_manager_->LookupChannel(client_id);
  DCHECK(gpu_channel);
  auto media_gpu_channel =
      std::make_unique<MediaGpuChannel>(gpu_channel, overlay_factory_cb_);
  media_gpu_channels_[client_id] = std::move(media_gpu_channel);
  channel_to_token_[client_id] = channel_token;
  token_to_channel_[channel_token] = client_id;
}

void MediaGpuChannelManager::RemoveChannel(int32_t client_id) {
  media_gpu_channels_.erase(client_id);
  const auto it = channel_to_token_.find(client_id);
  if (it != channel_to_token_.end()) {
    token_to_channel_.erase(it->second);
    channel_to_token_.erase(it);
  }
}

void MediaGpuChannelManager::DestroyAllChannels() {
  media_gpu_channels_.clear();
  token_to_channel_.clear();
  channel_to_token_.clear();
}

gpu::GpuChannel* MediaGpuChannelManager::LookupChannel(
    const base::UnguessableToken& channel_token) {
  const auto it = token_to_channel_.find(channel_token);
  if (it == token_to_channel_.end())
    return nullptr;
  return channel_manager_->LookupChannel(it->second);
}

void MediaGpuChannelManager::SetOverlayFactory(
    AndroidOverlayMojoFactoryCB overlay_factory_cb) {
  overlay_factory_cb_ = std::move(overlay_factory_cb);
}

AndroidOverlayMojoFactoryCB MediaGpuChannelManager::GetOverlayFactory() {
  return overlay_factory_cb_;
}

scoped_refptr<gpu::SharedContextState>
MediaGpuChannelManager::GetSharedContextState() {
  // FIXME: Should we be checking `result` == SUCCESS?
  gpu::ContextResult result;
  return channel_manager_->GetSharedContextState(&result);
}

}  // namespace media
