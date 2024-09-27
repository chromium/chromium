// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/muxers/webm_muxer.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/ranges/algorithm.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "media/base/audio_codecs.h"
#include "media/base/audio_parameters.h"
#include "media/base/channel_layout.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_tracks.h"
#include "media/base/mock_media_log.h"
#include "media/base/stream_parser.h"
#include "media/base/stream_parser_buffer.h"
#include "media/base/video_codecs.h"
#include "media/base/video_frame.h"
#include "media/formats/webm/webm_stream_parser.h"
#include "media/muxers/live_webm_muxer_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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
                                    base::Unretained(this))),
            std::nullopt)) {
    EXPECT_EQ(webm_muxer_->delegate_->Position(), 0);
    const mkvmuxer::int64 kRandomNewPosition = 333;
    EXPECT_EQ(webm_muxer_->delegate_->Position(kRandomNewPosition), -1);
    EXPECT_FALSE(webm_muxer_->delegate_->Seekable());
  }

  WebmMuxerTest(const WebmMuxerTest&) = delete;
  WebmMuxerTest& operator=(const WebmMuxerTest&) = delete;

  MOCK_METHOD(void, WriteCallback, (base::span<const uint8_t>));

  void SaveEncodedDataLen(base::span<const uint8_t> encoded_data) {
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

  void PutAudio(
      const AudioParameters& params,
      scoped_refptr<DecoderBuffer> encoded_data,
      std::optional<media::AudioEncoder::CodecDescription> codec_description) {
    webm_muxer_->PutFrame(
        Muxer::EncodedFrame{std::move(params), std::move(codec_description),
                            std::move(encoded_data)},
        now_);
    now_ = now_ + base::Milliseconds(10);
  }

  void PutVideo(
      const Muxer::VideoParameters& params,
      scoped_refptr<DecoderBuffer> encoded_data,
      std::optional<media::VideoEncoder::CodecDescription> codec_description) {
    webm_muxer_->PutFrame(
        Muxer::EncodedFrame{std::move(params), std::move(codec_description),
                            std::move(encoded_data)},
        now_);
    now_ = now_ + base::Milliseconds(30);
  }

  std::unique_ptr<WebmMuxer> webm_muxer_;

  size_t last_encoded_length_ = 0;
  int64_t accumulated_position_ = 0;
  base::TimeDelta now_ = base::Seconds(0);
};

// Checks that the WriteCallback is called with appropriate params when
// WebmMuxer::Write() method is called.
TEST_P(WebmMuxerTest, Write) {
  const std::string_view encoded_data("abcdefghijklmnopqrstuvwxyz");

  EXPECT_CALL(*this, WriteCallback(base::as_byte_span(encoded_data)));
  WebmMuxerWrite(encoded_data.data(), encoded_data.size());

  EXPECT_EQ(GetWebmMuxerPosition(), static_cast<int64_t>(encoded_data.size()));
}

// This test sends two frames and checks that the WriteCallback is called with
// appropriate params in both cases.
TEST_P(WebmMuxerTest, OnEncodedVideoTwoFrames) {
  if (GetParam().num_audio_tracks > 0)
    return;

  const gfx::Size frame_size(160, 80);
  const scoped_refptr<VideoFrame> video_frame =
      VideoFrame::CreateBlackFrame(frame_size);
  const auto encoded_data = media::DecoderBuffer::CopyFrom(
      base::as_byte_span("abcdefghijklmnopqrstuvwxyz"));
  encoded_data->set_is_key_frame(true);

  EXPECT_CALL(*this, WriteCallback(_))
      .Times(AtLeast(1))
      .WillRepeatedly(
          WithArgs<0>(Invoke(this, &WebmMuxerTest::SaveEncodedDataLen)));
  PutVideo(GetVideoParameters(video_frame), std::move(encoded_data),
           std::nullopt);

  // First time around WriteCallback() is pinged a number of times to write the
  // Matroska header, but at the end it dumps |encoded_data|.
  EXPECT_EQ(last_encoded_length_, encoded_data->size());
  EXPECT_EQ(GetWebmMuxerPosition(), accumulated_position_);
  EXPECT_GE(GetWebmMuxerPosition(), static_cast<int64_t>(last_encoded_length_));
  EXPECT_EQ(GetWebmSegmentMode(), mkvmuxer::Segment::kLive);

  const int64_t begin_of_second_block = accumulated_position_;
  EXPECT_CALL(*this, WriteCallback(_))
      .Times(AtLeast(1))
      .WillRepeatedly(
          WithArgs<0>(Invoke(this, &WebmMuxerTest::SaveEncodedDataLen)));
  encoded_data->set_is_key_frame(false);
  PutVideo(GetVideoParameters(video_frame), std::move(encoded_data),
           std::nullopt);

  // The second time around the callbacks should include a SimpleBlock header,
  // namely the track index, a timestamp and a flags byte, for a total of 6B.
  EXPECT_EQ(last_encoded_length_, encoded_data->size());
  EXPECT_EQ(GetWebmMuxerPosition(), accumulated_position_);
  const uint32_t kSimpleBlockSize = 6u;
  EXPECT_EQ(static_cast<int64_t>(begin_of_second_block + kSimpleBlockSize +
                                 encoded_data->size()),
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
  const auto encoded_data = media::DecoderBuffer::CopyFrom(
      base::as_byte_span("abcdefghijklmnopqrstuvwxyz"));
  const uint8_t alpha_data[] = "ijklmnopqrstuvwxyz";
  encoded_data->WritableSideData().alpha_data.assign(std::begin(alpha_data),
                                                     std::end(alpha_data));

  InSequence s;
  EXPECT_CALL(*this, WriteCallback(_))
      .Times(AtLeast(1))
      .WillRepeatedly(
          WithArgs<0>(Invoke(this, &WebmMuxerTest::SaveEncodedDataLen)));
  encoded_data->set_is_key_frame(true);
  PutVideo(GetVideoParameters(video_frame), std::move(encoded_data),
           std::nullopt);

  EXPECT_EQ(GetWebmMuxerPosition(), accumulated_position_);
  EXPECT_GE(GetWebmMuxerPosition(), static_cast<int64_t>(last_encoded_length_));
  EXPECT_EQ(GetWebmSegmentMode(), mkvmuxer::Segment::kLive);

  const int64_t begin_of_second_block = accumulated_position_;
  EXPECT_CALL(*this, WriteCallback(_))
      .Times(AtLeast(1))
      .WillRepeatedly(
          WithArgs<0>(Invoke(this, &WebmMuxerTest::SaveEncodedDataLen)));
  encoded_data->set_is_key_frame(false);
  PutVideo(GetVideoParameters(video_frame), std::move(encoded_data),
           std::nullopt);

  EXPECT_EQ(GetWebmMuxerPosition(), accumulated_position_);
  // Alpha introduces additional elements to be written, see
  // mkvmuxer::WriteBlock().
  const uint32_t kBlockGroupSize = 2u;
  const uint32_t kSimpleBlockSize = 6u;
  const uint32_t kAdditionsSize = 13u;
  EXPECT_EQ(static_cast<int64_t>(begin_of_second_block + kBlockGroupSize +
                                 kSimpleBlockSize + encoded_data->size() +
                                 kAdditionsSize +
                                 encoded_data->side_data()->alpha_data.size()),
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

  const auto encoded_data = media::DecoderBuffer::CopyFrom(
      base::as_byte_span("abcdefghijklmnopqrstuvwxyz"));

  EXPECT_CALL(*this, WriteCallback(_))
      .Times(AtLeast(1))
      .WillRepeatedly(
          WithArgs<0>(Invoke(this, &WebmMuxerTest::SaveEncodedDataLen)));
  PutAudio(audio_params, std::move(encoded_data), std::nullopt);

  // First time around WriteCallback() is pinged a number of times to write the
  // Matroska header, but at the end it dumps |encoded_data|.
  EXPECT_EQ(last_encoded_length_, encoded_data->size());
  EXPECT_EQ(GetWebmMuxerPosition(), accumulated_position_);
  EXPECT_GE(GetWebmMuxerPosition(), static_cast<int64_t>(last_encoded_length_));
  EXPECT_EQ(GetWebmSegmentMode(), mkvmuxer::Segment::kLive);

  const int64_t begin_of_second_block = accumulated_position_;
  EXPECT_CALL(*this, WriteCallback(_))
      .Times(AtLeast(1))
      .WillRepeatedly(
          WithArgs<0>(Invoke(this, &WebmMuxerTest::SaveEncodedDataLen)));
  PutAudio(audio_params, std::move(encoded_data), std::nullopt);

  // The second time around the callbacks should include a SimpleBlock header,
  // namely the track index, a timestamp and a flags byte, for a total of 6B.
  EXPECT_EQ(last_encoded_length_, encoded_data->size());
  EXPECT_EQ(GetWebmMuxerPosition(), accumulated_position_);
  const uint32_t kSimpleBlockSize = 6u;
  EXPECT_EQ(static_cast<int64_t>(begin_of_second_block + kSimpleBlockSize +
                                 encoded_data->size()),
            accumulated_position_);
}

TEST_P(WebmMuxerTest, ColorSpaceREC709IsPropagatedToTrack) {
  const auto encoded_data =
      media::DecoderBuffer::CopyFrom(base::as_byte_span("abab"));
  encoded_data->set_is_key_frame(true);
  Muxer::VideoParameters params(gfx::Size(1, 1), 0, media::VideoCodec::kVP9,
                                gfx::ColorSpace::CreateREC709());
  PutVideo(params, std::move(encoded_data), std::nullopt);
  mkvmuxer::Colour* colour = GetVideoTrackColor();
  EXPECT_EQ(colour->primaries(), mkvmuxer::Colour::kIturBt709P);
  EXPECT_EQ(colour->transfer_characteristics(), mkvmuxer::Colour::kIturBt709Tc);
  EXPECT_EQ(colour->matrix_coefficients(), mkvmuxer::Colour::kBt709);
  EXPECT_EQ(colour->range(), mkvmuxer::Colour::kBroadcastRange);
}

TEST_P(WebmMuxerTest, ColorSpaceExtendedSRGBIsPropagatedToTrack) {
  const auto encoded_data =
      media::DecoderBuffer::CopyFrom(base::as_byte_span("banana"));
  encoded_data->set_is_key_frame(true);
  Muxer::VideoParameters params(
      gfx::Size(1, 1), 0, media::VideoCodec::kVP9,
      gfx::ColorSpace(
          gfx::ColorSpace::PrimaryID::BT709, gfx::ColorSpace::TransferID::SRGB,
          gfx::ColorSpace::MatrixID::BT709, gfx::ColorSpace::RangeID::LIMITED));
  PutVideo(params, std::move(encoded_data), std::nullopt);
  mkvmuxer::Colour* colour = GetVideoTrackColor();
  EXPECT_EQ(colour->primaries(), mkvmuxer::Colour::kIturBt709P);
  EXPECT_EQ(colour->transfer_characteristics(), mkvmuxer::Colour::kIec6196621);
  EXPECT_EQ(colour->matrix_coefficients(), mkvmuxer::Colour::kBt709);
  EXPECT_EQ(colour->range(), mkvmuxer::Colour::kBroadcastRange);
}

TEST_P(WebmMuxerTest, ColorSpaceHDR10IsPropagatedToTrack) {
  const auto encoded_data =
      media::DecoderBuffer::CopyFrom(base::as_byte_span("latte"));
  encoded_data->set_is_key_frame(true);
  Muxer::VideoParameters params(
      gfx::Size(1, 1), 0, media::VideoCodec::kVP9,
      gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT2020,
                      gfx::ColorSpace::TransferID::PQ,
                      gfx::ColorSpace::MatrixID::BT2020_NCL,
                      gfx::ColorSpace::RangeID::LIMITED));
  PutVideo(params, std::move(encoded_data), std::nullopt);
  mkvmuxer::Colour* colour = GetVideoTrackColor();
  EXPECT_EQ(colour->primaries(), mkvmuxer::Colour::kIturBt2020);
  EXPECT_EQ(colour->transfer_characteristics(), mkvmuxer::Colour::kSmpteSt2084);
  EXPECT_EQ(colour->matrix_coefficients(),
            mkvmuxer::Colour::kBt2020NonConstantLuminance);
  EXPECT_EQ(colour->range(), mkvmuxer::Colour::kBroadcastRange);
}

TEST_P(WebmMuxerTest, ColorSpaceFullRangeHDR10IsPropagatedToTrack) {
  const auto encoded_data =
      media::DecoderBuffer::CopyFrom(base::as_byte_span("beatles"));
  encoded_data->set_is_key_frame(true);
  Muxer::VideoParameters params(
      gfx::Size(1, 1), 0, media::VideoCodec::kVP9,
      gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT2020,
                      gfx::ColorSpace::TransferID::PQ,
                      gfx::ColorSpace::MatrixID::BT2020_NCL,
                      gfx::ColorSpace::RangeID::FULL));
  PutVideo(params, std::move(encoded_data), std::nullopt);
  mkvmuxer::Colour* colour = GetVideoTrackColor();
  EXPECT_EQ(colour->range(), mkvmuxer::Colour::kFullRange);
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
      : environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void CreateMuxer(
      bool has_video,
      bool has_audio,
      std::optional<base::TimeDelta> max_data_output_interval = std::nullopt) {
    webm_muxer_ = std::make_unique<WebmMuxer>(
        AudioCodec::kOpus, has_video, has_audio,
        std::make_unique<LiveWebmMuxerDelegate>(base::BindRepeating(
            &WebmMuxerTestUnparametrized::SaveChunkAndInvokeWriteCallback,
            base::Unretained(this))),
        max_data_output_interval);
  }

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
        base::BindRepeating(
            &WebmMuxerTestUnparametrized::OnEncryptedMediaInitData,
            base::Unretained(this)),
        base::BindRepeating(&WebmMuxerTestUnparametrized::OnNewMediaSegment,
                            base::Unretained(this)),
        base::BindRepeating(&WebmMuxerTestUnparametrized::OnEndMediaSegment,
                            base::Unretained(this)),
        &media_log_);
    if (!parser.AppendToParseBuffer(base::make_span(muxed_data_))) {
      return false;
    }

    // Run the segment parser loop one time with the full size of the appended
    // data to ensure the parser has had a chance to parse all the appended
    // bytes. As a result, kSuccessHasMoreData is considered a failure since
    // there should be no uninspected data remaining after the parse.
    return StreamParser::ParseStatus::kSuccess ==
           parser.Parse(muxed_data_.size());
  }

  bool AddVideoAtOffset(int system_timestamp_offset_ms, bool is_key_frame) {
    Muxer::VideoParameters params(gfx::Size(1, 1), 0, media::VideoCodec::kVP8,
                                  gfx::ColorSpace());
    auto buffer =
        media::DecoderBuffer::CopyFrom(base::as_byte_span("video_at_offset"));
    buffer->set_is_key_frame(is_key_frame);
    bool result = webm_muxer_->PutFrame(
        Muxer::EncodedFrame{std::move(params), std::nullopt, buffer},
        base::Milliseconds(system_timestamp_offset_ms));
    got_video_ = true;
    return result;
  }

  bool AddAudioAtOffsetWithDuration(int system_timestamp_offset_ms,
                                    int duration_ms) {
    int frame_rate_hz = 48000;
    int frames_per_buffer = frame_rate_hz * duration_ms / 1000;
    media::AudioParameters audio_params(
        media::AudioParameters::Format::AUDIO_PCM_LOW_LATENCY,
        ChannelLayoutConfig::Mono(), frame_rate_hz, frames_per_buffer);
    auto buffer =
        media::DecoderBuffer::CopyFrom(base::as_byte_span("audio_at_offset"));
    buffer->set_is_key_frame(true);
    return webm_muxer_->PutFrame(
        Muxer::EncodedFrame{std::move(audio_params), std::nullopt, buffer},
        base::Milliseconds(system_timestamp_offset_ms));
  }

  MOCK_METHOD(void, OnWrite, ());

  base::test::TaskEnvironment environment_;
  std::unique_ptr<WebmMuxer> webm_muxer_;
  std::map<int, std::vector<int>> buffer_timestamps_ms_;

 protected:
  // media::StreamParser callbacks.
  void OnInit(const media::StreamParser::InitParameters&) {}
  bool OnNewConfig(std::unique_ptr<media::MediaTracks> tracks) { return true; }
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

  void SaveChunkAndInvokeWriteCallback(base::span<const uint8_t> chunk) {
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

TEST_F(WebmMuxerTestUnparametrized, HoldsAudioFramesBeforeVideo) {
  CreateMuxer(true, true);
  EXPECT_CALL(*this, OnWrite).Times(0);
  AddAudioAtOffsetWithDuration(0, 10);
  AddAudioAtOffsetWithDuration(10, 10);
  Mock::VerifyAndClearExpectations(this);
}

TEST_F(WebmMuxerTestUnparametrized, HoldsVideoFramesBeforeAudio) {
  CreateMuxer(true, true);
  EXPECT_CALL(*this, OnWrite).Times(0);
  AddVideoAtOffset(0, /*is_key_frame=*/true);
  AddVideoAtOffset(10, /*is_key_frame=*/false);
  Mock::VerifyAndClearExpectations(this);
}

TEST_F(WebmMuxerTestUnparametrized, HoldsAudioFramesUntilVideo) {
  CreateMuxer(true, true);
  AddAudioAtOffsetWithDuration(0, 10);
  AddAudioAtOffsetWithDuration(10, 10);
  AddVideoAtOffset(20, /*is_key_frame=*/true);
  ASSERT_TRUE(Parse());
  EXPECT_THAT(buffer_timestamps_ms_, ElementsAre(Pair(1, ElementsAre(0, 10)),
                                                 Pair(2, ElementsAre(20))));
}

TEST_F(WebmMuxerTestUnparametrized, HoldsVideoFramesUntilAudio) {
  CreateMuxer(true, true);
  AddVideoAtOffset(0, /*is_key_frame=*/true);
  AddVideoAtOffset(10, /*is_key_frame=*/false);
  AddAudioAtOffsetWithDuration(20, 10);
  ASSERT_TRUE(Parse());
  EXPECT_THAT(buffer_timestamps_ms_, ElementsAre(Pair(1, ElementsAre(0, 10)),
                                                 Pair(2, ElementsAre(20))));
}

TEST_F(WebmMuxerTestUnparametrized, EmitsVideoRightAway) {
  CreateMuxer(/*has_video=*/true, /*has_audio=*/false);
  EXPECT_CALL(*this, OnWrite).Times(AtLeast(1));
  AddVideoAtOffset(10, /*is_key_frame=*/true);
  ASSERT_TRUE(Parse());
  EXPECT_THAT(buffer_timestamps_ms_, ElementsAre(Pair(1, ElementsAre(10))));
}

TEST_F(WebmMuxerTestUnparametrized, EmitsAudioRightAway) {
  CreateMuxer(/*has_video=*/false, /*has_audio=*/true);
  EXPECT_CALL(*this, OnWrite).Times(AtLeast(1));
  AddAudioAtOffsetWithDuration(20, 10);
  ASSERT_TRUE(Parse());
  EXPECT_THAT(buffer_timestamps_ms_, ElementsAre(Pair(1, ElementsAre(20))));
}

TEST_F(WebmMuxerTestUnparametrized, HoldsDataUntilDurationExpiry) {
  CreateMuxer(true, true, base::Milliseconds(200));
  AddVideoAtOffset(0, /*is_key_frame=*/true);
  AddAudioAtOffsetWithDuration(0, 10);
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
  ASSERT_TRUE(Parse());
}

TEST_F(WebmMuxerTestUnparametrized, DurationExpiryLimitedByMaxFrequency) {
  CreateMuxer(true, true,
              base::Milliseconds(
                  50));  // This value is below the minimum limit of 100 ms.
  AddVideoAtOffset(0, /*is_key_frame=*/true);
  AddAudioAtOffsetWithDuration(0, 10);
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
  ASSERT_TRUE(Parse());
}

TEST_F(WebmMuxerTestUnparametrized, ForwardsAudioMuxingError) {
  CreateMuxer(/*has_video=*/false, /*has_audio=*/true);
  webm_muxer_->ForceOneLibWebmErrorForTesting();
  ASSERT_FALSE(AddAudioAtOffsetWithDuration(0, 10));
}

TEST_F(WebmMuxerTestUnparametrized, ForwardsVideoMuxingError) {
  CreateMuxer(/*has_video=*/true, /*has_audio=*/false);
  webm_muxer_->ForceOneLibWebmErrorForTesting();
  ASSERT_FALSE(AddVideoAtOffset(0, /*is_key_frame=*/true));
}

TEST_F(WebmMuxerTestUnparametrized, ForwardsAudioVideoMuxingError) {
  CreateMuxer(/*has_video=*/true, /*has_audio=*/true);
  webm_muxer_->ForceOneLibWebmErrorForTesting();
  // This returns true since no attempt to write happens before the audio
  // sample.
  ASSERT_TRUE(AddVideoAtOffset(0, /*is_key_frame=*/true));
  ASSERT_FALSE(AddAudioAtOffsetWithDuration(0, 10));
}

}  // namespace media
