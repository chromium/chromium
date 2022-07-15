// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/mailbox_video_frame_converter.h"

#include "base/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/gpu_memory_buffer.h"

using ::testing::_;

namespace media {

namespace {

VideoFrame* UnwrapVideoFrame(const VideoFrame& frame) {
  return const_cast<VideoFrame*>(&frame);
}

class MockGpuDelegate : public MailboxVideoFrameConverter::GpuDelegate {
 public:
  MOCK_METHOD0(Initialize, bool());
  MOCK_METHOD10(CreateSharedImage,
                gpu::SharedImageStub::SharedImageDestructionCallback(
                    const gpu::Mailbox& mailbox,
                    gfx::GpuMemoryBufferHandle handle,
                    gfx::BufferFormat format,
                    gfx::BufferPlane plane,
                    gpu::SurfaceHandle surface_handle,
                    const gfx::Size& size,
                    const gfx::ColorSpace& color_space,
                    GrSurfaceOrigin surface_origin,
                    SkAlphaType alpha_type,
                    uint32_t usage));
  MOCK_METHOD2(UpdateSharedImage,
               bool(const gpu::Mailbox& mailbox,
                    gfx::GpuFenceHandle in_fence_handle));
  MOCK_METHOD2(WaitOnSyncTokenAndReleaseFrame,
               bool(scoped_refptr<VideoFrame> frame,
                    const gpu::SyncToken& sync_token));
};

}  // anonymous namespace

class MailboxVideoFrameConverterTest : public testing::Test {
 public:
  MailboxVideoFrameConverterTest()
      : converter_(new MailboxVideoFrameConverter(
            base::BindRepeating(&UnwrapVideoFrame),
            base::ThreadTaskRunnerHandle::Get(),
            std::make_unique<MockGpuDelegate>(),
            /*enable_unsafe_webgpu=*/false)) {}

  MailboxVideoFrameConverterTest(const MailboxVideoFrameConverterTest&) =
      delete;
  MailboxVideoFrameConverterTest& operator=(
      const MailboxVideoFrameConverterTest&) = delete;

  ~MailboxVideoFrameConverterTest() override = default;

  void TearDown() override {
    // |converter_| might have created resources that need to be cleaned up.
    converter_.reset();
    task_environment_.RunUntilIdle();
  }

  MOCK_METHOD1(OutputCB, void(scoped_refptr<VideoFrame>));

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<VideoFrameConverter> converter_;
};

TEST_F(MailboxVideoFrameConverterTest, Initialize) {
  EXPECT_CALL(*this, OutputCB(_)).Times(0);
  converter_->Initialize(
      base::ThreadTaskRunnerHandle::Get(),
      base::BindRepeating(&MailboxVideoFrameConverterTest::OutputCB,
                          base::Unretained(this)));
  EXPECT_FALSE(converter_->HasPendingFrames());
}

}  // namespace media
