// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "services/video_effects/video_effects_processor_impl.h"

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "media/capture/mojom/video_capture_buffer.mojom.h"
#include "media/capture/mojom/video_effects_manager.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace video_effects {

using testing::_;

namespace {

// Helper, returns dummy (but valid) VideoBufferHandlePtr.
media::mojom::VideoBufferHandlePtr GetDummyVideoBufferHandle() {
  return media::mojom::VideoBufferHandle::NewMailboxHandles(
      media::mojom::MailboxBufferHandleSet::New(
          std::vector<gpu::MailboxHolder>(4)));
}

class VideoEffectsProcessorTest : public testing::Test {
  void SetUp() override {
    processor_impl_.emplace(manager_receiver_.InitWithNewPipeAndPassRemote(),
                            processor_remote_.BindNewPipeAndPassReceiver());
  }

 protected:
  base::test::TaskEnvironment task_environment_;

  // Processor under test (remote and impl):
  mojo::Remote<mojom::VideoEffectsProcessor> processor_remote_;
  std::optional<VideoEffectsProcessorImpl> processor_impl_;

  mojo::PendingReceiver<media::mojom::VideoEffectsManager> manager_receiver_;
};

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
