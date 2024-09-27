// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/muxers/mp4_muxer.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <vector>

#include "base/big_endian.h"
#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/path_service.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/audio_parameters.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_tracks.h"
#include "media/base/mock_media_log.h"
#include "media/base/stream_parser.h"
#include "media/base/video_decoder_config.h"
#include "media/formats/mp4/es_descriptor.h"
#include "media/formats/mp4/mp4_stream_parser.h"
#include "media/muxers/mp4_muxer_delegate.h"
#include "media/muxers/mp4_type_conversion.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(USE_PROPRIETARY_CODECS)

namespace media {
namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::Optional;
using ::testing::Property;
using ::testing::Return;

constexpr uint32_t kAudioSampleRate = 44100u;

class MockDelegate : public Mp4MuxerDelegateInterface {
 public:
  MOCK_METHOD(void,
              AddVideoFrame,
              (const Muxer::VideoParameters& params,
               scoped_refptr<DecoderBuffer> encoded_data,
               std::optional<VideoEncoder::CodecDescription> codec_description,
               base::TimeTicks timestamp),
              (override));
  MOCK_METHOD(void,
              AddAudioFrame,
              (const AudioParameters& params,
               scoped_refptr<DecoderBuffer> encoded_data,
               std::optional<AudioEncoder::CodecDescription> codec_description,
               base::TimeTicks timestamp),
              (override));
  MOCK_METHOD(bool, Flush, (), (override));
  MOCK_METHOD(bool, FlushFragment, (), (override));
};

AudioEncoder::CodecDescription GetAudioCodecDescription(uint8_t data) {
  return {data};
}

VideoEncoder::CodecDescription GetVideoCodecDescription(uint8_t data) {
  return {data};
}

struct TestParam {
  bool has_video;
  bool has_audio;
};

class Mp4MuxerTest : public ::testing::TestWithParam<TestParam> {
 public:
  Mp4MuxerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        delegate_(std::make_unique<MockDelegate>()),
        delegate_ptr_(delegate_.get()) {}
  ~Mp4MuxerTest() override { delegate_ptr_ = nullptr; }

  void CreateMuxer(
      std::optional<base::TimeDelta> max_data_output_interval = std::nullopt) {
    muxer_ = std::make_unique<Mp4Muxer>(
        AudioCodec::kAAC, GetParam().has_video, GetParam().has_audio,
        std::move(delegate_), max_data_output_interval);
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MockDelegate> delegate_;
  raw_ptr<MockDelegate> delegate_ptr_;
  std::unique_ptr<Mp4Muxer> muxer_;
};

MATCHER_P(MatchBufferData, data, "decoderbuffer data matcher") {
  return arg->AsSpan() == base::as_byte_span(data);
}

TEST_P(Mp4MuxerTest, ForwardsFlush) {
  CreateMuxer();
  InSequence s;
  EXPECT_CALL(*delegate_ptr_, Flush).WillOnce(Return(false));
  EXPECT_CALL(*delegate_ptr_, Flush).WillOnce(Return(true));
  EXPECT_FALSE(muxer_->Flush());
  EXPECT_TRUE(muxer_->Flush());
}

TEST_P(Mp4MuxerTest, ForwardsFrames) {
  CreateMuxer();
  InSequence s;
  if (GetParam().has_audio) {
    AudioParameters audio_params(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                 ChannelLayoutConfig::Stereo(),
                                 kAudioSampleRate, 1000);
    std::string str1 = "a1";
    std::string str2 = "a2";
    EXPECT_CALL(
        *delegate_ptr_,
        AddAudioFrame(
            AllOf(Property(&AudioParameters::format,
                           AudioParameters::AUDIO_PCM_LOW_LATENCY),
                  Property(&AudioParameters::sample_rate, kAudioSampleRate)),
            MatchBufferData(str1), Optional(GetAudioCodecDescription(99)),
            base::TimeTicks() + base::Milliseconds(10)));
    EXPECT_CALL(*delegate_ptr_,
                AddAudioFrame(_, MatchBufferData(str2), Eq(std::nullopt),
                              base::TimeTicks() + base::Milliseconds(20)));
    auto buffer = media::DecoderBuffer::CopyFrom(base::as_byte_span(str1));
    buffer->set_is_key_frame(true);
    muxer_->PutFrame(
        Muxer::EncodedFrame{audio_params, GetAudioCodecDescription(99), buffer},
        base::Milliseconds(10));
    buffer = media::DecoderBuffer::CopyFrom(base::as_byte_span(str2));
    buffer->set_is_key_frame(true);
    muxer_->PutFrame(Muxer::EncodedFrame{audio_params, std::nullopt, buffer},
                     base::Milliseconds(20));
  }
  if (GetParam().has_video) {
    Muxer::VideoParameters video_params(gfx::Size(40, 30), 30,
                                        VideoCodec::kH264, gfx::ColorSpace());
    std::string str1 = "v1";
    std::string str2 = "v2";
    EXPECT_CALL(
        *delegate_ptr_,
        AddVideoFrame(
            AllOf(Field(&Muxer::VideoParameters::frame_rate, 30),
                  Field(&Muxer::VideoParameters::codec, VideoCodec::kH264)),
            MatchBufferData(str1), Optional(GetVideoCodecDescription(66)),
            base::TimeTicks() + base::Milliseconds(30)));
    EXPECT_CALL(*delegate_ptr_,
                AddVideoFrame(_, MatchBufferData(str2), Eq(std::nullopt),
                              base::TimeTicks() + base::Milliseconds(40)));
    auto buffer = media::DecoderBuffer::CopyFrom(base::as_byte_span(str1));
    buffer->set_is_key_frame(true);
    muxer_->PutFrame(
        Muxer::EncodedFrame{video_params, GetVideoCodecDescription(66), buffer},
        base::Milliseconds(30));
    buffer = media::DecoderBuffer::CopyFrom(base::as_byte_span(str2));
    buffer->set_is_key_frame(false);
    muxer_->PutFrame(Muxer::EncodedFrame{video_params, std::nullopt, buffer},
                     base::Milliseconds(40));
  }
}

TEST_P(Mp4MuxerTest, DoesntFlushOnInsufficientlySpacedFrames) {
  // This test needs video.
  if (!GetParam().has_video) {
    return;
  }
  CreateMuxer(base::Seconds(2));
  Muxer::VideoParameters video_params(gfx::Size(40, 30), 30, VideoCodec::kH264,
                                      gfx::ColorSpace());
  auto buffer = media::DecoderBuffer::CopyFrom(base::as_byte_span("v1"));
  buffer->set_is_key_frame(true);
  muxer_->PutFrame(
      Muxer::EncodedFrame{video_params, GetVideoCodecDescription(1), buffer},
      base::Milliseconds(0));
  task_environment_.AdvanceClock(base::Seconds(1));
  buffer = media::DecoderBuffer::CopyFrom(base::as_byte_span("v2"));
  buffer->set_is_key_frame(false);
  muxer_->PutFrame(Muxer::EncodedFrame{video_params, std::nullopt, buffer},
                   base::Milliseconds(0));
  // Insert a frame just before the duration limit set initially in the test.
  // Expect that Flush isn't invoked.
  task_environment_.AdvanceClock(base::Seconds(1) - base::Milliseconds(1));
  EXPECT_CALL(*delegate_ptr_, Flush).Times(0);
  buffer = media::DecoderBuffer::CopyFrom(base::as_byte_span("v2"));
  buffer->set_is_key_frame(true);
  muxer_->PutFrame(
      Muxer::EncodedFrame{video_params, GetVideoCodecDescription(1), buffer},
      base::Milliseconds(0));
  Mock::VerifyAndClearExpectations(delegate_ptr_);
}

TEST_P(Mp4MuxerTest, FlushesOnSufficientlySpacedFramesForVideo) {
  // This test needs video.
  if (!GetParam().has_video) {
    return;
  }
  CreateMuxer(base::Seconds(2));
  Muxer::VideoParameters video_params(gfx::Size(40, 30), 30, VideoCodec::kH264,
                                      gfx::ColorSpace());
  auto buffer = media::DecoderBuffer::CopyFrom(base::as_byte_span("v1"));
  buffer->set_is_key_frame(true);
  muxer_->PutFrame(
      Muxer::EncodedFrame{video_params, GetVideoCodecDescription(1), buffer},
      base::Milliseconds(0));
  task_environment_.AdvanceClock(base::Seconds(1));
  buffer = media::DecoderBuffer::CopyFrom(base::as_byte_span("v2"));
  buffer->set_is_key_frame(false);
  muxer_->PutFrame(Muxer::EncodedFrame{video_params, std::nullopt, buffer},
                   base::Milliseconds(0));
  // Time will advance to the time for the next flush, so expect Flush called
  // on the next keyframe.
  task_environment_.AdvanceClock(base::Seconds(1));
  EXPECT_CALL(*delegate_ptr_, FlushFragment);
  buffer = media::DecoderBuffer::CopyFrom(base::as_byte_span("v2"));
  buffer->set_is_key_frame(true);
  muxer_->PutFrame(
      Muxer::EncodedFrame{video_params, GetVideoCodecDescription(1), buffer},
      base::Milliseconds(0));
  Mock::VerifyAndClearExpectations(delegate_ptr_);
}

TEST_P(Mp4MuxerTest, FlushesOnSufficientlySpacedFramesForAudioOnly) {
  if (GetParam().has_video) {
    return;
  }
  CreateMuxer(base::Seconds(2));
  AudioParameters audio_params(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                               ChannelLayoutConfig::Stereo(), kAudioSampleRate,
                               1000);
  auto buffer = media::DecoderBuffer::CopyFrom(base::as_byte_span("a1"));
  buffer->set_is_key_frame(true);
  muxer_->PutFrame(
      Muxer::EncodedFrame{audio_params, GetVideoCodecDescription(1), buffer},
      base::Milliseconds(0));
  task_environment_.AdvanceClock(base::Seconds(1));
  buffer = media::DecoderBuffer::CopyFrom(base::as_byte_span("a2"));
  buffer->set_is_key_frame(false);
  muxer_->PutFrame(Muxer::EncodedFrame{audio_params, std::nullopt, buffer},
                   base::Milliseconds(0));
  // Time will advance to the time for the next flush, so expect Flush called
  // on the next keyframe.
  task_environment_.AdvanceClock(base::Seconds(1));
  EXPECT_CALL(*delegate_ptr_, FlushFragment);
  buffer = media::DecoderBuffer::CopyFrom(base::as_byte_span("a2"));
  buffer->set_is_key_frame(true);
  muxer_->PutFrame(
      Muxer::EncodedFrame{audio_params, GetAudioCodecDescription(1), buffer},
      base::Milliseconds(0));
  Mock::VerifyAndClearExpectations(delegate_ptr_);
}

static const TestParam kTestCases[] = {
    {true, false},
    {false, true},
    {true, true},
};

INSTANTIATE_TEST_SUITE_P(, Mp4MuxerTest, testing::ValuesIn(kTestCases));

}  // namespace
}  // namespace media
#endif  // #if BUILDFLAG(USE_PROPRIETARY_CODECS)
