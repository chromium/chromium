// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_EFFECTS_TEST_FAKE_VIDEO_EFFECTS_SERVICE_H_
#define SERVICES_VIDEO_EFFECTS_TEST_FAKE_VIDEO_EFFECTS_SERVICE_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/test/test_future.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/video_effects/public/mojom/video_effects_service.mojom.h"
#include "services/video_effects/test/fake_video_effects_processor.h"
#include "services/viz/public/mojom/gpu.mojom-forward.h"

namespace video_effects {

// Fake implementation of Video Effects Service.
// Intended to be used in unit tests.
class FakeVideoEffectsService : public mojom::VideoEffectsService {
 public:
  explicit FakeVideoEffectsService(
      mojo::PendingReceiver<mojom::VideoEffectsService> receiver);
  ~FakeVideoEffectsService() override;

  // mojom::VideoEffectsService implementation:

  // The fake implementation will ensure that the remote end of the passed in
  // `processor` receiver will stay connected for as long as the fake is alive.
  void CreateEffectsProcessor(
      const std::string& device_id,
      mojo::PendingRemote<viz::mojom::Gpu> gpu,
      mojo::PendingRemote<media::mojom::VideoEffectsManager> manager,
      mojo::PendingReceiver<mojom::VideoEffectsProcessor> processor) override;

  void SetBackgroundSegmentationModel(base::File model_file) override;

  // Returns a test future which will be resolved when the next video effects
  // processor creation request is fulfilled. There can be at most one
  // outstanding test feature created at any given time.
  std::unique_ptr<base::test::TestFuture<void>>
  GetEffectsProcessorCreationFuture();

  std::unique_ptr<base::test::TestFuture<base::File>>
  GetBackgroundSegmentationModelFuture();

  // For testing, get the processors that this service created.
  base::flat_map<std::string, std::unique_ptr<FakeVideoEffectsProcessor>>&
  GetProcessors();

 private:
  mojo::Receiver<mojom::VideoEffectsService> receiver_;

  // Note: indirection via std::unique_ptr is required since the processor is
  // not copyable and not moveable.
  base::flat_map<std::string, std::unique_ptr<FakeVideoEffectsProcessor>>
      processors_;

  base::OnceClosure effects_processor_creation_cb_;
  base::OnceCallback<void(base::File)> background_segmentation_model_set_cb_;
};

}  // namespace video_effects

#endif  // SERVICES_VIDEO_EFFECTS_TEST_FAKE_VIDEO_EFFECTS_SERVICE_H_
