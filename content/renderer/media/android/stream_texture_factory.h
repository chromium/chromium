// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_ANDROID_STREAM_TEXTURE_FACTORY_H_
#define CONTENT_RENDERER_MEDIA_ANDROID_STREAM_TEXTURE_FACTORY_H_

#include <stdint.h>

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "base/unguessable_token.h"
#include "cc/layers/video_frame_provider.h"
#include "content/common/content_export.h"
#include "content/renderer/stream_texture_host_android.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/ipc/common/gpu_channel.mojom.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace gpu {
class ClientSharedImageInterface;
class GpuChannelHost;
class SharedImageInterface;
struct VulkanYCbCrInfo;
}  // namespace gpu

namespace content {

class StreamTextureFactory;

// The proxy class for the gpu thread to notify the compositor thread
// when a new video frame is available.
class CONTENT_EXPORT StreamTextureProxy : public StreamTextureHost::Listener {
 public:
  using CreateVideoFrameCB =
      base::RepeatingCallback<void(const gpu::Mailbox& mailbox,
                                   const gfx::Size& coded_size,
                                   const gfx::Rect& visible_rect,
                                   const std::optional<gpu::VulkanYCbCrInfo>&)>;

  StreamTextureProxy() = delete;
  StreamTextureProxy(const StreamTextureProxy&) = delete;
  StreamTextureProxy& operator=(const StreamTextureProxy&) = delete;

  ~StreamTextureProxy() override;

  // Initialize and bind to |task_runner|, which becomes the thread that the
  // provided callback will be run on. This can be called on any thread, but
  // must be called with the same |task_runner| every time.
  void BindToTaskRunner(
      const base::RepeatingClosure& received_frame_cb,
      const CreateVideoFrameCB& create_video_frame_cb,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // StreamTextureHost::Listener implementation:
  void OnFrameAvailable() override;
  void OnFrameWithInfoAvailable(
      const gpu::Mailbox& mailbox,
      const gfx::Size& coded_size,
      const gfx::Rect& visible_rect,
      const std::optional<gpu::VulkanYCbCrInfo>& ycbcr_info) override;

  // Sends an IPC to the GPU process.
  // Asks the StreamTexture to forward its SurfaceTexture to the
  // ScopedSurfaceRequestManager, using the gpu::ScopedSurfaceRequestConduit.
  void ForwardStreamTextureForSurfaceRequest(
      const base::UnguessableToken& request_token);

  // Notifies StreamTexture that video size has been changed and so it can
  // recreate shared image.
  void UpdateRotatedVisibleSize(const gfx::Size& size);

  // Clears |received_frame_cb_| in a thread safe way.
  void ClearReceivedFrameCB();

  // Clears |create_video_frame_cb_| in a thread safe way.
  void ClearCreateVideoFrameCB();

  struct Deleter {
    inline void operator()(StreamTextureProxy* ptr) const { ptr->Release(); }
  };
 private:
  friend class StreamTextureFactory;
  friend class StreamTextureProxyTest;
  explicit StreamTextureProxy(std::unique_ptr<StreamTextureHost> host);

  void BindOnThread();
  void Release();

  const std::unique_ptr<StreamTextureHost> host_;

  // Protects access to |received_frame_cb_| and |task_runner_|.
  base::Lock lock_;
  base::RepeatingClosure received_frame_cb_;
  CreateVideoFrameCB create_video_frame_cb_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

typedef std::unique_ptr<StreamTextureProxy, StreamTextureProxy::Deleter>
    ScopedStreamTextureProxy;

// Factory class for managing stream textures.
class CONTENT_EXPORT StreamTextureFactory
    : public base::RefCountedThreadSafe<StreamTextureFactory> {
 public:
  static scoped_refptr<StreamTextureFactory> Create(
      scoped_refptr<gpu::GpuChannelHost> channel);

  StreamTextureFactory() = delete;
  StreamTextureFactory(const StreamTextureFactory&) = delete;
  StreamTextureFactory& operator=(const StreamTextureFactory&) = delete;

  // Create the StreamTextureProxy object. This internally creates a
  // gpu::StreamTexture and returns its route_id. If this route_id is invalid
  // nullptr is returned. If the route_id is valid it returns
  // StreamTextureProxy object.
  ScopedStreamTextureProxy CreateProxy();

  // Returns true if the StreamTextureFactory's channel is lost.
  bool IsLost() const;

  gpu::SharedImageInterface* SharedImageInterface();

 private:
  friend class base::RefCountedThreadSafe<StreamTextureFactory>;
  StreamTextureFactory(scoped_refptr<gpu::GpuChannelHost> channel);
  ~StreamTextureFactory();

  scoped_refptr<gpu::GpuChannelHost> channel_;
  scoped_refptr<gpu::ClientSharedImageInterface> shared_image_interface_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_ANDROID_STREAM_TEXTURE_FACTORY_H_
