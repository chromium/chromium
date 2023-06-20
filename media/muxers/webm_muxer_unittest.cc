// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/muxers/webm_muxer.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/ranges/algorithm.h"
#include "base/test/task_environment.h"
#include "media/base/audio_codecs.h"
#include "media/base/audio_parameters.h"
#include "media/base/channel_layout.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_tracks.h"
#include "media/base/mock_media_log.h"
#include "media/base/stream_parser.h"
#include "media/base/stream_parser_buffer.h"
#include "media/base/text_track_config.h"
#include "media/base/video_codecs.h"
#include "media/base/video_frame.h"
#include "media/formats/webm/webm_stream_parser.h"
#include "media/muxers/live_webm_muxer_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::Not;
using ::testing::Pair;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;
using ::testing::Sequence;
using ::testing::TestWithParam;
using ::testing::UnorderedElementsAre;
using ::testing::ValuesIn;
using ::testing::WithArgs;

namespace media {

struct TestParams {
  VideoCodec video_codec;
  AudioCodec audio_codec;
  size_t num_video_tracks;
  size_t num_audio_tracks;
};

class WebmMuxerTest : public TestWithParam<TestParams> {
 public:
  WebmMuxerTest()
      : webm_muxer_(std::make_unique<WebmMuxer>(
            GetParam().audio_codec,
            GetParam().num_video_tracks,
            GetParam().num_audio_tracks,
            std::make_unique<LiveWebmMuxerDelegate>(
                base::BindRepeating(&WebmMuxerTest::WriteCallback,
                                    base::Unretained(this))))) {
    EXPECT_EQ(webm_muxer_->delegate_->Position(), 0);
    const mkvmuxer::int64 kRandomNewPosition = 333;
    EXPECT_EQ(webm_muxer_->delegate_->Position(kRandomNewPosition), -1);
    EXPECT_FALSE(webm_muxer_->delegate_->Seekable());
  }

  WebmMuxerTest(const WebmMuxerTest&) = delete;
  WebmMuxerTest& operator=(const WebmMuxerTest&) = delete;

  MOCK_METHOD(void, WriteCallback, (base::StringPiece));

  void SaveEncodedDataLen(const base::StringPiece& encoded_data) {
    last_encoded_length_ = encoded_data.size();
    accumulated_position_ += encoded_data.size();
  }

  mkvmuxer::int64 GetWebmMuxerPosition() const {
    return webm_muxer_->delegate_->Position();
  }

  mkvmuxer::Segment::Mode GetWebmSegmentMode() const {
    return webm_muxer_->segment_.mode();
  }

  mkvmuxer::int32 WebmMuxerWrite(const void* buf, mkvmuxer::uint32 len) {
    return webm_muxer_->delegate_->Write(buf, len);
  }

  Muxer::VideoParameters GetVideoParameters(scoped_refptr<VideoFrame> frame) {
    Muxer::VideoParameters parameters(*frame);
    parameters.codec = GetParam().video_codec;
    return parameters;
  }

  mkvmuxer::Colour* GetVideoTrackColor() const {
    mkvmuxer::VideoTrack* const video_track =
        reinterpret_cast<mkvmuxer::VideoTrack*>(
            webm_muxer_->segment_.GetTrackByNumber(
                webm_muxer_->video_track_index_));
    return video_track->colour();
  }

  std::unique_ptr<WebmMuxer> webm_muxer_;

  size_t last_encoded_length_ = 0;
  int64_t accumulated_position_ = 0;
};

// Checks that the WriteCallback is called with appropriate params when
// WebmMuxer::Write() method is called.
TEST_P(WebmMuxerTest, Write) {
  const base::StringPiece encoded_data("abcdefghijklmnopqrstuvwxyz");

  EXPECT_CALL(*this, WriteCallback(encoded_data));
  WebmMuxerWrite(encoded_data.data(), encoded_data.size());

  EXPECT_EQ(GetWebmMuxerPosition(), static_cast<int64_t>(encoded_data.size()));
}

TEST_P(WebmMuxerTest,
       HandlesMuxerErrorInPassingEncodedFramesWithAudioThenVideo) {
  auto video_params =
      GetVideoParameters(VideoFrame::CreateBlackFrame(gfx::Size(160, 80)));
  const std::string encoded_data("abcdefghijklmnopqrstuvwxyz");
  media::AudioParameters audio_params(
      media::AudioParameters::Format::AUDIO_PCM_LOW_LATENCY,
      ChannelLayoutConfig::Mono(),
      /*sample_rate=*/48000,
      /*frames_per_buffer=*/480);
  // Force an error in libwebm and expect OnEncodedVideo to fail.
  webm_muxer_->ForceOneLibWebmErrorForTesting();
  base::TimeTicks now = base::TimeTicks::Now();
  bool audio_success = !GetParam().num_audio_tracks ||
                       webm_muxer_->OnEncodedAudio(audio_params, encoded_data,
                                                   absl::nullopt, now);
  bool video_success =
      !GetParam().num_video_tracks ||
      webm_muxer_->OnEncodedVideo(video_params, encoded_data, std::string(),
                                  now + base::Milliseconds(1),
                                  /*is_key_frame=*/true);
  EXPECT_FALSE(audio_success && video_success);
}

TEST_P(WebmMuxerTest,
       HandlesMuxerErrorInPassingEncodedFramesWithVideoThenAudio) {
  auto video_params =
      GetVideoParameters(VideoFrame::CreateBlackFrame(gfx::Size(160, 80)));
  const std::string encoded_data("abcdefghijklmnopqrstuvwxyz");
  media::AudioParameters audio_params(
      media::AudioParameters::Format::AUDIO_PCM_LOW_LATENCY,
      ChannelLayoutConfig::Mono(),
      /*sample_rate=*/48000,
      /*frames_per_buffer=*/480);
  // Force an error in libwebm and expect OnEncodedVideo to fail.
  webm_muxer_->ForceOneLibWebmErrorForTesting();
  base::TimeTicks now = base::TimeTicks::Now();
  bool video_success =
      !GetParam().num_video_tracks ||
      webm_muxer_->OnEncodedVideo(video_params, encoded_data, std::string(),
                                  now + base::Milliseconds(1),
                                  /*is_key_frame=*/true);
  bool audio_success = !GetParam().num_audio_tracks ||
                       webm_muxer_->OnEncodedAudio(audio_params, encoded_data,
                                                   absl::nullopt, now);
  EXPECT_FALSE(audio_success && video_success);
}

// This test sends two frames and checks that the WriteCallback is called with
// appropriate params in both cases.
TEST_P(WebmMuxerTest, OnEncodedVideoTwoFrames) {
  if (GetParam().num_audio_tracks > 0)
    return;

  const gfx::Size frame_size(160, 80);
  const scoped_refptr<VideoFrame> video_frame =
      VideoFrame::CreateBlackFrame(frame_size);
  const std::string encoded_data("abcdefghijklmnopqrstuvwxyz");

  EXPECT_CALL(*this, WriteCallback(_))
      .Times(AtLeast(1))
      .WillRepeatedly(
          WithArgs<0>(Invoke(this, &WebmMuxerTest::SaveEncodedDataLen)));
  EXPECT_TRUE(webm_muxer_->OnEncodedVideo(
      GetVideoParameters(video_frame), encoded_data, std::string(),
      base::TimeTicks::Now(), false /* keyframe */));

  // First time around WriteCallback() is pinged a number of times to write the
  // Matroska header, but at the end it dumps |encoded_data|.
  EXPECT_EQ(last_encoded_length_, encoded_data.size());
  EXPECT_EQ(GetWebmMuxerPosition(), accumulated_position_);
  EXPECT_GE(GetWebmMuxerPosition(), static_cast<int64_t>(last_encoded_length_));
  EXPECT_EQ(GetWebmSegmentMode(), mkvmuxer::Segment::kLive);

  const int64_t begin_of_second_block = accumulated_position_;
  EXPECT_CALL(*this, WriteCallback(_))
      .Times(AtLeast(1))
      .WillRepeatedly(
          WithArgs<0>(Invoke(this, &WebmMuxerTest::SaveEncodedDataLen)));
  EXPECT_TRUE(webm_muxer_->OnEncodedVideo(
      GetVideoParameters(video_frame), encoded_data, std::string(),
      base::TimeTicks::Now(), false /* keyframe */));

  // The second time around the callbacks should include a SimpleBlock header,
  // namely the track index, a timestamp and a flags byte, for a total of 6B.
  EXPECT_EQ(last_encoded_length_, encoded_data.size());
  EXPECT_EQ(GetWebmMuxerPosition(), accumulated_position_);
  const uint32_t kSimpleBlockSize = 6u;
  EXPECT_EQ(static_cast<int64_t>(begin_of_second_block + kSimpleBlockSize +
                                 encoded_data.size()),
            accumulated_position_);
}

// This test sends two transparent frames and checks that the WriteCallback is
// called with appropriate params in both cases.
TEST_P(WebmMuxerTest, OnEncodedVideoTwoAlphaFrames) {
  if (GetParam().num_audio_tracks > 0)
    return;

  const gfx::Size frame_size(160, 80);
  const scoped_refptr<VideoFrame> video_frame =
      VideoFrame::CreateTransparentFrame(frame_size);
  const std::string encoded_data("abcdefghijklmnopqrstuvwxyz");
  const std::string alpha_encoded_data("ijklmnopqrstuvwxyz");

  InSequence s;
  EXPECT_CALL(*this, WriteCallback(_))
      .Times(AtLeast(1))
      .WillRepeatedly(
          WithArgs<0>(Invoke(this, &WebmMuxerTest::SaveEncodedDataLen)));
  EXPECT_TRUE(webm_muxer_->OnEncodedVideo(
      GetVideoParameters(video_frame), encoded_data, alpha_encoded_data,
      base::TimeTicks::Now(), true /* keyframe */));

  EXPECT_EQ(GetWebmMuxerPosition(), accumulated_position_);
  EXPECT_GE(GetWebmMuxerPosition(), static_cast<int64_t>(last_encoded_length_));
  EXPECT_EQ(GetWebmSegmentMode(), mkvmuxer::Segment::kLive);

  const int64_t begin_of_second_block = accumulated_position_;
  EXPECT_CALL(*this, WriteCallback(_))
      .Times(AtLeast(1))
      .WillRepeatedly(
          WithArgs<0>(Invoke(this, &WebmMuxerTest::SaveEncodedDataLen)));
  EXPECT_TRUE(webm_muxer_->OnEncodedVideo(
      GetVideoParameters(video_frame), encoded_data, alpha_encoded_data,
      base::TimeTicks::Now(), false /* keyframe */));

  EXPECT_EQ(GetWebmMuxerPosition(), accumulated_position_);
  // Alpha introduces additional elements to be written, see
  // mkvmuxer::WriteBlock().
  const uint32_t kBlockGroupSize = 2u;
  const uint32_t kSimpleBlockSize = 6u;
  const uint32_t kAdditionsSize = 13u;
  EXPECT_EQ(static_cast<int64_t>(begin_of_second_block + kBlockGroupSize +
                                 kSimpleBlockSize + encoded_data.size() +
                                 kAdditionsSize + alpha_encoded_data.size()),
            accumulated_position_);
}

TEST_P(WebmMuxerTest, OnEncodedAudioTwoFrames) {
  if (GetParam().num_video_tracks > 0)
    return;

  const int sample_rate = 48000;
  const int frames_per_buffer = 480;
  media::AudioParameters audio_params(
      media::AudioParameters::Format::AUDIO_PCM_LOW_LATENCY,
      ChannelLayoutConfig::Mono(), sample_rate, frames_per_buffer);

  const std::string encoded_data("abcdefghijklmnopqrstuvwxyz");

  EXPECT_CALL(*this, WriteCallback(_))
      .Times(AtLeast(1))
      .WillRepeatedly(
          WithArgs<0>(Invoke(this, &WebmMuxerTest::SaveEncodedDataLen)));
  EXPECT_TRUE(webm_muxer_->OnEncodedAudio(
      audio_params, encoded_data, absl::nullopt, base::TimeTicks::Now()));

  // First time around WriteCallback() is pinged a number of times to write the
  // Matroska header, but at the end it dumps |encoded_data|.
  EXPECT_EQ(last_encoded_length_, encoded_data.size());
  EXPECT_EQ(GetWebmMuxerPosition(), accumulated_position_);
  EXPECT_GE(GetWebmMuxerPosition(), static_cast<int64_t>(last_encoded_length_));
  EXPECT_EQ(GetWebmSegmentMode(), mkvmuxer::Segment::kLive);

  const int64_t begin_of_second_block = accumulated_position_;
  EXPECT_CALL(*this, WriteCallback(_))
      .Times(AtLeast(1))
      .WillRepeatedly(
          WithArgs<0>(Invoke(this, &WebmMuxerTest::SaveEncodedDataLen)));
  EXPECT_TRUE(webm_muxer_->OnEncodedAudio(
      audio_params, encoded_data, absl::nullopt, base::TimeTicks::Now()));

  // The second time around the callbacks should include a SimpleBlock header,
  // namely the track index, a timestamp and a flags byte, for a total of 6B.
  EXPECT_EQ(last_encoded_length_, encoded_data.size());
  EXPECT_EQ(GetWebmMuxerPosition(), accumulated_position_);
  const uint32_t kSimpleBlockSize = 6u;
  EXPECT_EQ(static_cast<int64_t>(begin_of_second_block + kSimpleBlockSize +
                                 encoded_data.size()),
            accumulated_position_);
}

TEST_P(WebmMuxerTest, ColorSpaceREC709IsPropagatedToTrack) {
  Muxer::VideoParameters params(gfx::Size(1, 1), 0, media::VideoCodec::kVP9,
                                gfx::ColorSpace::CreateREC709());
  webm_muxer_->OnEncodedVideo(params, "abab", {}, base::TimeTicks::Now(),
                              true /* keyframe */);
  mkvmuxer::Colour* colour = GetVideoTrackColor();
  EXPECT_EQ(colour->primaries(), mkvmuxer::Colour::kIturBt709P);
  EXPECT_EQ(colour->transfer_characteristics(), mkvmuxer::Colour::kIturBt709Tc);
  EXPECT_EQ(colour->matrix_coefficients(), mkvmuxer::Colour::kBt709);
  EXPECT_EQ(colour->range(), mkvmuxer::Colour::kBroadcastRange);
}

TEST_P(WebmMuxerTest, ColorSpaceExtendedSRGBIsPropagatedToTrack) {
  Muxer::VideoParameters params(
      gfx::Size(1, 1), 0, media::VideoCodec::kVP9,
      gfx::ColorSpace(
          gfx::ColorSpace::PrimaryID::BT709, gfx::ColorSpace::TransferID::SRGB,
          gfx::ColorSpace::MatrixID::BT709, gfx::ColorSpace::RangeID::LIMITED));
  webm_muxer_->OnEncodedVideo(params, "banana", {}, base::TimeTicks::Now(),
                              true /* keyframe */);
  mkvmuxer::Colour* colour = GetVideoTrackColor();
  EXPECT_EQ(colour->primaries(), mkvmuxer::Colour::kIturBt709P);
  EXPECT_EQ(colour->transfer_characteristics(), mkvmuxer::Colour::kIec6196621);
  EXPECT_EQ(colour->matrix_coefficients(), mkvmuxer::Colour::kBt709);
  EXPECT_EQ(colour->range(), mkvmuxer::Colour::kBroadcastRange);
}

TEST_P(WebmMuxerTest, ColorSpaceHDR10IsPropagatedToTrack) {
  Muxer::VideoParameters params(
      gfx::Size(1, 1), 0, media::VideoCodec::kVP9,
      gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT2020,
                      gfx::ColorSpace::TransferID::PQ,
                      gfx::ColorSpace::MatrixID::BT2020_NCL,
                      gfx::ColorSpace::RangeID::LIMITED));
  webm_muxer_->OnEncodedVideo(params, "cafebabe", {}, base::TimeTicks::Now(),
                              true /* keyframe */);
  mkvmuxer::Colour* colour = GetVideoTrackColor();
  EXPECT_EQ(colour->primaries(), mkvmuxer::Colour::kIturBt2020);
  EXPECT_EQ(colour->transfer_characteristics(), mkvmuxer::Colour::kSmpteSt2084);
  EXPECT_EQ(colour->matrix_coefficients(),
            mkvmuxer::Colour::kBt2020NonConstantLuminance);
  EXPECT_EQ(colour->range(), mkvmuxer::Colour::kBroadcastRange);
}

TEST_P(WebmMuxerTest, ColorSpaceFullRangeHDR10IsPropagatedToTrack) {
  Muxer::VideoParameters params(
      gfx::Size(1, 1), 0, media::VideoCodec::kVP9,
      gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT2020,
                      gfx::ColorSpace::TransferID::PQ,
                      gfx::ColorSpace::MatrixID::BT2020_NCL,
                      gfx::ColorSpace::RangeID::FULL));
  webm_muxer_->OnEncodedVideo(params, "beatles", {}, base::TimeTicks::Now(),
                              true /* keyframe */);
  mkvmuxer::Colour* colour = GetVideoTrackColor();
  EXPECT_EQ(colour->range(), mkvmuxer::Colour::kFullRange);
}

// This test verifies that when video data comes before audio data, we save the
// encoded video frames and add it to the video track when audio data arrives.
TEST_P(WebmMuxerTest, VideoIsStoredWhileWaitingForAudio) {
  // This test is only relevant if we have both kinds of tracks.
  if (GetParam().num_video_tracks == 0 || GetParam().num_audio_tracks == 0)
    return;

  // First send a video keyframe.
  const gfx::Size frame_size(160, 80);
  const scoped_refptr<VideoFrame> video_frame =
      VideoFrame::CreateBlackFrame(frame_size);
  const std::string encoded_video("thisisanencodedvideopacket");

  // Timestamp: 0 (video origin)
  webm_muxer_->OnEncodedVideo(GetVideoParameters(video_frame), encoded_video,
                              std::string(), base::TimeTicks(),
                              true /* keyframe */);

  // Timestamp: video origin + X
  webm_muxer_->OnEncodedVideo(
      GetVideoParameters(video_frame), encoded_video, std::string(),
      base::TimeTicks() + base::Milliseconds(1), false /* keyframe */);

  // Timestamp: video origin + X + Y
  webm_muxer_->OnEncodedVideo(
      GetVideoParameters(video_frame), encoded_video, std::string(),
      base::TimeTicks() + base::Milliseconds(2), false /* keyframe */);

  const int sample_rate = 48000;
  const int frames_per_buffer = 480;
  media::AudioParameters audio_params(
      media::AudioParameters::Format::AUDIO_PCM_LOW_LATENCY,
      ChannelLayoutConfig::Mono(), sample_rate, frames_per_buffer);
  const std::string encoded_audio("thisisanencodedaudiopacket");

  // Timestamped frames should come as:
  // [video origin, video origin + X, video origin + X + Y, audio origin]
  Sequence s;
  EXPECT_CALL(*this, WriteCallback(Eq(encoded_video))).Times(1).InSequence(s);
  EXPECT_CALL(*this, WriteCallback(Eq(encoded_video))).Times(1).InSequence(s);
  EXPECT_CALL(*this, WriteCallback(Eq(encoded_video))).Times(1).InSequence(s);
  EXPECT_CALL(*this, WriteCallback(Eq(encoded_audio))).Times(1).InSequence(s);

  // We'll also get lots of other header-related stuff.
  EXPECT_CALL(*this, WriteCallback(
                         AllOf(Not(Eq(encoded_video)), Not(Eq(encoded_audio)))))
      .Times(AnyNumber());

  // Timestamp: 0 (audio origin)
  webm_muxer_->OnEncodedAudio(audio_params, encoded_audio, absl::nullopt,
                              base::TimeTicks() + base::Milliseconds(3));
  webm_muxer_.reset();
}

const TestParams kTestCases[] = {
    {VideoCodec::kVP8, AudioCodec::kOpus, 1 /* num_video_tracks */,
     0 /*num_audio_tracks*/},
    {VideoCodec::kVP8, AudioCodec::kOpus, 0, 1},
    {VideoCodec::kVP8, AudioCodec::kOpus, 1, 1},
    {VideoCodec::kVP9, AudioCodec::kOpus, 1, 0},
    {VideoCodec::kVP9, AudioCodec::kOpus, 0, 1},
    {VideoCodec::kVP9, AudioCodec::kOpus, 1, 1},
    {VideoCodec::kH264, AudioCodec::kOpus, 1, 0},
    {VideoCodec::kH264, AudioCodec::kOpus, 0, 1},
    {VideoCodec::kH264, AudioCodec::kOpus, 1, 1},
    {VideoCodec::kVP8, AudioCodec::kPCM, 0, 1},
    {VideoCodec::kVP8, AudioCodec::kPCM, 1, 1},
    {VideoCodec::kAV1, AudioCodec::kOpus, 1, 0},
    {VideoCodec::kAV1, AudioCodec::kOpus, 0, 1},
    {VideoCodec::kAV1, AudioCodec::kOpus, 1, 1},
};

INSTANTIATE_TEST_SUITE_P(All, WebmMuxerTest, ValuesIn(kTestCases));

class WebmMuxerTestUnparametrized : public testing::Test {
 public:
  WebmMuxerTestUnparametrized()
      : environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        webm_muxer_(std::make_unique<WebmMuxer>(
            AudioCodec::kOpus,
            /*has_audio=*/true,
            /*has_video=*/true,
            std::make_unique<LiveWebmMuxerDelegate>(base::BindRepeating(
                &WebmMuxerTestUnparametrized::SaveChunkAndInvokeWriteCallback,
                base::Unretained(this))))) {}

  bool Parse() {
    if (got_video_) {
      // Add one more video buffer to force WebMStreamParser to not hold back
      // the last added video buffer due to missing duration of it.
      AddVideoAtOffset(kSentinelVideoBufferTimestampMs, /*is_key_frame=*/false);
    }
    // Force any final flushes.
    webm_muxer_ = nullptr;
    media::WebMStreamParser parser;
    parser.Init(
        base::BindOnce(&WebmMuxerTestUnparametrized::OnInit,
                       base::Unretained(this)),
        base::BindRepeating(&WebmMuxerTestUnparametrized::OnNewConfig,
                            base::Unretained(this)),
        base::BindRepeating(&WebmMuxerTestUnparametrized::OnNewBuffers,
                            base::Unretained(this)),
        /*ignore_text_tracks=*/true,
        base::BindRepeating(
            &WebmMuxerTestUnparametrized::OnEncryptedMediaInitData,
            base::Unretained(this)),
        base::BindRepeating(&WebmMuxerTestUnparametrized::OnNewMediaSegment,
                            base::Unretained(this)),
        base::BindRepeating(&WebmMuxerTestUnparametrized::OnEndMediaSegment,
                            base::Unretained(this)),
        &media_log_);
    if (!parser.AppendToParseBuffer(muxed_data_.data(), muxed_data_.size())) {
      return false;
    }

    // Run the segment parser loop one time with the full size of the appended
    // data to ensure the parser has had a chance to parse all the appended
    // bytes. As a result, kSuccessHasMoreData is considered a failure since
    // there should be no uninspected data remaining after the parse.
    return StreamParser::ParseStatus::kSuccess ==
           parser.Parse(muxed_data_.size());
  }

  void AddVideoAtOffset(int system_timestamp_offset_ms, bool is_key_frame) {
    Muxer::VideoParameters params(gfx::Size(1, 1), 0, media::VideoCodec::kVP8,
                                  gfx::ColorSpace());
    webm_muxer_->OnEncodedVideo(
        params, "video_at_offset", "",
        base::TimeTicks() + base::Milliseconds(system_timestamp_offset_ms),
        is_key_frame);
    got_video_ = true;
  }

  void AddAudioAtOffsetWithDuration(int system_timestamp_offset_ms,
                                    int duration_ms) {
    int frame_rate_hz = 48000;
    int frames_per_buffer = frame_rate_hz * duration_ms / 1000;
    media::AudioParameters audio_params(
        media::AudioParameters::Format::AUDIO_PCM_LOW_LATENCY,
        ChannelLayoutConfig::Mono(), frame_rate_hz, frames_per_buffer);
    webm_muxer_->OnEncodedAudio(
        audio_params, "audio_at_offset", absl::nullopt,
        base::TimeTicks() + base::Milliseconds(system_timestamp_offset_ms));
  }

  MOCK_METHOD(void, OnWrite, ());

  base::test::TaskEnvironment environment_;
  std::unique_ptr<WebmMuxer> webm_muxer_;
  std::map<int, std::vector<int>> buffer_timestamps_ms_;

 protected:
  // media::StreamParser callbacks.
  void OnInit(const media::StreamParser::InitParameters&) {}
  bool OnNewConfig(std::unique_ptr<media::MediaTracks> tracks,
                   const media::StreamParser::TextTrackConfigMap&) {
    return true;
  }
  bool OnNewBuffers(const media::StreamParser::BufferQueueMap& map) {
    for (const auto& [track_id, queue] : map) {
      for (const auto& stream_parser_buffer : queue) {
        buffer_timestamps_ms_[track_id].push_back(
            stream_parser_buffer->timestamp().InMilliseconds());
      }
    }
    return true;
  }
  void OnEncryptedMediaInitData(EmeInitDataType, const std::vector<uint8_t>&) {}
  void OnNewMediaSegment() {}
  void OnEndMediaSegment() {}

 private:
  static constexpr int kSentinelVideoBufferTimestampMs = 1000000;

  void SaveChunkAndInvokeWriteCallback(base::StringPiece chunk) {
    OnWrite();
    base::ranges::copy(chunk, std::back_inserter(muxed_data_));
  }

  // Muxed data gets saved here. The content is guaranteed to be finalized first
  // when webm_muxer_ has been destroyed.
  std::vector<unsigned char> muxed_data_;

  // Mock media log for WebM parser.
  media::MockMediaLog media_log_;

  // True after a call to AddVideoAtOffset.
  bool got_video_ = false;
};

TEST_F(WebmMuxerTestUnparametrized, MuxerCompensatesForPausedTimeWithVideo) {
  AddVideoAtOffset(123, /*is_key_frame=*/true);
  webm_muxer_->Pause();
  environment_.FastForwardBy(base::Milliseconds(200));
  webm_muxer_->Resume();
  AddVideoAtOffset(123 + 266, /*is_key_frame=*/false);
  EXPECT_TRUE(Parse());
  EXPECT_THAT(buffer_timestamps_ms_,
              ElementsAre(Pair(1, ElementsAre(0, /*266 - 200=*/66))));
}

TEST_F(WebmMuxerTestUnparametrized, MuxerCompensatesForPausedTimeWithAudio) {
  AddAudioAtOffsetWithDuration(234, 10);
  webm_muxer_->Pause();
  environment_.FastForwardBy(base::Milliseconds(666));
  webm_muxer_->Resume();
  AddAudioAtOffsetWithDuration(234 + 686, 10);
  EXPECT_TRUE(Parse());
  EXPECT_THAT(buffer_timestamps_ms_,
              ElementsAre(Pair(1, ElementsAre(0, /*686 - 666=*/20))));
}

TEST_F(WebmMuxerTestUnparametrized,
       MuxerCompensatesForPausedTimeWithAudioAndVideo) {
  AddAudioAtOffsetWithDuration(234, 10);
  AddVideoAtOffset(234 + 1, /*is_key_frame=*/true);
  webm_muxer_->Pause();
  environment_.FastForwardBy(base::Milliseconds(300));
  webm_muxer_->Resume();
  AddAudioAtOffsetWithDuration(234 + 321, 10);
  AddVideoAtOffset(234 + 315, /*is_key_frame=*/false);
  EXPECT_TRUE(Parse());
  EXPECT_THAT(buffer_timestamps_ms_,
              UnorderedElementsAre(Pair(1, ElementsAre(0, /*321 - 300=*/21)),
                                   Pair(2, ElementsAre(1, /*315 - 300=*/15))));
}

TEST_F(WebmMuxerTestUnparametrized,
       MuxerCompensatesForPausedTimeBeforeAudioVideo) {
  webm_muxer_->Pause();
  environment_.FastForwardBy(base::Milliseconds(100));
  webm_muxer_->Resume();
  AddAudioAtOffsetWithDuration(50, 10);
  AddVideoAtOffset(65, /*is_key_frame=*/true);
  AddAudioAtOffsetWithDuration(60, 10);
  AddVideoAtOffset(70, /*is_key_frame=*/false);
  EXPECT_TRUE(Parse());
  EXPECT_THAT(buffer_timestamps_ms_,
              UnorderedElementsAre(Pair(1, ElementsAre(0, 10)),
                                   Pair(2, ElementsAre(15, 20))));
}

TEST_F(WebmMuxerTestUnparametrized, HoldsDataUntilDurationExpiry) {
  webm_muxer_->SetMaximumDurationToForceDataOutput(base::Milliseconds(200));
  AddVideoAtOffset(0, /*is_key_frame=*/true);
  AddAudioAtOffsetWithDuration(0, 10);
  // Mute video. The muxer will hold on to audio data after this until the max
  // data output duration is expired.
  webm_muxer_->SetLiveAndEnabled(/*track_live_and_enabled=*/false,
                                 /*is_video=*/true);
  EXPECT_CALL(*this, OnWrite).Times(0);
  AddAudioAtOffsetWithDuration(10, 10);
  AddAudioAtOffsetWithDuration(20, 10);
  AddAudioAtOffsetWithDuration(30, 10);
  AddAudioAtOffsetWithDuration(40, 10);
  Mock::VerifyAndClearExpectations(this);
  environment_.FastForwardBy(base::Milliseconds(200));
  EXPECT_CALL(*this, OnWrite).Times(AtLeast(1));
  AddAudioAtOffsetWithDuration(50, 10);
  Mock::VerifyAndClearExpectations(this);
  // Stop mock dispatch from happening too late in the WebmMuxer's destructor.
  webm_muxer_ = nullptr;
}

TEST_F(WebmMuxerTestUnparametrized, DurationExpiryLimitedByMaxFrequency) {
  webm_muxer_->SetMaximumDurationToForceDataOutput(base::Milliseconds(
      50));  // This value is below the minimum limit of 100 ms.
  AddVideoAtOffset(0, /*is_key_frame=*/true);
  AddAudioAtOffsetWithDuration(0, 10);
  // Mute video. The muxer will hold on to audio data after this until the max
  // data output duration is expired.
  webm_muxer_->SetLiveAndEnabled(/*track_live_and_enabled=*/false,
                                 /*is_video=*/true);
  EXPECT_CALL(*this, OnWrite).Times(0);
  AddAudioAtOffsetWithDuration(10, 10);
  AddAudioAtOffsetWithDuration(20, 10);
  AddAudioAtOffsetWithDuration(30, 10);
  AddAudioAtOffsetWithDuration(40, 10);
  Mock::VerifyAndClearExpectations(this);
  environment_.FastForwardBy(base::Milliseconds(100));
  EXPECT_CALL(*this, OnWrite).Times(AtLeast(1));
  AddAudioAtOffsetWithDuration(50, 10);
  Mock::VerifyAndClearExpectations(this);
  // Stop mock dispatch from happening too late in the WebmMuxer's destructor.
  webm_muxer_ = nullptr;
}

}  // namespace media
