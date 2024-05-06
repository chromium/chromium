// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_IPC_SERVICE_MEDIA_GPU_CHANNEL_MANAGER_H_
#define MEDIA_GPU_IPC_SERVICE_MEDIA_GPU_CHANNEL_MANAGER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <unordered_map>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "media/base/android_overlay_mojo_factory.h"
#include "media/video/video_decode_accelerator.h"

namespace gpu {
class GpuChannel;
class GpuChannelManager;
class SharedContextState;
}

namespace media {

class MediaGpuChannel;

class MediaGpuChannelManager final {
 public:
  explicit MediaGpuChannelManager(gpu::GpuChannelManager* channel_manager);
  MediaGpuChannelManager(const MediaGpuChannelManager&) = delete;
  MediaGpuChannelManager& operator=(const MediaGpuChannelManager&) = delete;
  ~MediaGpuChannelManager();

  void AddChannel(int32_t client_id,
                  const base::UnguessableToken& channel_token);
  void RemoveChannel(int32_t client_id);
  void DestroyAllChannels();

  void SetOverlayFactory(AndroidOverlayMojoFactoryCB overlay_factory_cb);
  AndroidOverlayMojoFactoryCB GetOverlayFactory();

  // TODO(sandersd): Should we expose the MediaGpuChannel instead?
  gpu::GpuChannel* LookupChannel(const base::UnguessableToken& channel_token);

  scoped_refptr<gpu::SharedContextState> GetSharedContextState();

  base::WeakPtr<MediaGpuChannelManager> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  const raw_ptr<gpu::GpuChannelManager> channel_manager_;
  std::unordered_map<int32_t, std::unique_ptr<MediaGpuChannel>>
      media_gpu_channels_;
  std::map<base::UnguessableToken, int32_t> token_to_channel_;
  std::map<int32_t, base::UnguessableToken> channel_to_token_;
  AndroidOverlayMojoFactoryCB overlay_factory_cb_;
  base::WeakPtrFactory<MediaGpuChannelManager> weak_ptr_factory_{this};
};

}  // namespace media

#endif  // MEDIA_GPU_IPC_SERVICE_MEDIA_GPU_CHANNEL_MANAGER_H_
