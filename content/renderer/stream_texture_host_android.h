// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_STREAM_TEXTURE_HOST_ANDROID_H_
#define CONTENT_RENDERER_STREAM_TEXTURE_HOST_ANDROID_H_

#include <stdint.h>

#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "gpu/ipc/common/gpu_channel.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

namespace base {
class UnguessableToken;
}

namespace gfx {
class Rect;
class Size;
}

namespace gpu {
class GpuChannelHost;
struct Mailbox;
struct SyncToken;
struct VulkanYCbCrInfo;
}

namespace content {

// Class for handling all the IPC messages between the GPU process and
// StreamTextureProxy.
class CONTENT_EXPORT StreamTextureHost
    : public gpu::mojom::StreamTextureClient {
 public:
  StreamTextureHost() = delete;

  explicit StreamTextureHost(
      scoped_refptr<gpu::GpuChannelHost> channel,
      int32_t route_id,
      mojo::PendingAssociatedRemote<gpu::mojom::StreamTexture> texture);

  StreamTextureHost(const StreamTextureHost&) = delete;
  StreamTextureHost& operator=(const StreamTextureHost&) = delete;

  ~StreamTextureHost() override;

  // Listener class that is listening to the stream texture updates. It is
  // implemented by StreamTextureProxyImpl.
  class Listener {
   public:
    virtual void OnFrameAvailable() = 0;
    virtual void OnFrameWithInfoAvailable(
        const gpu::Mailbox& mailbox,
        const gfx::Size& coded_size,
        const gfx::Rect& visible_rect,
        const absl::optional<gpu::VulkanYCbCrInfo>& ycbcr_info) = 0;
    virtual ~Listener() {}
  };

  bool BindToCurrentThread(Listener* listener);
  void OnDisconnectedFromGpuProcess();

  void ForwardStreamTextureForSurfaceRequest(
      const base::UnguessableToken& request_token);
  void UpdateRotatedVisibleSize(const gfx::Size& size);
  gpu::SyncToken GenUnverifiedSyncToken();

 private:
  // gpu::mojom::StreamTextureClient:
  void OnFrameAvailable() override;
  void OnFrameWithInfoAvailable(
      const gpu::Mailbox& mailbox,
      const gfx::Size& coded_size,
      const gfx::Rect& visible_rect,
      absl::optional<gpu::VulkanYCbCrInfo> ycbcr_info) override;

  int32_t route_id_;
  Listener* listener_;
  scoped_refptr<gpu::GpuChannelHost> channel_;
  uint32_t release_id_ = 0;

  // The StreamTextureHost may be created on another thread, but the Mojo
  // endpoints below are to be bound on the compositor thread. This holds the
  // pending renderer-side StreamTexture endpoint until it can be bound on the
  // compositor thread.
  mojo::PendingAssociatedRemote<gpu::mojom::StreamTexture> pending_texture_;

  // Receives client messages from a corresponding StreamTexture instance in
  // the GPU process.
  mojo::AssociatedReceiver<gpu::mojom::StreamTextureClient> receiver_{this};

  // Sends messages to a corresponding StreamTexture instance in the GPU
  // process.
  mojo::AssociatedRemote<gpu::mojom::StreamTexture> texture_remote_;

  base::WeakPtrFactory<StreamTextureHost> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_RENDERER_STREAM_TEXTURE_HOST_ANDROID_H_
