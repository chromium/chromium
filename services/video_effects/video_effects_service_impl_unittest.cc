// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "services/video_effects/video_effects_service_impl.h"

#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom.h"
#include "services/video_effects/public/mojom/video_effects_service.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace video_effects {

using testing::_;

namespace {

class TestGpuChannelHostProvider : public GpuChannelHostProvider {
 public:
  TestGpuChannelHostProvider() = default;

  scoped_refptr<gpu::GpuChannelHost> GetGpuChannelHost() override {
    return nullptr;
  }
};

}  // namespace

class VideoEffectsServiceTest : public testing::Test {
  void SetUp() override {
    service_impl_.emplace(service_remote_.BindNewPipeAndPassReceiver(),
                          std::make_unique<TestGpuChannelHostProvider>());
  }

 protected:
  base::test::TaskEnvironment task_environment_;

  // Service under test (remote and impl):
  mojo::Remote<mojom::VideoEffectsService> service_remote_;
  std::optional<VideoEffectsServiceImpl> service_impl_;
};

TEST_F(VideoEffectsServiceTest, CreateEffectsProcessorWorks) {
  // Calling into `VideoEffectsService:::CreateEffectsProcessor()` is expected
  // to work (irrespective of whether the passed-in pipes are usable or not).

  base::RunLoop run_loop;

  mojo::PendingReceiver<media::mojom::VideoEffectsManager> manager_receiver;
  mojo::Remote<mojom::VideoEffectsProcessor> processor_remote;

  service_remote_->CreateEffectsProcessor(
      manager_receiver.InitWithNewPipeAndPassRemote(),
      processor_remote.BindNewPipeAndPassReceiver());

  run_loop.RunUntilIdle();

  EXPECT_TRUE(processor_remote.is_connected());
}

}  // namespace video_effects
