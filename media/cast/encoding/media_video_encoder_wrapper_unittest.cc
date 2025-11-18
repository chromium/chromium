// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/encoding/media_video_encoder_wrapper.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "gpu/command_buffer/client/test_shared_image_interface.h"
#include "media/base/encoder_status.h"
#include "media/base/mock_filters.h"
#include "media/base/video_codecs.h"
#include "media/base/video_encoder_metrics_provider.h"
#include "media/base/video_frame.h"
#include "media/cast/cast_environment.h"
#include "media/cast/common/rtp_time.h"
#include "media/cast/common/sender_encoded_frame.h"
#include "media/cast/test/fake_video_encode_accelerator_factory.h"
#include "media/cast/test/test_with_cast_environment.h"
#include "media/cast/test/utility/default_config.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "media/video/mock_gpu_video_accelerator_factories.h"
#include "media_video_encoder_wrapper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "video_encoder.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;

namespace media::cast {

namespace {

constexpr VideoCodecProfile kProfile = VP8PROFILE_ANY;
constexpr gfx::Size kSize(1920, 1080);

enum class FrameType { kKey, kIntermediate };

struct FrameInfo {
  FrameType frame_type;
  gfx::Size size;
  base::TimeTicks capture_begin_time;
  base::TimeTicks capture_end_time;
  base::TimeTicks reference_time;
  base::TimeDelta frame_duration;
};

// NOTE: media::VideoFrame doesn't have a frame_type, so it is ignored for this
// matcher.
MATCHER_P(FrameInfosAreEqual, frame_info, "") {
  return arg->visible_rect().size() == frame_info.size &&
         arg->metadata().capture_begin_time == frame_info.capture_begin_time &&
         arg->metadata().capture_end_time == frame_info.capture_end_time &&
         arg->timestamp() == frame_info.reference_time - base::TimeTicks() &&
         arg->metadata().frame_duration == frame_info.frame_duration;
}

MATCHER_P(FrameTypeIsEqual, frame_type, "") {
  const bool key_frame = frame_type == FrameType::kKey;
  *result_listener << "where the frame type is "
                   << (key_frame ? "key" : "intermediate");
  return arg.key_frame == key_frame && !arg.quantizer &&
         arg.reference_buffers.empty() && !arg.update_buffer;
}

MATCHER_P(OptionsAreEqual, options, "") {
  return arg.frame_size == options.frame_size && arg.bitrate == options.bitrate;
}

scoped_refptr<VideoFrame> CreateVideoFrame(const FrameInfo& frame_info) {
  auto video_frame =
      VideoFrame::CreateFrame(PIXEL_FORMAT_I420, kSize, gfx::Rect(kSize), kSize,
                              frame_info.reference_time - base::TimeTicks());
  video_frame->metadata().capture_begin_time = frame_info.capture_begin_time;
  video_frame->metadata().capture_end_time = frame_info.capture_end_time;
  video_frame->metadata().frame_duration = frame_info.frame_duration;
  return video_frame;
}

}  // namespace

class MediaVideoEncoderWrapperTest : public TestWithCastEnvironment {
 protected:
  MediaVideoEncoderWrapperTest() {
    sii_ = base::MakeRefCounted<gpu::TestSharedImageInterface>();
    mock_gpu_factories_ =
        std::make_unique<MockGpuVideoAcceleratorFactories>(sii_.get());

    EXPECT_CALL(status_change_cb_, Run(STATUS_INITIALIZED))
        .Times(testing::AtLeast(1));
    auto mock_encoder = std::make_unique<NiceMock<MockVideoEncoder>>();
    mock_encoder_ = mock_encoder.get();
    encoder_ = std::make_unique<MediaVideoEncoderWrapper>(
        cast_environment(), config_,
        std::make_unique<NiceMock<MockVideoEncoderMetricsProvider>>(),
        status_change_cb_.Get(), mock_gpu_factories_.get());
    encoder_->SetEncoderForTesting(std::move(mock_encoder));
  }

  ~MediaVideoEncoderWrapperTest() override = default;

  void SetEncoderAsInitialized() {
    // We simulate the encoder as initialized for the purpose of testing.
    encoder_->OnEncoderStatus(EncoderStatus::Codes::kOk);
  }

  FrameInfo CreateFrameInfo(FrameType frame_type) const {
    const base::TimeTicks now = NowTicks();
    return FrameInfo{.frame_type = frame_type,
                     .size = kSize,
                     .capture_begin_time = now,
                     .capture_end_time = now + base::Milliseconds(10),
                     .reference_time = now + base::Milliseconds(20),
                     .frame_duration = base::Milliseconds(30)};
  }

  // Sets up expectations for a video frame.
  void ExpectVideoFrameEncoded(const FrameInfo& frame_info) {
    EXPECT_CALL(*mock_encoder_,
                Encode(FrameInfosAreEqual(frame_info),
                       FrameTypeIsEqual(frame_info.frame_type), _))
        .WillOnce([&](scoped_refptr<VideoFrame> frame,
                      const media::VideoEncoder::EncodeOptions& options,
                      media::VideoEncoder::EncoderStatusCB done) {
          std::move(done).Run(EncoderStatus::Codes::kOk);
        });
  }

  // Expect that the encoder gets initialized. Note this only occurs if a frame
  // is sent, so we also need the type of frame here.
  void ExpectEncoderInitialized(
      media::VideoEncoder::OutputCB* output_cb = nullptr) {
    const media::VideoEncoder::Options options = GetOptions();
    EXPECT_CALL(*mock_encoder_,
                Initialize(kProfile, OptionsAreEqual(options), _, _, _))
        .WillOnce([output_cb](VideoCodecProfile profile,
                              const media::VideoEncoder::Options& options,
                              media::VideoEncoder::EncoderInfoCB info,
                              media::VideoEncoder::OutputCB output,
                              media::VideoEncoder::EncoderStatusCB done) {
          std::move(info).Run(VideoEncoderInfo());
          std::move(done).Run(EncoderStatus::Codes::kOk);

          // The first frame should be returned after initialization is
          // complete, and should always be a keyframe.
          media::VideoEncoderOutput encoder_output;
          encoder_output.key_frame = true;
          output.Run(std::move(encoder_output), std::nullopt);
          if (output_cb) {
            *output_cb = std::move(output);
          }
        });
    EXPECT_CALL(*mock_encoder_, DisablePostedCallbacks());
  }

  // Encodes a video frame and returns the encoded frame. If set, executes
  // `output_cb` before entering a RunLoop.
  std::unique_ptr<SenderEncodedFrame> EncodeVideoFrame(
      const FrameInfo& frame_info,
      media::VideoEncoder::OutputCB output_cb = {}) {
    auto video_frame = CreateVideoFrame(frame_info);
    std::unique_ptr<SenderEncodedFrame> out;
    EXPECT_TRUE(encoder_->EncodeVideoFrame(
        video_frame, frame_info.reference_time,
        base::BindLambdaForTesting(
            [&, closure = QuitClosure()](
                std::unique_ptr<SenderEncodedFrame> encoded_frame) {
              out = std::move(encoded_frame);
              std::move(closure).Run();
            })));

    if (output_cb) {
      VideoEncoderOutput output;
      output.key_frame = frame_info.frame_type == FrameType::kKey;
      output.timestamp = frame_info.reference_time - base::TimeTicks();
      std::move(output_cb).Run(std::move(output), std::nullopt);
    }

    RunUntilQuit();
    return out;
  }

  media::VideoEncoder::Options GetOptions() const {
    media::VideoEncoder::Options options;
    options.bitrate = Bitrate::ConstantBitrate(
        base::checked_cast<uint32_t>(config_.start_bitrate));
    options.frame_size = kSize;
    return options;
  }

  FrameSenderConfig config_ = GetDefaultVideoSenderConfig();
  testing::NiceMock<base::MockCallback<StatusChangeCallback>> status_change_cb_;
  scoped_refptr<gpu::TestSharedImageInterface> sii_;
  std::unique_ptr<MockGpuVideoAcceleratorFactories> mock_gpu_factories_;
  std::unique_ptr<MediaVideoEncoderWrapper> encoder_;
  raw_ptr<MockVideoEncoder> mock_encoder_ = nullptr;
};

// Test that encoder initialization is called with the correct parameters.
TEST_F(MediaVideoEncoderWrapperTest, InitializeEncoderAndSendFrame) {
  ExpectEncoderInitialized();
  const FrameInfo frame_info = CreateFrameInfo(FrameType::kKey);
  ExpectVideoFrameEncoded(frame_info);

  const auto encoded_frame = EncodeVideoFrame(frame_info);
  EXPECT_NE(encoded_frame, nullptr);
  EXPECT_EQ(encoded_frame->capture_begin_time, frame_info.capture_begin_time);
  EXPECT_EQ(encoded_frame->capture_end_time, frame_info.capture_end_time);
  EXPECT_EQ(encoded_frame->reference_time, frame_info.reference_time);
  EXPECT_EQ(encoded_frame->is_key_frame, true);
  EXPECT_EQ(encoded_frame->frame_id, FrameId::first());
}

TEST_F(MediaVideoEncoderWrapperTest, SendsIntermediateFramesAfterKeyFrames) {
  media::VideoEncoder::OutputCB output_cb;
  ExpectEncoderInitialized(&output_cb);

  const FrameInfo frame_info = CreateFrameInfo(FrameType::kKey);
  ExpectVideoFrameEncoded(frame_info);
  AdvanceClock(base::Milliseconds(30));
  const FrameInfo second_frame_info = CreateFrameInfo(FrameType::kIntermediate);
  ExpectVideoFrameEncoded(second_frame_info);

  EXPECT_NE(EncodeVideoFrame(frame_info), nullptr);
  EXPECT_NE(EncodeVideoFrame(second_frame_info, output_cb), nullptr);
}

TEST_F(MediaVideoEncoderWrapperTest, CanGenerateKeyFrame) {
  media::VideoEncoder::OutputCB output_cb;
  ExpectEncoderInitialized(&output_cb);

  const FrameInfo frame_info = CreateFrameInfo(FrameType::kKey);
  ExpectVideoFrameEncoded(frame_info);
  AdvanceClock(base::Milliseconds(30));
  const FrameInfo second_frame_info = CreateFrameInfo(FrameType::kKey);
  ExpectVideoFrameEncoded(second_frame_info);

  EXPECT_NE(EncodeVideoFrame(frame_info), nullptr);
  encoder_->GenerateKeyFrame();
  EXPECT_NE(EncodeVideoFrame(second_frame_info, output_cb), nullptr);
}

TEST_F(MediaVideoEncoderWrapperTest, CanSetBitRate) {
  constexpr int kNewBitRate = 1234567;
  media::VideoEncoder::OutputCB output_cb;
  ExpectEncoderInitialized(&output_cb);

  const FrameInfo frame_info = CreateFrameInfo(FrameType::kKey);
  ExpectVideoFrameEncoded(frame_info);
  AdvanceClock(base::Milliseconds(30));
  const FrameInfo second_frame_info = CreateFrameInfo(FrameType::kIntermediate);
  ExpectVideoFrameEncoded(second_frame_info);

  EXPECT_CALL(*mock_encoder_, Flush(_))
      .WillOnce([](media::VideoEncoder::EncoderStatusCB done) {
        std::move(done).Run(EncoderStatus::Codes::kOk);
      });

  base::OnceClosure quit_closure;
  EXPECT_CALL(*mock_encoder_, ChangeOptions(_, _, _))
      .WillOnce([&](const media::VideoEncoder::Options& options,
                    media::VideoEncoder::OutputCB output,
                    media::VideoEncoder::EncoderStatusCB done_cb) {
        EXPECT_EQ(options.bitrate,
                  Bitrate::ConstantBitrate(
                      base::checked_cast<uint32_t>(kNewBitRate)));
        std::move(done_cb).Run(EncoderStatus::Codes::kOk);
        std::move(quit_closure).Run();
      });

  EXPECT_NE(EncodeVideoFrame(frame_info), nullptr);

  quit_closure = QuitClosure();
  encoder_->SetBitRate(kNewBitRate);
  RunUntilQuit();

  // At this point, we should have posted the task to run
  // MediaVideoEncoderWrapper::OnOptionsUpdated, but it is waiting in the task
  // queue. Make sure we run the task before encoding a video frame.
  GetMainThreadTaskRunner()->PostTask(FROM_HERE, QuitClosure());
  RunUntilQuit();

  // Don't encode the second frame until we have completely updated
  // options.
  EXPECT_NE(EncodeVideoFrame(second_frame_info, output_cb), nullptr);
}

// Test that we still call `frame_encoded_callback` even if the backing encoder
// fails to encode a frame.
//
// See https://issuetracker.google.com/391901608 for motivation.
TEST_F(MediaVideoEncoderWrapperTest, StillOutputsIfFrameDroppedByEncoder) {
  const media::VideoEncoder::Options options = GetOptions();
  EXPECT_CALL(*mock_encoder_,
              Initialize(kProfile, OptionsAreEqual(options), _, _, _))
      .WillOnce([](VideoCodecProfile profile,
                   const media::VideoEncoder::Options& options,
                   media::VideoEncoder::EncoderInfoCB info,
                   media::VideoEncoder::OutputCB output,
                   media::VideoEncoder::EncoderStatusCB done) {
        std::move(info).Run(VideoEncoderInfo());
        std::move(done).Run(EncoderStatus::Codes::kOk);
      });
  EXPECT_CALL(*mock_encoder_, DisablePostedCallbacks());

  const FrameInfo frame_info = CreateFrameInfo(FrameType::kKey);
  EXPECT_CALL(*mock_encoder_,
              Encode(FrameInfosAreEqual(frame_info),
                     FrameTypeIsEqual(frame_info.frame_type), _))
      .WillOnce([&](scoped_refptr<VideoFrame> frame,
                    const media::VideoEncoder::EncodeOptions& options,
                    media::VideoEncoder::EncoderStatusCB done) {
        std::move(done).Run(
            EncoderStatus::Codes::kEncoderInitializeNeverCompleted);
      });

  // We should have a nullptr output, but what's important is that we have
  // output. If the encoder implementation does not invoke the output callback,
  // this will hang forever.
  EXPECT_EQ(EncodeVideoFrame(frame_info), nullptr);
}

// Ensure that the encoder wrapper can handle multiple, sequential calls to
// the SetBitRate method without crashing. For motivation, see
// crbug.com/457353867.
TEST_F(MediaVideoEncoderWrapperTest, CanHandleMultiplePendingUpdates) {
  constexpr int kNewBitRate1 = 1234567;
  constexpr int kNewBitRate2 = 2345678;
  constexpr int kNewBitRate3 = 3456789;
  media::VideoEncoder::OutputCB output_cb;
  ExpectEncoderInitialized(&output_cb);

  const FrameInfo frame_info = CreateFrameInfo(FrameType::kKey);
  ExpectVideoFrameEncoded(frame_info);
  AdvanceClock(base::Milliseconds(30));
  const FrameInfo second_frame_info = CreateFrameInfo(FrameType::kIntermediate);
  ExpectVideoFrameEncoded(second_frame_info);

  EXPECT_CALL(*mock_encoder_, Flush(_))
      .Times(3)
      .WillRepeatedly([](media::VideoEncoder::EncoderStatusCB done) {
        std::move(done).Run(EncoderStatus::Codes::kOk);
      });

  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*mock_encoder_, ChangeOptions(_, _, _))
      .Times(3)
      .WillRepeatedly([&](const media::VideoEncoder::Options& options,
                          media::VideoEncoder::OutputCB output,
                          media::VideoEncoder::EncoderStatusCB done_cb) {
        if (options.bitrate ==
            Bitrate::ConstantBitrate(
                base::checked_cast<uint32_t>(kNewBitRate3))) {
          std::move(quit_closure).Run();
        }
        std::move(done_cb).Run(EncoderStatus::Codes::kOk);
      });

  EXPECT_NE(EncodeVideoFrame(frame_info), nullptr);

  encoder_->SetBitRate(kNewBitRate1);
  encoder_->SetBitRate(kNewBitRate2);
  encoder_->SetBitRate(kNewBitRate3);
  run_loop.Run();

  // At this point, we should have posted the tasks to run
  // MediaVideoEncoderWrapper::OnOptionsUpdated, but they are waiting in the
  // task queue. Make sure we run the tasks before encoding a video frame.
  GetMainThreadTaskRunner()->PostTask(FROM_HERE, QuitClosure());
  RunUntilQuit();
  GetMainThreadTaskRunner()->PostTask(FROM_HERE, QuitClosure());
  RunUntilQuit();
  GetMainThreadTaskRunner()->PostTask(FROM_HERE, QuitClosure());
  RunUntilQuit();

  // Don't encode the second frame until we have completely updated
  // options.
  EXPECT_NE(EncodeVideoFrame(second_frame_info, output_cb), nullptr);
}

}  // namespace media::cast
