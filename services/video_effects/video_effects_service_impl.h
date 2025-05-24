// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_EFFECTS_VIDEO_EFFECTS_SERVICE_IMPL_H_
#define SERVICES_VIDEO_EFFECTS_VIDEO_EFFECTS_SERVICE_IMPL_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/memory_mapped_file.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "media/capture/mojom/video_effects_manager.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/video_effects/gpu_channel_host_provider.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom-forward.h"
#include "services/video_effects/public/mojom/video_effects_service.mojom.h"
#include "services/video_effects/webgpu_device.h"
#include "services/viz/public/mojom/gpu.mojom-forward.h"
#include "third_party/dawn/include/dawn/webgpu_cpp.h"

namespace video_effects {

class VideoEffectsProcessorImpl;

class VideoEffectsServiceImpl : public mojom::VideoEffectsService,
                                public GpuChannelHostProvider::Observer {
 public:
  explicit VideoEffectsServiceImpl(
      mojo::PendingReceiver<mojom::VideoEffectsService> receiver,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);

  ~VideoEffectsServiceImpl() override;

  // mojom::VideoEffectsService implementation:
  void CreateEffectsProcessor(
      const std::string& device_id,
      mojo::PendingRemote<viz::mojom::Gpu> gpu,
      mojo::PendingRemote<media::mojom::ReadonlyVideoEffectsManager> manager,
      mojo::PendingReceiver<mojom::VideoEffectsProcessor> processor) override;

  void SetBackgroundSegmentationModel(base::File model_file) override;

 private:
  // GpuChannelHostProvider::Observer:
  void OnPermanentError(scoped_refptr<GpuChannelHostProvider>) override;
  void OnContextLost(scoped_refptr<GpuChannelHostProvider>) override;

  // Creates `webgpu_device_` and initializes it asynchronously.  On completion,
  // invokes `FinishCreatingEffectsProcessors()`.
  void CreateWebGpuDeviceAndEffectsProcessors();

  // Callback functions for WebGpuDevice.
  void OnDeviceCreated(wgpu::Device device);
  void OnDeviceError(WebGpuDevice::Error error, std::string msg);
  void OnDeviceLost(wgpu::DeviceLostReason reason, std::string msg);

  // Finishes creation of pending effects processors in `pending_processors_`.
  void FinishCreatingEffectsProcessors();

  // Finishes creation of an effects processor and inserts it into
  // `processors_`.
  void FinishCreatingEffectsProcessor(
      const std::string& device_id,
      mojo::PendingRemote<media::mojom::ReadonlyVideoEffectsManager>
          manager_remote,
      mojo::PendingReceiver<mojom::VideoEffectsProcessor> processor_receiver);

  // Helper - used to clean up instances of `VideoEffectsProcessor`s that are
  // no longer functional.
  void RemoveProcessor(const std::string& id);

  // Destroy all processors (pending and live).
  void Cleanup();

  std::unique_ptr<base::MemoryMappedFile> model_;

  // Holder of wgpu::Device instance.
  std::unique_ptr<WebGpuDevice> webgpu_device_;

  // Reference wgpu::Device currently in use.  If empty, then there is no active
  // wgpu::Device.
  wgpu::Device device_;

  mojo::Receiver<mojom::VideoEffectsService> receiver_;

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // Mapping from the device ID to processor implementation. Device ID is only
  // used to deduplicate processor creation requests.
  base::flat_map<std::string, std::unique_ptr<VideoEffectsProcessorImpl>>
      processors_;

  // Holds arguments needed to create a VideoEffectsProcessor.
  struct PendingEffectsProcessor {
    PendingEffectsProcessor();
    PendingEffectsProcessor(const PendingEffectsProcessor&) = delete;
    PendingEffectsProcessor& operator=(const PendingEffectsProcessor&) = delete;
    PendingEffectsProcessor(PendingEffectsProcessor&&);
    PendingEffectsProcessor& operator=(PendingEffectsProcessor&&);
    ~PendingEffectsProcessor();

    mojo::PendingRemote<media::mojom::ReadonlyVideoEffectsManager>
        manager_remote;
    mojo::PendingReceiver<mojom::VideoEffectsProcessor> processor_receiver;
  };

  // Mapping of device ID to pending requests to create effects processors.
  base::flat_map<std::string, PendingEffectsProcessor> pending_processors_;

  // Provides GPU context objects as needed and monitors context lost events.
  scoped_refptr<GpuChannelHostProvider> gpu_channel_host_provider_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Must be last:
  base::WeakPtrFactory<VideoEffectsServiceImpl> weak_ptr_factory_{this};
};

}  // namespace video_effects

#endif  // SERVICES_VIDEO_EFFECTS_VIDEO_EFFECTS_SERVICE_IMPL_H_
