// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_effects/test/fake_video_effects_service.h"

#include <memory>

#include "base/test/test_future.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom.h"
#include "services/video_effects/test/fake_video_effects_processor.h"
#include "services/viz/public/mojom/gpu.mojom.h"

namespace video_effects {

FakeVideoEffectsService::FakeVideoEffectsService(
    mojo::PendingReceiver<mojom::VideoEffectsService> receiver)
    : receiver_(this, std::move(receiver)) {}

FakeVideoEffectsService::~FakeVideoEffectsService() = default;

void FakeVideoEffectsService::CreateEffectsProcessor(
    const std::string& device_id,
    mojo::PendingRemote<viz::mojom::Gpu> gpu,
    mojo::PendingRemote<media::mojom::VideoEffectsManager> manager,
    mojo::PendingReceiver<mojom::VideoEffectsProcessor> processor) {
  processors_.insert(
      std::make_pair(device_id, std::make_unique<FakeVideoEffectsProcessor>(
                                    std::move(processor), std::move(manager))));

  if (effects_processor_creation_cb_) {
    std::move(effects_processor_creation_cb_).Run();
  }
}

std::unique_ptr<base::test::TestFuture<void>>
FakeVideoEffectsService::GetEffectsProcessorCreationFuture() {
  CHECK(!effects_processor_creation_cb_);

  auto result = std::make_unique<base::test::TestFuture<void>>();
  effects_processor_creation_cb_ = result->GetCallback();
  return result;
}

void FakeVideoEffectsService::SetBackgroundSegmentationModel(
    base::File model_file) {
  if (background_segmentation_model_set_cb_) {
    std::move(background_segmentation_model_set_cb_).Run(std::move(model_file));
  }
}

std::unique_ptr<base::test::TestFuture<base::File>>
FakeVideoEffectsService::GetBackgroundSegmentationModelFuture() {
  CHECK(!background_segmentation_model_set_cb_);

  auto result = std::make_unique<base::test::TestFuture<base::File>>();
  background_segmentation_model_set_cb_ = result->GetCallback();
  return result;
}

base::flat_map<std::string, std::unique_ptr<FakeVideoEffectsProcessor>>&
FakeVideoEffectsService::GetProcessors() {
  return processors_;
}

}  // namespace video_effects
