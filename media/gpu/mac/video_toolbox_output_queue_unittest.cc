// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/mac/video_toolbox_output_queue.h"

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "media/base/decoder_status.h"
#include "media/base/video_frame.h"
#include "media/gpu/codec_picture.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace media {

namespace {

MATCHER_P(StatusEq, code, "") {
  return arg.code() == code;
}

}  // namespace

class VideoToolboxOutputQueueTest : public testing::Test {
 public:
  VideoToolboxOutputQueueTest() {
    output_queue_.SetOutputCB(base::BindRepeating(
        &VideoToolboxOutputQueueTest::OnOutput, base::Unretained(this)));
  }

  ~VideoToolboxOutputQueueTest() override = default;

 protected:
  MOCK_METHOD1(OnOutput, void(scoped_refptr<VideoFrame>));
  MOCK_METHOD1(OnFlush, void(DecoderStatus));

  void Flush() {
    output_queue_.Flush(base::BindOnce(&VideoToolboxOutputQueueTest::OnFlush,
                                       base::Unretained(this)));
  }

  base::test::TaskEnvironment task_environment_;
  VideoToolboxOutputQueue output_queue_{
      task_environment_.GetMainThreadTaskRunner()};
};

TEST_F(VideoToolboxOutputQueueTest, Construct) {}

TEST_F(VideoToolboxOutputQueueTest, Basic) {
  auto pic = base::MakeRefCounted<CodecPicture>();
  auto frame = VideoFrame::CreateEOSFrame();

  EXPECT_CALL(*this, OnOutput(frame));

  output_queue_.SchedulePicture(pic);
  output_queue_.FulfillPicture(pic, frame);

  task_environment_.RunUntilIdle();
}

TEST_F(VideoToolboxOutputQueueTest, Reordered) {
  auto pic1 = base::MakeRefCounted<CodecPicture>();
  auto pic2 = base::MakeRefCounted<CodecPicture>();
  auto frame1 = VideoFrame::CreateEOSFrame();
  auto frame2 = VideoFrame::CreateEOSFrame();

  testing::InSequence in_sequence;
  EXPECT_CALL(*this, OnOutput(frame2));
  EXPECT_CALL(*this, OnOutput(frame1));

  output_queue_.SchedulePicture(pic2);
  output_queue_.FulfillPicture(pic1, frame1);
  output_queue_.FulfillPicture(pic2, frame2);
  output_queue_.SchedulePicture(pic1);

  task_environment_.RunUntilIdle();
}

TEST_F(VideoToolboxOutputQueueTest, Flush) {
  auto pic = base::MakeRefCounted<CodecPicture>();
  auto frame = VideoFrame::CreateEOSFrame();

  testing::InSequence in_sequence;
  EXPECT_CALL(*this, OnOutput(frame));
  EXPECT_CALL(*this, OnFlush(_));

  output_queue_.SchedulePicture(pic);
  Flush();
  output_queue_.FulfillPicture(pic, frame);

  task_environment_.RunUntilIdle();
}

TEST_F(VideoToolboxOutputQueueTest, Reset) {
  auto pic1 = base::MakeRefCounted<CodecPicture>();
  auto pic2 = base::MakeRefCounted<CodecPicture>();
  auto frame1 = VideoFrame::CreateEOSFrame();
  auto frame2 = VideoFrame::CreateEOSFrame();

  output_queue_.SchedulePicture(pic2);
  output_queue_.FulfillPicture(pic1, frame1);

  EXPECT_CALL(*this, OnFlush(StatusEq(DecoderStatus::Codes::kAborted)));

  Flush();
  output_queue_.Reset(DecoderStatus::Codes::kAborted);

  EXPECT_CALL(*this, OnOutput(_)).Times(0);

  output_queue_.FulfillPicture(pic2, frame2);
  output_queue_.SchedulePicture(pic1);

  task_environment_.RunUntilIdle();
}

}  // namespace media
