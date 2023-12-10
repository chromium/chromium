// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "media/muxers/muxer_timestamp_adapter.h"

#include <map>
#include <memory>

#include "base/strings/string_piece.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "media/base/audio_parameters.h"
#include "media/muxers/muxer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::AtLeast;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::Pair;
using ::testing::Return;
using ::testing::Sequence;
using ::testing::StrEq;
using ::testing::VariantWith;

constexpr int kAudio = 0;
constexpr int kVideo = 1;

using MediaTimestamps =
    std::vector<std::pair</*media type*/ int, /*timestamp_ms*/ int>>;

struct TestParams {
  bool has_video;
  bool has_audio;
};

class MockMuxer : public Muxer {
 public:
  MOCK_METHOD(bool, Flush, (), (override));
  MOCK_METHOD(bool,
              PutFrame,
              (EncodedFrame frame, base::TimeDelta relative_timestamp),
              (override));
};

class SuccessfulMuxer : public Muxer {
 public:
  SuccessfulMuxer(std::unique_ptr<MockMuxer> muxer,
                  MediaTimestamps& put_timestamps)
      : muxer_(std::move(muxer)), put_timestamps_(&put_timestamps) {}

  bool Flush() override {
    muxer_->Flush();
    return true;
  }

  bool PutFrame(EncodedFrame frame,
                base::TimeDelta relative_timestamp) override {
    int media_type =
        absl::get_if<AudioParameters>(&frame.params) ? kAudio : kVideo;
    put_timestamps_->emplace_back(media_type,
                                  relative_timestamp.InMilliseconds());
    muxer_->PutFrame(std::move(frame), relative_timestamp);
    return true;
  }

 private:
  std::unique_ptr<MockMuxer> muxer_;
  raw_ptr<MediaTimestamps> put_timestamps_;
};

class MuxerTimestampAdapterTestBase {
 public:
  struct Frame {
    Frame& WithData(base::StringPiece v) {
      data = v;
      return *this;
    }
    Frame& WithAlphaData(base::StringPiece v) {
      alpha_data = v;
      return *this;
    }
    Frame& AsKeyframe() {
      keyframe = true;
      return *this;
    }
    Frame& WithTimestamp(int relative_timestamp_ms) {
      timestamp = base::TimeTicks() + base::Milliseconds(relative_timestamp_ms);
      return *this;
    }
    std::string data = "data";
    std::string alpha_data;
    bool keyframe = false;
    base::TimeTicks timestamp;
  };

  MuxerTimestampAdapterTestBase()
      : environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~MuxerTimestampAdapterTestBase() { muxer_ = nullptr; }

  void CreateAdapter(bool has_video, bool has_audio) {
    auto muxer_holder = std::make_unique<MockMuxer>();
    muxer_ = muxer_holder.get();
    adapter_ = std::make_unique<MuxerTimestampAdapter>(std::move(muxer_holder),
                                                       has_video, has_audio);
  }

  void CreateAdapterWithSuccessfulPut(bool has_video, bool has_audio) {
    auto muxer_holder = std::make_unique<MockMuxer>();
    muxer_ = muxer_holder.get();
    auto successful_muxer = std::make_unique<SuccessfulMuxer>(
        std::move(muxer_holder), put_timestamps_);
    adapter_ = std::make_unique<MuxerTimestampAdapter>(
        std::move(successful_muxer), has_video, has_audio);
  }

  bool PutAudioFrame(const Frame& frame) {
    media::AudioParameters audio_params(
        media::AudioParameters::Format::AUDIO_PCM_LOW_LATENCY,
        ChannelLayoutConfig::Mono(),
        /*sample_rate=*/48000,
        /*frames_per_buffer=*/480);
    return adapter_->OnEncodedAudio(audio_params, frame.data, absl::nullopt,
                                    frame.timestamp);
  }

  bool PutVideoFrame(const Frame& frame) {
    auto video_params = Muxer::VideoParameters(
        *VideoFrame::CreateBlackFrame(gfx::Size(160, 80)));
    return adapter_->OnEncodedVideo(video_params, frame.data, frame.alpha_data,
                                    absl::nullopt, frame.timestamp,
                                    frame.keyframe);
  }

  MediaTimestamps put_timestamps_;
  base::test::TaskEnvironment environment_;
  raw_ptr<MockMuxer> muxer_ = nullptr;
  std::unique_ptr<MuxerTimestampAdapter> adapter_;
};

class MuxerTimestampAdapterTest : public MuxerTimestampAdapterTestBase,
                                  public ::testing::Test {};

TEST_F(MuxerTimestampAdapterTest, ForwardsAudioSamples) {
  CreateAdapterWithSuccessfulPut(/*has_video=*/false, /*has_audio=*/true);
  InSequence s;
  EXPECT_CALL(*muxer_,
              PutFrame(AllOf(Field(&Muxer::EncodedFrame::data, StrEq("f1")),
                             Field(&Muxer::EncodedFrame::params,
                                   VariantWith<AudioParameters>(_))),
                       base::Milliseconds(0)));
  EXPECT_CALL(*muxer_,
              PutFrame(AllOf(Field(&Muxer::EncodedFrame::data, StrEq("f2")),
                             Field(&Muxer::EncodedFrame::params,
                                   VariantWith<AudioParameters>(_))),
                       base::Milliseconds(1)));
  PutAudioFrame(Frame().WithData("f1").WithTimestamp(1));
  PutAudioFrame(Frame().WithData("f2").WithTimestamp(2));
}

TEST_F(MuxerTimestampAdapterTest, ForwardsVideoSamples) {
  CreateAdapterWithSuccessfulPut(/*has_video=*/true, /*has_audio=*/false);
  InSequence s;
  EXPECT_CALL(
      *muxer_,
      PutFrame(AllOf(Field(&Muxer::EncodedFrame::data, StrEq("f1")),
                     Field(&Muxer::EncodedFrame::alpha_data, StrEq("a1")),
                     Field(&Muxer::EncodedFrame::params,
                           VariantWith<Muxer::VideoParameters>(_))),
               base::Milliseconds(0)));
  EXPECT_CALL(
      *muxer_,
      PutFrame(AllOf(Field(&Muxer::EncodedFrame::data, StrEq("f2")),
                     Field(&Muxer::EncodedFrame::alpha_data, StrEq("a2")),
                     Field(&Muxer::EncodedFrame::params,
                           VariantWith<Muxer::VideoParameters>(_))),
               base::Milliseconds(1)));
  PutVideoFrame(Frame().WithData("f1").WithAlphaData("a1").WithTimestamp(2));
  PutVideoFrame(Frame().WithData("f2").WithAlphaData("a2").WithTimestamp(3));
}

TEST_F(MuxerTimestampAdapterTest, ForwardsAudioVideoSamples) {
  CreateAdapterWithSuccessfulPut(/*has_video=*/true, /*has_audio=*/true);
  InSequence s;
  EXPECT_CALL(
      *muxer_,
      PutFrame(AllOf(Field(&Muxer::EncodedFrame::data, StrEq("f1")),
                     Field(&Muxer::EncodedFrame::alpha_data, StrEq("a1")),
                     Field(&Muxer::EncodedFrame::params,
                           VariantWith<Muxer::VideoParameters>(_))),
               base::Milliseconds(0)));
  EXPECT_CALL(*muxer_,
              PutFrame(AllOf(Field(&Muxer::EncodedFrame::data, StrEq("f2")),
                             Field(&Muxer::EncodedFrame::params,
                                   VariantWith<AudioParameters>(_))),
                       base::Milliseconds(1)));
  PutVideoFrame(Frame().WithData("f1").WithAlphaData("a1").WithTimestamp(3));
  PutAudioFrame(Frame().WithData("f2").WithTimestamp(4));
}

TEST_F(MuxerTimestampAdapterTest, HandlesMuxerErrorInAudioThenVideo) {
  CreateAdapter(/*has_video=*/true, /*has_audio=*/true);
  EXPECT_CALL(*muxer_, PutFrame).WillOnce(Return(false)).WillOnce(Return(true));
  bool audio_success = PutAudioFrame(Frame().WithTimestamp(1));
  bool video_success = PutVideoFrame(Frame().WithTimestamp(2));
  EXPECT_FALSE(audio_success && video_success);
}

TEST_F(MuxerTimestampAdapterTest, HandlesMuxerErrorInVideoThenAudio) {
  CreateAdapter(/*has_video=*/true, /*has_audio=*/true);
  EXPECT_CALL(*muxer_, PutFrame).WillOnce(Return(false)).WillOnce(Return(true));
  bool video_success = PutVideoFrame(Frame().WithTimestamp(1));
  bool audio_success = PutAudioFrame(Frame().WithTimestamp(2));
  EXPECT_FALSE(audio_success && video_success);
}

TEST_F(MuxerTimestampAdapterTest, IgnoresEmptyVideoFrame) {
  CreateAdapterWithSuccessfulPut(/*has_video=*/true, /*has_audio=*/false);
  EXPECT_CALL(*muxer_, PutFrame).Times(0);
  PutVideoFrame(Frame().WithData(""));
}

TEST_F(MuxerTimestampAdapterTest, VideoKeptWhileWaitingForAudio) {
  CreateAdapterWithSuccessfulPut(/*has_video=*/true, /*has_audio=*/true);
  InSequence s;
  PutVideoFrame(Frame().WithTimestamp(1).AsKeyframe());
  PutVideoFrame(Frame().WithTimestamp(2));
  PutVideoFrame(Frame().WithTimestamp(3));
  EXPECT_CALL(*muxer_, PutFrame(Field(&Muxer::EncodedFrame::params,
                                      VariantWith<Muxer::VideoParameters>(_)),
                                base::Milliseconds(0)));
  EXPECT_CALL(*muxer_, PutFrame(Field(&Muxer::EncodedFrame::params,
                                      VariantWith<Muxer::VideoParameters>(_)),
                                base::Milliseconds(1)));
  EXPECT_CALL(*muxer_, PutFrame(Field(&Muxer::EncodedFrame::params,
                                      VariantWith<Muxer::VideoParameters>(_)),
                                base::Milliseconds(2)));
  EXPECT_CALL(*muxer_, PutFrame(Field(&Muxer::EncodedFrame::params,
                                      VariantWith<AudioParameters>(_)),
                                base::Milliseconds(3)));
  PutAudioFrame(Frame().WithTimestamp(4));
  muxer_ = nullptr;
  adapter_ = nullptr;
}

TEST_F(MuxerTimestampAdapterTest, CompensatesForPausedTimeWithVideo) {
  CreateAdapterWithSuccessfulPut(/*has_video=*/true, /*has_audio=*/true);
  PutVideoFrame(Frame().WithTimestamp(123).AsKeyframe());
  adapter_->Pause();
  environment_.FastForwardBy(base::Milliseconds(200));
  adapter_->Resume();
  PutVideoFrame(Frame().WithTimestamp(123 + 266));
  muxer_ = nullptr;
  adapter_ = nullptr;
  EXPECT_THAT(put_timestamps_,
              ElementsAre(Pair(kVideo, 0), Pair(kVideo, /*266 - 200=*/66)));
}

TEST_F(MuxerTimestampAdapterTest, CompensatesForPausedTimeWithAudio) {
  CreateAdapterWithSuccessfulPut(/*has_video=*/true, /*has_audio=*/true);
  PutAudioFrame(Frame().WithTimestamp(234));
  adapter_->Pause();
  environment_.FastForwardBy(base::Milliseconds(666));
  adapter_->Resume();
  PutAudioFrame(Frame().WithTimestamp(234 + 686));
  muxer_ = nullptr;
  adapter_ = nullptr;
  EXPECT_THAT(put_timestamps_,
              ElementsAre(Pair(kAudio, 0), Pair(kAudio, /*686 - 666=*/20)));
}

TEST_F(MuxerTimestampAdapterTest, CompensatesForPausedTimeWithAudioAndVideo) {
  CreateAdapterWithSuccessfulPut(/*has_video=*/true, /*has_audio=*/true);
  PutAudioFrame(Frame().WithTimestamp(234));
  PutVideoFrame(Frame().WithTimestamp(234 + 1).AsKeyframe());
  adapter_->Pause();
  environment_.FastForwardBy(base::Milliseconds(300));
  adapter_->Resume();
  PutAudioFrame(Frame().WithTimestamp(234 + 321));
  PutVideoFrame(Frame().WithTimestamp(234 + 315));
  muxer_ = nullptr;
  adapter_ = nullptr;
  EXPECT_THAT(put_timestamps_, ElementsAre(Pair(kAudio, 0), Pair(kVideo, 1),
                                           Pair(kVideo, /*315 - 300=*/15),
                                           Pair(kAudio, /*321 - 300=*/21)));
}

TEST_F(MuxerTimestampAdapterTest, ReleasesAudioDataWhileVideoMuted) {
  CreateAdapterWithSuccessfulPut(/*has_video=*/true, /*has_audio=*/true);
  PutVideoFrame(Frame().WithTimestamp(1).AsKeyframe());
  PutAudioFrame(Frame().WithTimestamp(1));
  // Mute video. The muxer will start releasing audio data assuming no video
  // samples will emerge while muted.
  adapter_->SetLiveAndEnabled(/*track_live_and_enabled=*/false,
                              /*is_video=*/true);
  // The last audio frame goes too.
  EXPECT_CALL(*muxer_, PutFrame).Times(4 + 1);
  PutAudioFrame(Frame().WithTimestamp(2));
  PutAudioFrame(Frame().WithTimestamp(3));
  PutAudioFrame(Frame().WithTimestamp(4));
  PutAudioFrame(Frame().WithTimestamp(5));
  Mock::VerifyAndClearExpectations(muxer_);
}

TEST_F(MuxerTimestampAdapterTest, ReleasesVideoDataWhileAudioMuted) {
  CreateAdapterWithSuccessfulPut(/*has_video=*/true, /*has_audio=*/true);
  PutAudioFrame(Frame().WithTimestamp(1));
  PutVideoFrame(Frame().WithTimestamp(1).AsKeyframe());
  // Mute video. The muxer will start releasing audio data assuming no audio
  // samples will emerge while muted.
  adapter_->SetLiveAndEnabled(/*track_live_and_enabled=*/false,
                              /*is_video=*/false);
  // The last video frame goes too.
  EXPECT_CALL(*muxer_, PutFrame).Times(4 + 1);
  PutVideoFrame(Frame().WithTimestamp(2));
  PutVideoFrame(Frame().WithTimestamp(3));
  PutVideoFrame(Frame().WithTimestamp(4));
  PutVideoFrame(Frame().WithTimestamp(5));
  Mock::VerifyAndClearExpectations(muxer_);
}

class MuxerTimestampAdapterParametrizedTest
    : public MuxerTimestampAdapterTestBase,
      public ::testing::TestWithParam<TestParams> {
 public:
  MuxerTimestampAdapterParametrizedTest() {
    CreateAdapterWithSuccessfulPut(/*has_video=*/GetParam().has_video,
                                   /*has_audio=*/GetParam().has_audio);
  }
};

TEST_P(MuxerTimestampAdapterParametrizedTest, CallsFlushOnDestruction) {
  EXPECT_CALL(*muxer_, Flush);
  muxer_ = nullptr;
  adapter_ = nullptr;
}

TEST_P(MuxerTimestampAdapterParametrizedTest, ForwardsFlush) {
  EXPECT_CALL(*muxer_, Flush);
  adapter_->Flush();
  Mock::VerifyAndClearExpectations(muxer_);
}

const TestParams kTestCases[] = {
    {/*has_video=*/true, /*has_audio=*/false},
    {/*has_video=*/false, /*has_audio=*/false},
    {/*has_video=*/true, /*has_audio=*/false},
};

INSTANTIATE_TEST_SUITE_P(,
                         MuxerTimestampAdapterParametrizedTest,
                         ::testing::ValuesIn(kTestCases));

}  // namespace
}  // namespace media
