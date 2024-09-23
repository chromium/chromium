// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/openscreen/remoting_proto_utils.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/ref_counted.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/encryption_scheme.h"
#include "media/base/test_helpers.h"
#include "media/base/video_decoder_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/openscreen/src/cast/streaming/remoting.pb.h"

using testing::_;
using testing::Invoke;
using testing::Return;

namespace media::cast {

class ProtoUtilsTest : public testing::Test {
 protected:
  void SetUp() override {}
};

TEST_F(ProtoUtilsTest, PassEOSDecoderBuffer) {
  // 1. To DecoderBuffer
  scoped_refptr<media::DecoderBuffer> input_buffer =
      media::DecoderBuffer::CreateEOSBuffer();

  // 2. To Byte Array
  std::vector<uint8_t> data = DecoderBufferToByteArray(*input_buffer);

  // 3. To DecoderBuffer
  scoped_refptr<media::DecoderBuffer> output_buffer =
      ByteArrayToDecoderBuffer(data);
  DCHECK(output_buffer);

  ASSERT_TRUE(output_buffer->end_of_stream());
}

TEST_F(ProtoUtilsTest, PassValidDecoderBuffer) {
  const uint8_t buffer[] = {
      0,   0,   0,   1,   9,   224, 0,   0,   0,   1,   103, 77,  64,  21,  217,
      1,   177, 254, 78,  16,  0,   0,   62,  144, 0,   11,  184, 0,   241, 98,
      228, 128, 0,   0,   0,   1,   104, 235, 143, 32,  0,   0,   0,   1,   103,
      77,  64,  21,  217, 1,   177, 254, 78,  16,  0,   0,   62,  144, 0,   11,
      184, 0,   241, 98,  228, 128, 0,   0,   0,   1,   104, 235, 143, 32,  0,
      0,   0,   1,   101, 136, 132, 25,  255, 0,   191, 98,  0,   6,   29,  63,
      252, 65,  246, 207, 255, 235, 63,  172, 35,  112, 198, 115, 222, 243, 159,
      232, 208, 32,  0,   0,   3,   0,   0,   203, 255, 149, 20,  71,  203, 213,
      40,  0,   0,   139, 0,   24,  117, 166, 249, 227, 68,  230, 177, 134, 161,
      162, 1,   22,  105, 78,  66,  183, 130, 158, 108, 252, 112, 113, 58,  159,
      72,  116, 78,  141, 133, 76,  225, 209, 13,  221, 49,  187, 83,  123, 193,
      112, 123, 112, 74,  121, 133};
  const uint8_t side_buffer[] = {'X', 'X'};
  base::TimeDelta pts = base::Milliseconds(5);

  // 1. To DecoderBuffer
  scoped_refptr<media::DecoderBuffer> input_buffer =
      media::DecoderBuffer::CopyFrom(buffer);
  input_buffer->set_timestamp(pts);
  input_buffer->set_is_key_frame(true);
  input_buffer->WritableSideData().alpha_data.assign(std::begin(side_buffer),
                                                     std::end(side_buffer));

  // 2. To Byte Array
  std::vector<uint8_t> data = DecoderBufferToByteArray(*input_buffer);

  // 3. To DecoderBuffer
  scoped_refptr<media::DecoderBuffer> output_buffer =
      ByteArrayToDecoderBuffer(data);
  DCHECK(output_buffer);

  ASSERT_FALSE(output_buffer->end_of_stream());
  ASSERT_TRUE(output_buffer->is_key_frame());
  ASSERT_EQ(output_buffer->timestamp(), pts);
  EXPECT_EQ(base::span(*output_buffer), base::span(buffer));
  ASSERT_TRUE(output_buffer->has_side_data());
  EXPECT_EQ(base::span(output_buffer->side_data()->alpha_data),
            base::span(side_buffer));
}

TEST_F(ProtoUtilsTest, AudioDecoderConfigConversionTest) {
  const char extra_data[4] = {'A', 'C', 'E', 'G'};
  media::AudioDecoderConfig audio_config(
      media::AudioCodec::kOpus, media::kSampleFormatF32,
      media::CHANNEL_LAYOUT_MONO, 48000,
      std::vector<uint8_t>(std::begin(extra_data), std::end(extra_data)),
      media::EncryptionScheme::kUnencrypted);
  ASSERT_TRUE(audio_config.IsValidConfig());

  openscreen::cast::AudioDecoderConfig audio_message;
  ConvertAudioDecoderConfigToProto(audio_config, &audio_message);

  media::AudioDecoderConfig audio_output_config;
  ASSERT_TRUE(
      ConvertProtoToAudioDecoderConfig(audio_message, &audio_output_config));

  ASSERT_TRUE(audio_config.Matches(audio_output_config));
}

TEST_F(ProtoUtilsTest, AudioDecoderConfigHandlesAacExtraDataCorrectly) {
  constexpr char aac_extra_data[4] = {'A', 'C', 'E', 'G'};
  media::AudioDecoderConfig audio_config(
      media::AudioCodec::kAAC, media::kSampleFormatF32,
      media::CHANNEL_LAYOUT_MONO, 48000, std::vector<uint8_t>{},
      media::EncryptionScheme::kUnencrypted);
  audio_config.set_aac_extra_data(std::vector<uint8_t>(
      std::begin(aac_extra_data), std::end(aac_extra_data)));
  ASSERT_TRUE(audio_config.IsValidConfig());

  openscreen::cast::AudioDecoderConfig audio_message;
  ConvertAudioDecoderConfigToProto(audio_config, &audio_message);

  // We should have filled the "extra_data" protobuf field with
  // "aac_extra_data."
  const std::vector<uint8_t> proto_extra_data(
      audio_message.extra_data().begin(), audio_message.extra_data().end());
  EXPECT_THAT(proto_extra_data, testing::ElementsAreArray(aac_extra_data));

  media::AudioDecoderConfig audio_output_config;
  ASSERT_TRUE(
      ConvertProtoToAudioDecoderConfig(audio_message, &audio_output_config));
  ASSERT_TRUE(audio_config.Matches(audio_output_config))
      << "expected=" << audio_config.AsHumanReadableString()
      << ", actual=" << audio_output_config.AsHumanReadableString();
}

TEST_F(ProtoUtilsTest, PipelineStatisticsConversion) {
  media::PipelineStatistics original;
  // NOTE: all fields should be initialised here.
  original.audio_bytes_decoded = 123;
  original.video_bytes_decoded = 456;
  original.video_frames_decoded = 789;
  original.video_frames_decoded_power_efficient = 0;
  original.video_frames_dropped = 21;
  original.audio_memory_usage = 32;
  original.video_memory_usage = 43;
  original.video_keyframe_distance_average = base::TimeDelta::Max();
  original.video_frame_duration_average = base::TimeDelta::Max();
  original.audio_pipeline_info = {false, false,
                                  media::AudioDecoderType::kUnknown,
                                  media::EncryptionType::kClear};
  original.video_pipeline_info = {false, false,
                                  media::VideoDecoderType::kUnknown,
                                  media::EncryptionType::kClear};

  // There is no convert-to-proto function, so just do that here.
  openscreen::cast::PipelineStatistics pb_stats;
  openscreen::cast::VideoDecoderInfo* pb_video_info =
      pb_stats.mutable_video_decoder_info();
  openscreen::cast::AudioDecoderInfo* pb_audio_info =
      pb_stats.mutable_audio_decoder_info();
  pb_stats.set_audio_bytes_decoded(original.audio_bytes_decoded);
  pb_stats.set_video_bytes_decoded(original.video_bytes_decoded);
  pb_stats.set_video_frames_decoded(original.video_frames_decoded);
  pb_stats.set_video_frames_dropped(original.video_frames_dropped);
  pb_stats.set_audio_memory_usage(original.audio_memory_usage);
  pb_stats.set_video_memory_usage(original.video_memory_usage);
  pb_stats.set_video_frame_duration_average_usec(
      original.video_frame_duration_average.InMicroseconds());

  pb_video_info->set_decoder_type(
      static_cast<int64_t>(original.video_pipeline_info.decoder_type));
  pb_video_info->set_is_platform_decoder(
      original.video_pipeline_info.is_platform_decoder);

  pb_audio_info->set_decoder_type(
      static_cast<int64_t>(original.audio_pipeline_info.decoder_type));
  pb_audio_info->set_is_platform_decoder(
      original.audio_pipeline_info.is_platform_decoder);

  media::PipelineStatistics converted;

  // NOTE: fields will all be initialized with 0xcd. Forcing the conversion to
  // properly assigned them. Since nested structs have strings, memsetting must
  // be done infividually for them.
  memset(&converted, 0xcd,
         sizeof(converted) - sizeof(media::AudioPipelineInfo) -
             sizeof(media::VideoPipelineInfo));
  memset(&converted.audio_pipeline_info, 0xcd,
         sizeof(media::AudioPipelineInfo));
  memset(&converted.video_pipeline_info, 0xcd,
         sizeof(media::VideoPipelineInfo));

  ConvertProtoToPipelineStatistics(pb_stats, &converted);

  // If this fails, did media::PipelineStatistics add/change fields that are not
  // being set by media::remoting::ConvertProtoToPipelineStatistics()?
  EXPECT_EQ(original, converted);
}

TEST_F(ProtoUtilsTest, VideoDecoderConfigConversionTest) {
  const media::VideoDecoderConfig video_config =
      media::TestVideoConfig::Normal();
  ASSERT_TRUE(video_config.IsValidConfig());
  openscreen::cast::VideoDecoderConfig message;
  ConvertVideoDecoderConfigToProto(video_config, &message);
  media::VideoDecoderConfig converted;
  ASSERT_TRUE(ConvertProtoToVideoDecoderConfig(message, &converted));
  ASSERT_TRUE(converted.Matches(video_config));
}

}  // namespace media::cast
