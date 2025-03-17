// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/encoding/size_adaptable_video_encoder_base.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "media/base/mock_filters.h"
#include "media/base/video_encoder_metrics_provider.h"
#include "media/base/video_frame.h"
#include "media/cast/cast_environment.h"
#include "media/cast/common/sender_encoded_frame.h"
#include "media/cast/constants.h"
#include "media/cast/encoding/video_encoder.h"
#include "media/cast/test/mock_video_encoder.h"
#include "media/cast/test/test_with_cast_environment.h"
#include "media/cast/test/utility/default_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

using ::testing::_;
using ::testing::ByMove;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;

namespace media::cast {

namespace {

void ExpectEncodeAndSaveCallback(MockVideoEncoder& mock_encoder,
                                 scoped_refptr<media::VideoFrame> video_frame,
                                 VideoEncoder::FrameEncodedCallback& callback) {
  EXPECT_CALL(mock_encoder, EncodeVideoFrame(video_frame, _, _))
      .WillOnce([&callback](auto, auto, VideoEncoder::FrameEncodedCallback cb) {
        callback = std::move(cb);
        return true;
      });
}

}  // namespace

class SizeAdaptableVideoEncoderBaseTest : public TestWithCastEnvironment {
 protected:
  using MockStatusChangeCallback =
      base::MockCallback<base::RepeatingCallback<void(OperationalStatus)>>;

  class MockSizeAdaptableVideoEncoder : public SizeAdaptableVideoEncoderBase {
   public:
    MockSizeAdaptableVideoEncoder(
        const scoped_refptr<CastEnvironment>& cast_environment,
        const FrameSenderConfig& video_config,
        std::unique_ptr<VideoEncoderMetricsProvider> metrics_provider,
        StatusChangeCallback status_change_cb)
        : SizeAdaptableVideoEncoderBase(cast_environment,
                                        video_config,
                                        std::move(metrics_provider),
                                        status_change_cb) {}

    void UpdateEncoderStatus(OperationalStatus status) {
      std::move(CreateEncoderStatusChangeCallback()).Run(status);
    }

    MOCK_METHOD(std::unique_ptr<VideoEncoder>, CreateEncoder, (), (override));

    MOCK_METHOD(void,
                OnEncoderReplaced,
                (VideoEncoder * replacement_encoder),
                (override));
  };

  SizeAdaptableVideoEncoderBaseTest() { CreateEncoder(); }

  ~SizeAdaptableVideoEncoderBaseTest() override {
    GetMainThreadTaskRunner()->DeleteSoon(FROM_HERE,
                                          std::move(mock_status_change_cb_));
    GetMainThreadTaskRunner()->PostTask(FROM_HERE, QuitClosure());
    RunUntilQuit();
  }

  void CreateEncoder() {
    auto video_config = GetDefaultVideoSenderConfig();
    mock_status_change_cb_ =
        std::make_unique<NiceMock<MockStatusChangeCallback>>();

    auto metrics_provider = std::make_unique<MockVideoEncoderMetricsProvider>();
    encoder_ = std::make_unique<MockSizeAdaptableVideoEncoder>(
        cast_environment(), video_config, std::move(metrics_provider),
        mock_status_change_cb_->Get());
  }

  MockSizeAdaptableVideoEncoder& encoder() { return *encoder_; }
  MockStatusChangeCallback& status_change_cb() {
    return *mock_status_change_cb_;
  }

 private:
  std::unique_ptr<MockSizeAdaptableVideoEncoder> encoder_;

  // This is a nice mock so that we avoid duplicating the relatively complicated
  // code that tests the status change order.
  std::unique_ptr<NiceMock<MockStatusChangeCallback>> mock_status_change_cb_;
};

TEST_F(SizeAdaptableVideoEncoderBaseTest, InitialStatusIsEncoderInitialized) {
  EXPECT_CALL(status_change_cb(), Run(Eq(STATUS_INITIALIZED)))
      .WillOnce(base::test::RunOnceClosure((QuitClosure())));

  RunUntilQuit();
}

// Tests the status change order behaves as described in the
// SizeAdaptableVideoEncoderBase header file.
TEST_F(SizeAdaptableVideoEncoderBaseTest, EncoderStatusChangeCallbackFlow) {
  // First, a note about quit closure usage. GMock wants ALL of the EXPECT_CALLs
  // to occur before there are ANY usages of the mock type. And the
  // TaskEnvironment also does not allow you to ever have more than one valid
  // quit closure. Since we have multiple expectations that we desire to use a
  // quit closure with, pass the closure by reference to the invoked lambda and
  // update it between calls to RunUntilQuit().
  base::OnceClosure quit_closure = QuitClosure();

  // At this point in the test, since encoder() has already been created, a
  // task to update the status to STATUS_INITIALIZED has already been posted.
  {
    InSequence seq;
    EXPECT_CALL(status_change_cb(), Run(Eq(STATUS_INITIALIZED)))
        .WillOnce([&quit_closure]() { std::move(quit_closure).Run(); });

    EXPECT_CALL(status_change_cb(), Run(Eq(STATUS_CODEC_REINIT_PENDING)));
    EXPECT_CALL(status_change_cb(), Run(Eq(STATUS_INITIALIZED)))
        .WillOnce([&quit_closure]() { std::move(quit_closure).Run(); });
  }

  // Flush the task queue to ensure that the async STATUS_INITIALIZED gets ran
  // before the synchronous call to update the status to
  // STATUS_CODEC_REINIT_PENDING as part of creating the backing encoder in
  // EncodeVideoFrame.
  RunUntilQuit();
  quit_closure = QuitClosure();

  auto video_frame = VideoFrame::CreateBlackFrame(gfx::Size(100, 100));
  auto mock_encoder = std::make_unique<MockVideoEncoder>();
  VideoEncoder::FrameEncodedCallback callback;
  ExpectEncodeAndSaveCallback(*mock_encoder, video_frame, callback);
  EXPECT_CALL(encoder(), CreateEncoder)
      .WillOnce(Return(ByMove(std::move(mock_encoder))));
  EXPECT_CALL(encoder(), OnEncoderReplaced(_));

  // The first frame is ALWAYS dropped for this implementation.
  EXPECT_FALSE(
      encoder().EncodeVideoFrame(video_frame, NowTicks(), base::DoNothing()));

  // Now that the first frame has been sent, the encoder should be pending
  // initialization.
  encoder().UpdateEncoderStatus(STATUS_INITIALIZED);

  // Now that the backing encoder has updated its status to initialized, the
  // next video frame should be accepted.
  EXPECT_TRUE(
      encoder().EncodeVideoFrame(video_frame, NowTicks(), base::DoNothing()));

  // NOTE: the callback has to be run after the method finishes (so that the
  // enqueue is recorded).
  std::move(callback).Run(nullptr);
  RunUntilQuit();
}

TEST_F(SizeAdaptableVideoEncoderBaseTest,
       FrameShouldBeDroppedIfBackingEncoderNotInitialized) {
  auto video_frame = VideoFrame::CreateBlackFrame(gfx::Size(100, 100));

  auto mock_encoder = std::make_unique<MockVideoEncoder>();
  EXPECT_CALL(encoder(), CreateEncoder)
      .WillOnce(Return(ByMove(std::move(mock_encoder))));

  EXPECT_CALL(status_change_cb(), Run(Eq(STATUS_CODEC_REINIT_PENDING)));
  EXPECT_CALL(status_change_cb(), Run(Eq(STATUS_INITIALIZED)))
      .WillOnce(base::test::RunOnceClosure(QuitClosure()));

  EXPECT_FALSE(
      encoder().EncodeVideoFrame(video_frame, NowTicks(), base::DoNothing()));
  EXPECT_FALSE(
      encoder().EncodeVideoFrame(video_frame, NowTicks(), base::DoNothing()));

  RunUntilQuit();
}

TEST_F(SizeAdaptableVideoEncoderBaseTest,
       FrameShouldBeEncodedIfBackingEncoderIsInitialized) {
  auto video_frame = VideoFrame::CreateBlackFrame(gfx::Size(100, 100));

  auto mock_encoder = std::make_unique<MockVideoEncoder>();
  VideoEncoder::FrameEncodedCallback callback;
  ExpectEncodeAndSaveCallback(*mock_encoder, video_frame, callback);
  EXPECT_CALL(encoder(), CreateEncoder)
      .WillOnce(Return(ByMove(std::move(mock_encoder))));
  EXPECT_CALL(encoder(), OnEncoderReplaced(_));

  // A comprehensive test of status_change_cb() is above. Here, just make
  // sure it gets initialized twice so we can use to to invoke a quit closure.
  EXPECT_CALL(status_change_cb(), Run(Eq(STATUS_CODEC_REINIT_PENDING)));
  EXPECT_CALL(status_change_cb(), Run(Eq(STATUS_INITIALIZED)))
      .WillOnce(Return())
      .WillOnce(base::test::RunOnceClosure(QuitClosure()));

  // The first frame is ALWAYS dropped for this implementation.
  EXPECT_FALSE(
      encoder().EncodeVideoFrame(video_frame, NowTicks(), base::DoNothing()));

  encoder().UpdateEncoderStatus(STATUS_INITIALIZED);

  EXPECT_TRUE(
      encoder().EncodeVideoFrame(video_frame, NowTicks(), base::DoNothing()));

  // NOTE: the callback has to be run after the method finishes (so that the
  // enqueue is recorded).
  std::move(callback).Run(nullptr);
  RunUntilQuit();
}

// Pass a valid, non zero-sized video frame to the encoder, and ensure that we
// still invoke the frame change callback if the encoder returns a nullptr
// `SenderEncodedFrame` object.
//
// For more information on this test, see issuetracker.google.com/393880773.
TEST_F(SizeAdaptableVideoEncoderBaseTest,
       CallbackShouldBeCalledEvenIfEncodedFrameIsEmpty) {
  // Create a valid video frame with non-empty size.
  auto video_frame = VideoFrame::CreateBlackFrame(gfx::Size(100, 100));

  auto mock_encoder = std::make_unique<MockVideoEncoder>();
  VideoEncoder::FrameEncodedCallback callback;
  ExpectEncodeAndSaveCallback(*mock_encoder, video_frame, callback);
  EXPECT_CALL(encoder(), CreateEncoder)
      .WillOnce(Return(ByMove(std::move(mock_encoder))));
  EXPECT_CALL(encoder(), OnEncoderReplaced(_));
  EXPECT_CALL(status_change_cb(), Run(Eq(STATUS_CODEC_REINIT_PENDING)));
  EXPECT_CALL(status_change_cb(), Run(Eq(STATUS_INITIALIZED)))
      .WillOnce(Return())
      .WillOnce(base::test::RunOnceClosure(QuitClosure()));

  // The first frame is ALWAYS dropped for this implementation.
  EXPECT_FALSE(
      encoder().EncodeVideoFrame(video_frame, NowTicks(), base::DoNothing()));

  encoder().UpdateEncoderStatus(STATUS_INITIALIZED);
  auto mock_frame_encoded_cb = base::MockCallback<
      base::OnceCallback<void(std::unique_ptr<SenderEncodedFrame>)>>();
  EXPECT_CALL(mock_frame_encoded_cb, Run(_));
  EXPECT_TRUE(encoder().EncodeVideoFrame(video_frame, NowTicks(),
                                         mock_frame_encoded_cb.Get()));

  // Although the original video frame was not empty, the encoder failed and
  // returned an empty frame.
  std::move(callback).Run(nullptr);
  RunUntilQuit();
}

TEST_F(SizeAdaptableVideoEncoderBaseTest, IsSizeAdaptable) {
  base::OnceClosure quit_closure = QuitClosure();

  // At this point in the test, since encoder() has already been created, a
  // task to update the status to STATUS_INITIALIZED has already been posted.
  {
    InSequence seq;
    EXPECT_CALL(status_change_cb(), Run(Eq(STATUS_INITIALIZED)))
        .WillOnce([&quit_closure]() { std::move(quit_closure).Run(); });

    // The first frame being sent should result in a re-init.
    EXPECT_CALL(status_change_cb(), Run(Eq(STATUS_CODEC_REINIT_PENDING)));

    // And then the encoder should successfully initialize.
    EXPECT_CALL(status_change_cb(), Run(Eq(STATUS_INITIALIZED)))
        .WillOnce([&quit_closure]() { std::move(quit_closure).Run(); });

    // The third frame should result in a new reinit due to the size being
    // different.
    EXPECT_CALL(status_change_cb(), Run(Eq(STATUS_CODEC_REINIT_PENDING)));
    EXPECT_CALL(status_change_cb(), Run(Eq(STATUS_INITIALIZED)))
        .WillOnce([&quit_closure]() { std::move(quit_closure).Run(); });
  }

  RunUntilQuit();
  quit_closure = QuitClosure();

  auto video_frame = VideoFrame::CreateBlackFrame(gfx::Size(640, 480));
  auto larger_video_frame = VideoFrame::CreateBlackFrame(gfx::Size(1280, 720));

  auto mock_encoder = std::make_unique<MockVideoEncoder>();
  auto larger_mock_encoder = std::make_unique<MockVideoEncoder>();

  VideoEncoder::FrameEncodedCallback callback;
  ExpectEncodeAndSaveCallback(*mock_encoder, video_frame, callback);
  VideoEncoder::FrameEncodedCallback larger_callback;
  ExpectEncodeAndSaveCallback(*larger_mock_encoder, larger_video_frame,
                              larger_callback);

  EXPECT_CALL(encoder(), CreateEncoder)
      .WillOnce(Return(ByMove(std::move(mock_encoder))))
      .WillOnce(Return(ByMove(std::move(larger_mock_encoder))));
  EXPECT_CALL(encoder(), OnEncoderReplaced(_)).Times(2);

  EXPECT_FALSE(
      encoder().EncodeVideoFrame(video_frame, NowTicks(), base::DoNothing()));
  encoder().UpdateEncoderStatus(STATUS_INITIALIZED);
  EXPECT_TRUE(
      encoder().EncodeVideoFrame(video_frame, NowTicks(), base::DoNothing()));
  std::move(callback).Run(nullptr);
  RunUntilQuit();
  quit_closure = QuitClosure();

  // Encoding a larger frame should return false and cause a reinitialization.
  // NOTE: the callback has to be run after the method finishes (so that the
  // enqueue is recorded).
  EXPECT_FALSE(encoder().EncodeVideoFrame(larger_video_frame, NowTicks(),
                                          base::DoNothing()));
  encoder().UpdateEncoderStatus(STATUS_INITIALIZED);
  EXPECT_TRUE(encoder().EncodeVideoFrame(larger_video_frame, NowTicks(),
                                         base::DoNothing()));
  std::move(larger_callback).Run(nullptr);
  RunUntilQuit();
}

}  // namespace media::cast
