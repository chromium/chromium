// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_EFFECTS_VIDEO_EFFECTS_SERVICE_IMPL_H_
#define SERVICES_VIDEO_EFFECTS_VIDEO_EFFECTS_SERVICE_IMPL_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "media/capture/mojom/video_effects_manager.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom-forward.h"
#include "services/video_effects/public/mojom/video_effects_service.mojom.h"

namespace video_effects {

class VideoEffectsProcessorImpl;

// Abstract interface that is used by `VideoEffectsServiceImpl` to obtain
// instances of `gpu::GpuChannelHost`. Those are then going to be used to
// create context providers over which the communication to GPU service will
// happen.
class GpuChannelHostProvider {
 public:
  virtual ~GpuChannelHostProvider() = default;

  // Return a connected `gpu::GpuChannelHost`. Implementations should expect
  // this method to be called somewhat frequently when a new Video Effects
  // Processor is created.
  virtual scoped_refptr<gpu::GpuChannelHost> GetGpuChannelHost() = 0;
};

class VideoEffectsServiceImpl : public mojom::VideoEffectsService {
 public:
  // Similarly to `VideoCaptureServiceImpl`, `VideoEfffectsServiceImpl` needs
  // to receive something that returns `gpu::GpuChannelHost` instances in order
  // to be able to communicate with the GPU service - this is passed in via the
  // `gpu_channel_host_provider`.
  // `receiver` is the receiving end of the mojo pipe used to communicate with
  // this instance.
  explicit VideoEffectsServiceImpl(
      mojo::PendingReceiver<mojom::VideoEffectsService> receiver,
      std::unique_ptr<GpuChannelHostProvider> gpu_channel_host_provider);

  ~VideoEffectsServiceImpl() override;

  // mojom::VideoEffectsService implementation:
  void CreateEffectsProcessor(
      const std::string& device_id,
      mojo::PendingRemote<media::mojom::VideoEffectsManager> manager,
      mojo::PendingReceiver<mojom::VideoEffectsProcessor> processor) override;

 private:
  // Helper - used to clean up instances of `VideoEffectsProcessor`s that are
  // no longer functional.
  void RemoveProcessor(const std::string& id);

  // Mapping from the device ID to processor implementation. Device ID is only
  // used to deduplicate processor creation requests.
  base::flat_map<std::string, std::unique_ptr<VideoEffectsProcessorImpl>>
      processors_;

  mojo::Receiver<mojom::VideoEffectsService> receiver_;
  std::unique_ptr<GpuChannelHostProvider> gpu_channel_host_provider_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Must be last:
  base::WeakPtrFactory<VideoEffectsServiceImpl> weak_ptr_factory_{this};
};

}  // namespace video_effects

#endif
