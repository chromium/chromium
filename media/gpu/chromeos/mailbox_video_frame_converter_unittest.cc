// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/mailbox_video_frame_converter.h"

#include "base/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace media {

namespace {

VideoFrame* UnwrapVideoFrame(const VideoFrame& frame) {
  return const_cast<VideoFrame*>(&frame);
}

base::WeakPtr<gpu::GpuChannel> GetGpuChannel() {
  return nullptr;
}

}  // anonymous namespace

class MailboxVideoFrameConverterTest : public testing::Test {
 public:
  MailboxVideoFrameConverterTest()
      : converter_(new MailboxVideoFrameConverter(
            base::BindRepeating(&UnwrapVideoFrame),
            base::ThreadTaskRunnerHandle::Get(),
            base::BindRepeating(&GetGpuChannel))) {}
  ~MailboxVideoFrameConverterTest() override = default;

  void TearDown() override {
    // |converter_| might have created resources that need to be cleaned up.
    converter_.reset();
    task_environment_.RunUntilIdle();
  }

  MOCK_METHOD1(OutputCB, void(scoped_refptr<VideoFrame>));

  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<VideoFrameConverter> converter_;

  DISALLOW_COPY_AND_ASSIGN(MailboxVideoFrameConverterTest);
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
