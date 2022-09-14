// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_IPC_CLIENT_GPU_VIDEO_DECODE_ACCELERATOR_HOST_H_
#define MEDIA_GPU_IPC_CLIENT_GPU_VIDEO_DECODE_ACCELERATOR_HOST_H_

#include <stdint.h>

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "gpu/ipc/client/command_buffer_proxy_impl.h"
#include "media/mojo/mojom/gpu_accelerated_video_decoder.mojom.h"
#include "media/video/video_decode_accelerator.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/shared_associated_remote.h"
#include "ui/gfx/geometry/size.h"

namespace media {

// This class is used to talk to VideoDecodeAccelerator in the Gpu process
// through IPC messages.
class GpuVideoDecodeAcceleratorHost
    : public VideoDecodeAccelerator,
      public gpu::CommandBufferProxyImpl::DeletionObserver,
      public mojom::GpuAcceleratedVideoDecoderClient {
 public:
  // |this| is guaranteed not to outlive |impl|.  (See comments for |impl_|.)
  explicit GpuVideoDecodeAcceleratorHost(gpu::CommandBufferProxyImpl* impl);
  GpuVideoDecodeAcceleratorHost(const GpuVideoDecodeAcceleratorHost&) = delete;
  GpuVideoDecodeAcceleratorHost& operator=(GpuVideoDecodeAcceleratorHost&) =
      delete;

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

  void OnDisconnectedFromGpuProcess();

  // Notify |client_| of an error.  Posts a task to avoid re-entrancy.
  void PostNotifyError(Error);

  // mojom::GpuAcceleratedVideoDecoderClient:
  void OnInitializationComplete(bool success) override;
  void OnBitstreamBufferProcessed(int32_t bitstream_buffer_id) override;
  void OnProvidePictureBuffers(uint32_t num_requested_buffers,
                               VideoPixelFormat format,
                               uint32_t textures_per_buffer,
                               const gfx::Size& dimensions,
                               uint32_t texture_target) override;
  void OnDismissPictureBuffer(int32_t picture_buffer_id) override;
  void OnPictureReady(mojom::PictureReadyParamsPtr params) override;
  void OnError(uint32_t error) override;

  void OnFlushDone();
  void OnResetDone();

  // Receiver and remote endpoints for connections to the GPU process. These are
  // associated with the corresponding CommandBuffer interface given at
  // construction time.
  mojo::AssociatedReceiver<mojom::GpuAcceleratedVideoDecoderClient>
      client_receiver_{this};
  mojo::SharedAssociatedRemote<mojom::GpuAcceleratedVideoDecoder> decoder_;

  // The client that will receive callbacks from the decoder.
  raw_ptr<Client> client_;

  // Protect |impl_|. |impl_| is used on media thread, but it can be invalidated
  // on main thread.
  base::Lock impl_lock_;

  // Unowned reference to the gpu::CommandBufferProxyImpl that created us.
  // |this| registers as a DeletionObserver of |impl_|, the so reference is
  // always valid as long as it is not NULL.
  raw_ptr<gpu::CommandBufferProxyImpl> impl_;

  // Requested dimensions of the buffer, from ProvidePictureBuffers().
  gfx::Size picture_buffer_dimensions_;

  // Task runner for tasks that should run on the thread this class is
  // constructed.
  scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;

  // WeakPtr for posting tasks to ourself.
  base::WeakPtr<GpuVideoDecodeAcceleratorHost> weak_this_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<GpuVideoDecodeAcceleratorHost> weak_this_factory_{this};
};

}  // namespace media

#endif  // MEDIA_GPU_IPC_CLIENT_GPU_VIDEO_DECODE_ACCELERATOR_HOST_H_
