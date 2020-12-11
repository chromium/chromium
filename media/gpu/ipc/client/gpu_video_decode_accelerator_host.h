// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_IPC_CLIENT_GPU_VIDEO_DECODE_ACCELERATOR_HOST_H_
#define MEDIA_GPU_IPC_CLIENT_GPU_VIDEO_DECODE_ACCELERATOR_HOST_H_

#include <stdint.h>

#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/single_thread_task_runner.h"
#include "gpu/ipc/client/command_buffer_proxy_impl.h"
#include "ipc/ipc_listener.h"
#include "media/video/video_decode_accelerator.h"
#include "ui/gfx/geometry/size.h"

struct AcceleratedVideoDecoderHostMsg_PictureReady_Params;

namespace gpu {
class GpuChannelHost;
}

namespace media {

// This class is used to talk to VideoDecodeAccelerator in the Gpu process
// through IPC messages.
class GpuVideoDecodeAcceleratorHost
    : public IPC::Listener,
      public VideoDecodeAccelerator,
      public gpu::CommandBufferProxyImpl::DeletionObserver {
 public:
  // |this| is guaranteed not to outlive |impl|.  (See comments for |impl_|.)
  explicit GpuVideoDecodeAcceleratorHost(gpu::CommandBufferProxyImpl* impl);

  // IPC::Listener implementation.
  void OnChannelError() override;
  bool OnMessageReceived(const IPC::Message& message) override;

  // VideoDecodeAccelerator implementation.
  bool Initialize(const Config& config, Client* client) override;
  void Decode(BitstreamBuffer bitstream_buffer) override;
  void AssignPictureBuffers(const std::vector<PictureBuffer>& buffers) override;
  void ReusePictureBuffer(int32_t picture_buffer_id) override;
  void Flush() override;
  void Reset() override;
  void SetOverlayInfo(const OverlayInfo&) override;
  void Destroy() override;

  // gpu::CommandBufferProxyImpl::DeletionObserver implemetnation.
  void OnWillDeleteImpl() override;

 private:
  // Only Destroy() should be deleting |this|.
  ~GpuVideoDecodeAcceleratorHost() override;

  // Notify |client_| of an error.  Posts a task to avoid re-entrancy.
  void PostNotifyError(Error);

  void Send(IPC::Message* message);

  // IPC handlers, proxying VideoDecodeAccelerator::Client for the GPU
  // process.  Should not be called directly.
  void OnInitializationComplete(bool success);
  void OnBitstreamBufferProcessed(int32_t bitstream_buffer_id);
  void OnProvidePictureBuffers(uint32_t num_requested_buffers,
                               VideoPixelFormat format,
                               uint32_t textures_per_buffer,
                               const gfx::Size& dimensions,
                               uint32_t texture_target);
  void OnDismissPictureBuffer(int32_t picture_buffer_id);
  void OnPictureReady(
      const AcceleratedVideoDecoderHostMsg_PictureReady_Params& params);
  void OnFlushDone();
  void OnResetDone();
  void OnNotifyError(uint32_t error);

  scoped_refptr<gpu::GpuChannelHost> channel_;

  // Route ID for the associated decoder in the GPU process.
  int32_t decoder_route_id_;

  // The client that will receive callbacks from the decoder.
  Client* client_;

  // Protect |impl_|. |impl_| is used on media thread, but it can be invalidated
  // on main thread.
  base::Lock impl_lock_;

  // Unowned reference to the gpu::CommandBufferProxyImpl that created us.
  // |this| registers as a DeletionObserver of |impl_|, the so reference is
  // always valid as long as it is not NULL.
  gpu::CommandBufferProxyImpl* impl_;

  // Requested dimensions of the buffer, from ProvidePictureBuffers().
  gfx::Size picture_buffer_dimensions_;

  // Task runner for tasks that should run on the thread this class is
  // constructed.
  scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;

  // WeakPtr for posting tasks to ourself.
  base::WeakPtr<GpuVideoDecodeAcceleratorHost> weak_this_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<GpuVideoDecodeAcceleratorHost> weak_this_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(GpuVideoDecodeAcceleratorHost);
};

}  // namespace media

#endif  // MEDIA_GPU_IPC_CLIENT_GPU_VIDEO_DECODE_ACCELERATOR_HOST_H_
