// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_effects/video_effects_service_impl.h"

#include <optional>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "gpu/ipc/common/mock_gpu_channel.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom.h"
#include "services/video_effects/public/mojom/video_effects_service.mojom.h"
#include "services/video_effects/test_gpu_channel_host_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace video_effects {

using testing::_;

namespace {

constexpr char kDeviceId[] = "test_device";

}  // namespace

class VideoEffectsServiceTest : public testing::Test {
  void SetUp() override {
    service_impl_.emplace(service_remote_.BindNewPipeAndPassReceiver(),
                          task_environment_.GetMainThreadTaskRunner());
  }

 protected:
  base::test::TaskEnvironment task_environment_;

  gpu::MockGpuChannel gpu_channel_;

  // Service under test (remote and impl):
  mojo::Remote<mojom::VideoEffectsService> service_remote_;
  std::optional<VideoEffectsServiceImpl> service_impl_;
};

// TODO(b/333097635): Figure out how to mock/fake the GpuChannel so that it
// does not raise context loss events immediately after creating context
// providers.
TEST_F(VideoEffectsServiceTest, DISABLED_CreateEffectsProcessorWorks) {
  // Calling into `VideoEffectsService:::CreateEffectsProcessor()` is expected
  // to work (irrespective of whether the passed-in pipes are usable or not).

  mojo::PendingReceiver<viz::mojom::Gpu> gpu_receiver;
  mojo::PendingReceiver<media::mojom::VideoEffectsManager> manager_receiver;
  mojo::Remote<mojom::VideoEffectsProcessor> processor_remote;

  service_remote_->CreateEffectsProcessor(
      kDeviceId, gpu_receiver.InitWithNewPipeAndPassRemote(),
      manager_receiver.InitWithNewPipeAndPassRemote(),
      processor_remote.BindNewPipeAndPassReceiver());

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(processor_remote.is_connected());
}

// TODO(b/333097635): Figure out how to mock/fake the GpuChannel so that it
// does not raise context loss events immediately after creating context
// providers.
TEST_F(VideoEffectsServiceTest,
       DISABLED_CreateEffectsProcessorWithSameIdFails) {
  // Calling into `VideoEffectsService:::CreateEffectsProcessor()` is expected
  // to fail if the same device id is passed.

  mojo::PendingReceiver<viz::mojom::Gpu> gpu_receiver1;
  mojo::PendingReceiver<media::mojom::VideoEffectsManager> manager_receiver1;
  mojo::Remote<mojom::VideoEffectsProcessor> processor_remote1;

  service_remote_->CreateEffectsProcessor(
      kDeviceId, gpu_receiver1.InitWithNewPipeAndPassRemote(),
      manager_receiver1.InitWithNewPipeAndPassRemote(),
      processor_remote1.BindNewPipeAndPassReceiver());

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(processor_remote1.is_connected());

  mojo::PendingReceiver<viz::mojom::Gpu> gpu_receiver2;
  mojo::PendingReceiver<media::mojom::VideoEffectsManager> manager_receiver2;
  mojo::Remote<mojom::VideoEffectsProcessor> processor_remote2;

  service_remote_->CreateEffectsProcessor(
      kDeviceId, gpu_receiver2.InitWithNewPipeAndPassRemote(),
      manager_receiver2.InitWithNewPipeAndPassRemote(),
      processor_remote2.BindNewPipeAndPassReceiver());

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(processor_remote1.is_connected());
  EXPECT_FALSE(processor_remote2.is_connected());
}

// TODO(b/333097635): Figure out how to mock/fake the GpuChannel so that it
// does not raise context loss events immediately after creating context
// providers.
TEST_F(VideoEffectsServiceTest,
       DISABLED_RecreateEffectsProcessorWithSameIdSucceeds) {
  // Calling into `VideoEffectsService:::CreateEffectsProcessor()` is expected
  // to succeed if the previous processor with that ID has been removed

  mojo::PendingReceiver<viz::mojom::Gpu> gpu_receiver1;
  mojo::PendingReceiver<media::mojom::VideoEffectsManager> manager_receiver1;
  mojo::Remote<mojom::VideoEffectsProcessor> processor_remote1;

  service_remote_->CreateEffectsProcessor(
      kDeviceId, gpu_receiver1.InitWithNewPipeAndPassRemote(),
      manager_receiver1.InitWithNewPipeAndPassRemote(),
      processor_remote1.BindNewPipeAndPassReceiver());

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(processor_remote1.is_connected());

  // Disconnect the first processor.
  processor_remote1.reset();
  base::RunLoop().RunUntilIdle();

  mojo::PendingReceiver<viz::mojom::Gpu> gpu_receiver2;
  mojo::PendingReceiver<media::mojom::VideoEffectsManager> manager_receiver2;
  mojo::Remote<mojom::VideoEffectsProcessor> processor_remote2;

  service_remote_->CreateEffectsProcessor(
      kDeviceId, gpu_receiver2.InitWithNewPipeAndPassRemote(),
      manager_receiver2.InitWithNewPipeAndPassRemote(),
      processor_remote2.BindNewPipeAndPassReceiver());

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(processor_remote2.is_connected());
}

}  // namespace video_effects
