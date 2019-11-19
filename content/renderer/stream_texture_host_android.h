// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_STREAM_TEXTURE_HOST_ANDROID_H_
#define CONTENT_RENDERER_STREAM_TEXTURE_HOST_ANDROID_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_message.h"

namespace base {
class UnguessableToken;
}

namespace gfx {
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
class CONTENT_EXPORT StreamTextureHost : public IPC::Listener {
 public:
  explicit StreamTextureHost(scoped_refptr<gpu::GpuChannelHost> channel,
                             int32_t route_id);
  ~StreamTextureHost() override;

  // Listener class that is listening to the stream texture updates. It is
  // implemented by StreamTextureProxyImpl.
  class Listener {
   public:
    virtual void OnFrameAvailable() = 0;
    virtual void OnFrameWithYcbcrInfoAvailable(
        base::Optional<gpu::VulkanYCbCrInfo> ycbcr_info) = 0;
    virtual ~Listener() {}
  };

  bool BindToCurrentThread(Listener* listener);

  // IPC::Channel::Listener implementation:
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnChannelError() override;

  void ForwardStreamTextureForSurfaceRequest(
      const base::UnguessableToken& request_token);
  gpu::Mailbox CreateSharedImage(const gfx::Size& size);
  gpu::SyncToken GenUnverifiedSyncToken();

 private:
  // Message handlers:
  void OnFrameAvailable();
  void OnFrameWithYcbcrInfoAvailable(
      base::Optional<gpu::VulkanYCbCrInfo> ycbcr_info);

  int32_t route_id_;
  Listener* listener_;
  scoped_refptr<gpu::GpuChannelHost> channel_;
  uint32_t release_id_ = 0;

  base::WeakPtrFactory<StreamTextureHost> weak_ptr_factory_{this};

  DISALLOW_IMPLICIT_CONSTRUCTORS(StreamTextureHost);
};

}  // namespace content

#endif  // CONTENT_RENDERER_STREAM_TEXTURE_HOST_ANDROID_H_
