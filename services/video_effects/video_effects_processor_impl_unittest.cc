// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_effects/video_effects_processor_impl.h"

#include <optional>

#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/context_result.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "gpu/ipc/common/gpu_channel.mojom-forward.h"
#include "gpu/ipc/common/mock_gpu_channel.h"
#include "media/capture/mojom/video_capture_buffer.mojom.h"
#include "media/capture/mojom/video_effects_manager.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom.h"
#include "services/video_effects/test_gpu_channel_host_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace video_effects {

using testing::_;

namespace {

// Helper, returns dummy (but valid) VideoBufferHandlePtr.
media::mojom::VideoBufferHandlePtr GetDummyVideoBufferHandle() {
  auto shared_image = gpu::ClientSharedImage::CreateForTesting();
  return media::mojom::VideoBufferHandle::NewSharedImageHandle(
      media::mojom::SharedImageBufferHandleSet::New(shared_image->Export(),
                                                    gpu::SyncToken()));
}

class VideoEffectsProcessorTest : public testing::Test {
  void SetUp() override {
    auto gpu_channel_host_provider =
        std::make_unique<TestGpuChannelHostProvider>(gpu_channel_);

    on_processor_error_.emplace();

    processor_impl_.emplace(manager_receiver_.InitWithNewPipeAndPassRemote(),
                            processor_remote_.BindNewPipeAndPassReceiver(),
                            std::move(gpu_channel_host_provider),
                            on_processor_error_->GetCallback());
  }

 protected:
  base::test::TaskEnvironment task_environment_;

  gpu::MockGpuChannel gpu_channel_;

  // Processor under test:
  std::optional<VideoEffectsProcessorImpl> processor_impl_;

  std::optional<base::test::TestFuture<void>> on_processor_error_;

  // Processor under test's remote. The unit-tests will usually interact with
  // the processor via the remote.
  mojo::Remote<mojom::VideoEffectsProcessor> processor_remote_;
  mojo::PendingReceiver<media::mojom::VideoEffectsManager> manager_receiver_;
};

TEST_F(VideoEffectsProcessorTest, InitializeSucceeds) {
  ON_CALL(gpu_channel_, CreateCommandBuffer(_, _, _, _, _, _, _, _))
      .WillByDefault(
          [](gpu::mojom::CreateCommandBufferParamsPtr params,
             int32_t routing_id, base::UnsafeSharedMemoryRegion shared_state,
             mojo::PendingAssociatedReceiver<gpu::mojom::CommandBuffer>
                 receiver,
             mojo::PendingAssociatedRemote<gpu::mojom::CommandBufferClient>
                 client,
             gpu::ContextResult* result, gpu::Capabilities* capabilities,
             gpu::GLCapabilities* gl_capabilities) -> bool {
            capabilities->sync_query = true;
            receiver.EnableUnassociatedUsage();
            *result = gpu::ContextResult::kSuccess;
            return true;
          });

  EXPECT_TRUE(processor_impl_->Initialize());
}

TEST_F(VideoEffectsProcessorTest, ErrorCallbackCalledWhenManagerDisconnects) {
  manager_receiver_.reset();
  EXPECT_TRUE(on_processor_error_->Wait());
}

TEST_F(VideoEffectsProcessorTest, ErrorCallbackCalledWhenProcessorDisconnects) {
  processor_remote_.reset();
  EXPECT_TRUE(on_processor_error_->Wait());
}

// TODO(b/333097635): Figure out how to mock/fake the GpuChannel so that it
// does not raise context loss events immediately after creating context
// providers.
TEST_F(VideoEffectsProcessorTest,
       DISABLED_ErrorCallbackCalledContextRecreationFailed) {
  // Initialize the processor first, GPU service connections won't be
  // established until the processor has been initialized and thus it won't
  // auto-recover from GPU context loss.
  ASSERT_TRUE(processor_impl_->Initialize());
}

// TODO(b/333097635): Figure out how to mock/fake the GpuChannel so that it
// does not raise context loss events immediately after creating context
// providers.
TEST_F(VideoEffectsProcessorTest,
       DISABLED_ProcessorGivesUpAfterTooManyContextLosses) {}

TEST_F(VideoEffectsProcessorTest, PostProcessRunsAndFails) {
  // For now, since `VideoEffectsProcessorImpl` is still a stub, we expect a
  // call to `VideoEffectsProcessor::PostProcess()` to fail.

  base::test::TestFuture<mojom::PostProcessResultPtr> post_process_result;

  processor_remote_->PostProcess(
      GetDummyVideoBufferHandle(), media::mojom::VideoFrameInfo::New(),
      GetDummyVideoBufferHandle(), media::VideoPixelFormat::PIXEL_FORMAT_I420,
      post_process_result.GetCallback());

  mojom::PostProcessResultPtr result = post_process_result.Take();
  EXPECT_TRUE(result->is_error());
}

}  // namespace

}  // namespace video_effects
