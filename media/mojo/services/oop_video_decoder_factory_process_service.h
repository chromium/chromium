// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_OOP_VIDEO_DECODER_FACTORY_PROCESS_SERVICE_H_
#define MEDIA_MOJO_SERVICES_OOP_VIDEO_DECODER_FACTORY_PROCESS_SERVICE_H_

#include "base/sequence_checker.h"
#include "gpu/ipc/client/gpu_channel_observer.h"
#include "media/mojo/mojom/video_decoder_factory_process.mojom.h"
#include "media/mojo/services/media_mojo_export.h"
#include "media/mojo/services/oop_video_decoder_factory_service.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace gpu {
class SharedImageInterface;
}

namespace viz {
class Gpu;
}

namespace media {

// An OOPVideoDecoderFactoryProcessService allows the browser process to
// initialize an InterfaceFactory with a gpu::GpuFeatureInfo.
class MEDIA_MOJO_EXPORT OOPVideoDecoderFactoryProcessService final
    : public mojom::VideoDecoderFactoryProcess,
      public gpu::GpuChannelLostObserver {
 public:
  explicit OOPVideoDecoderFactoryProcessService(
      mojo::PendingReceiver<mojom::VideoDecoderFactoryProcess> receiver,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);
  OOPVideoDecoderFactoryProcessService(
      const OOPVideoDecoderFactoryProcessService&) = delete;
  OOPVideoDecoderFactoryProcessService& operator=(
      const OOPVideoDecoderFactoryProcessService&) = delete;
  ~OOPVideoDecoderFactoryProcessService() final;

  // mojom::VideoDecoderFactoryProcess implementation.
  void InitializeVideoDecoderFactory(
      const gpu::GpuFeatureInfo& gpu_feature_info,
      mojo::PendingReceiver<mojom::InterfaceFactory> receiver,
      mojo::PendingRemote<viz::mojom::Gpu> gpu_remote) final;

  // gpu::GpuChannelLostObserver implementation.
  void OnGpuChannelLost() final;

  void OnFactoryDisconnected();

 private:
  mojo::Receiver<mojom::VideoDecoderFactoryProcess> receiver_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<OOPVideoDecoderFactoryService> factory_
      GUARDED_BY_CONTEXT(sequence_checker_);

  void OnGpuChannelLostTask();
  scoped_refptr<gpu::SharedImageInterface> GetSharedImageInterface();

  scoped_refptr<gpu::SharedImageInterface> shared_image_interface_;
  std::unique_ptr<viz::Gpu> viz_gpu_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<OOPVideoDecoderFactoryProcessService> weak_ptr_factory_{
      this};
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_OOP_VIDEO_DECODER_FACTORY_PROCESS_SERVICE_H_
