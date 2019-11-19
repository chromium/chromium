// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_ANDROID_STREAM_TEXTURE_FACTORY_H_
#define CONTENT_RENDERER_MEDIA_ANDROID_STREAM_TEXTURE_FACTORY_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/unguessable_token.h"
#include "cc/layers/video_frame_provider.h"
#include "content/common/content_export.h"
#include "content/renderer/stream_texture_host_android.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "ui/gfx/geometry/size.h"

namespace gpu {
class GpuChannelHost;
class SharedImageInterface;
struct SyncToken;
struct VulkanYCbCrInfo;
}  // namespace gpu

namespace content {

class StreamTextureFactory;

// The proxy class for the gpu thread to notify the compositor thread
// when a new video frame is available.
class CONTENT_EXPORT StreamTextureProxy : public StreamTextureHost::Listener {
 public:
  using SetYcbcrInfoCb =
      base::OnceCallback<void(base::Optional<gpu::VulkanYCbCrInfo>)>;

  ~StreamTextureProxy() override;

  // Initialize and bind to |task_runner|, which becomes the thread that the
  // provided callback will be run on. This can be called on any thread, but
  // must be called with the same |task_runner| every time.
  void BindToTaskRunner(
      const base::RepeatingClosure& received_frame_cb,
      SetYcbcrInfoCb set_ycbcr_info_cb,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // StreamTextureHost::Listener implementation:
  void OnFrameAvailable() override;
  void OnFrameWithYcbcrInfoAvailable(
      base::Optional<gpu::VulkanYCbCrInfo> ycbcr_info) override;

  // Sends an IPC to the GPU process.
  // Asks the StreamTexture to forward its SurfaceTexture to the
  // ScopedSurfaceRequestManager, using the gpu::ScopedSurfaceRequestConduit.
  void ForwardStreamTextureForSurfaceRequest(
      const base::UnguessableToken& request_token);

  // Creates a SharedImage for the provided texture size. Returns the
  // |mailbox| for the SharedImage, as well as an |unverified_sync_token|
  // representing SharedImage creation.
  // If creation fails, returns an empty |mailbox| and does not modify
  // |unverified_sync_token|.
  void CreateSharedImage(const gfx::Size& size,
                         gpu::Mailbox* mailbox,
                         gpu::SyncToken* unverified_sync_token);

  // Clears |received_frame_cb_| in a thread safe way.
  void ClearReceivedFrameCB();

  struct Deleter {
    inline void operator()(StreamTextureProxy* ptr) const { ptr->Release(); }
  };
 private:
  friend class StreamTextureFactory;
  friend class StreamTextureProxyTest;
  explicit StreamTextureProxy(std::unique_ptr<StreamTextureHost> host);

  void BindOnThread();
  void Release();

  // Clears |set_ycbcr_info_cb_| in a thread safe way.
  void ClearSetYcbcrInfoCB();

  const std::unique_ptr<StreamTextureHost> host_;

  // Protects access to |received_frame_cb_| and |task_runner_|.
  base::Lock lock_;
  base::RepeatingClosure received_frame_cb_;
  SetYcbcrInfoCb set_ycbcr_info_cb_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(StreamTextureProxy);
};

typedef std::unique_ptr<StreamTextureProxy, StreamTextureProxy::Deleter>
    ScopedStreamTextureProxy;

// Factory class for managing stream textures.
class CONTENT_EXPORT StreamTextureFactory
    : public base::RefCounted<StreamTextureFactory> {
 public:
  static scoped_refptr<StreamTextureFactory> Create(
      scoped_refptr<gpu::GpuChannelHost> channel);

  // Create the StreamTextureProxy object. This internally creates a
  // gpu::StreamTexture and returns its route_id. If this route_id is invalid
  // nullptr is returned. If the route_id is valid it returns
  // StreamTextureProxy object.
  ScopedStreamTextureProxy CreateProxy();

  // Returns true if the StreamTextureFactory's channel is lost.
  bool IsLost() const;

  gpu::SharedImageInterface* SharedImageInterface();

 private:
  friend class base::RefCounted<StreamTextureFactory>;
  StreamTextureFactory(scoped_refptr<gpu::GpuChannelHost> channel);
  ~StreamTextureFactory();
  // Creates a gpu::StreamTexture and returns its id.
  unsigned CreateStreamTexture();

  scoped_refptr<gpu::GpuChannelHost> channel_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(StreamTextureFactory);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_ANDROID_STREAM_TEXTURE_FACTORY_H_
