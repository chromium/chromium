// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "media/base/audio_decoder_config.h"
#include "media/base/demuxer.h"
#include "media/base/demuxer_memory_limit.h"
#include "media/base/media_util.h"
#include "media/base/video_decoder_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

static const gfx::Size kCodedSize(320, 240);
static const gfx::Rect kVisibleRect(320, 240);
static const gfx::Size kNaturalSize(320, 240);

TEST(DemuxerMemoryLimitCastTest, GetDemuxerStreamAudioMemoryLimit) {
  EXPECT_EQ(GetDemuxerStreamAudioMemoryLimit(nullptr),
            internal::kDemuxerStreamAudioMemoryLimitLow);

  AudioDecoderConfig audio_config_opus(
      AudioCodec::kOpus, SampleFormat::kSampleFormatS16,
      ChannelLayout::CHANNEL_LAYOUT_STEREO, 5000 /* samples_per_second */,
      EmptyExtraData(), EncryptionScheme::kUnencrypted);
  EXPECT_EQ(GetDemuxerStreamAudioMemoryLimit(&audio_config_opus),
            internal::kDemuxerStreamAudioMemoryLimitLow);

  AudioDecoderConfig audio_config_ac3(
      AudioCodec::kAC3, SampleFormat::kSampleFormatS16,
      ChannelLayout::CHANNEL_LAYOUT_STEREO, 5000 /* samples_per_second */,
      EmptyExtraData(), EncryptionScheme::kUnencrypted);
  EXPECT_EQ(GetDemuxerStreamAudioMemoryLimit(&audio_config_ac3),
            internal::kDemuxerStreamAudioMemoryLimitMedium);

  AudioDecoderConfig audio_config_aac_1(
      AudioCodec::kAAC, SampleFormat::kSampleFormatS16,
      ChannelLayout::CHANNEL_LAYOUT_5_0, 5000 /* samples_per_second */,
      EmptyExtraData(), EncryptionScheme::kUnencrypted);
  EXPECT_EQ(GetDemuxerStreamAudioMemoryLimit(&audio_config_aac_1),
            internal::kDemuxerStreamAudioMemoryLimitMedium);

  AudioDecoderConfig audio_config_aac_2(
      AudioCodec::kAAC, SampleFormat::kSampleFormatS16,
      ChannelLayout::CHANNEL_LAYOUT_STEREO, 5000 /* samples_per_second */,
      EmptyExtraData(), EncryptionScheme::kUnencrypted);
  EXPECT_EQ(GetDemuxerStreamAudioMemoryLimit(&audio_config_aac_2),
            internal::kDemuxerStreamAudioMemoryLimitLow);

  AudioDecoderConfig audio_config_dts_20(
      AudioCodec::kDTS, SampleFormat::kSampleFormatS16,
      ChannelLayout::CHANNEL_LAYOUT_STEREO, 5000 /* samples_per_second */,
      EmptyExtraData(), EncryptionScheme::kUnencrypted);
  EXPECT_EQ(GetDemuxerStreamAudioMemoryLimit(&audio_config_dts_20),
            internal::kDemuxerStreamAudioMemoryLimitMedium);

  AudioDecoderConfig audio_config_dts_51(
      AudioCodec::kDTS, SampleFormat::kSampleFormatS16,
      ChannelLayout::CHANNEL_LAYOUT_5_1, 5000 /* samples_per_second */,
      EmptyExtraData(), EncryptionScheme::kUnencrypted);
  EXPECT_EQ(GetDemuxerStreamAudioMemoryLimit(&audio_config_dts_51),
            internal::kDemuxerStreamAudioMemoryLimitMedium);

  AudioDecoderConfig audio_config_dtsxp2_20(
      AudioCodec::kDTSXP2, SampleFormat::kSampleFormatS16,
      ChannelLayout::CHANNEL_LAYOUT_STEREO, 5000 /* samples_per_second */,
      EmptyExtraData(), EncryptionScheme::kUnencrypted);
  EXPECT_EQ(GetDemuxerStreamAudioMemoryLimit(&audio_config_dtsxp2_20),
            internal::kDemuxerStreamAudioMemoryLimitMedium);

  AudioDecoderConfig audio_config_dtsxp2_51(
      AudioCodec::kDTSXP2, SampleFormat::kSampleFormatS16,
      ChannelLayout::CHANNEL_LAYOUT_5_1, 5000 /* samples_per_second */,
      EmptyExtraData(), EncryptionScheme::kUnencrypted);
  EXPECT_EQ(GetDemuxerStreamAudioMemoryLimit(&audio_config_dtsxp2_51),
            internal::kDemuxerStreamAudioMemoryLimitMedium);
}

TEST(DemuxerMemoryLimitCastTest, GetDemuxerStreamVideoMemoryLimit) {
  EXPECT_EQ(GetDemuxerStreamVideoMemoryLimit(
                Demuxer::DemuxerTypes::kFFmpegDemuxer, nullptr),
            internal::kDemuxerStreamVideoMemoryLimitDefault);
  EXPECT_EQ(GetDemuxerStreamVideoMemoryLimit(
                Demuxer::DemuxerTypes::kChunkDemuxer, nullptr),
            internal::kDemuxerStreamVideoMemoryLimitLow);
  EXPECT_EQ(GetDemuxerStreamVideoMemoryLimit(
                Demuxer::DemuxerTypes::kMediaUrlDemuxer, nullptr),
            internal::kDemuxerStreamVideoMemoryLimitLow);

  VideoDecoderConfig video_config(
      VideoCodec::kVP8, VIDEO_CODEC_PROFILE_UNKNOWN,
      VideoDecoderConfig::AlphaMode::kIsOpaque, VideoColorSpace(),
      kNoTransformation, kCodedSize, kVisibleRect, kNaturalSize,
      EmptyExtraData(), EncryptionScheme::kUnencrypted);
  EXPECT_EQ(GetDemuxerStreamVideoMemoryLimit(
                Demuxer::DemuxerTypes::kFFmpegDemuxer, &video_config),
            internal::kDemuxerStreamVideoMemoryLimitDefault);
  EXPECT_EQ(GetDemuxerStreamVideoMemoryLimit(
                Demuxer::DemuxerTypes::kChunkDemuxer, &video_config),
            internal::kDemuxerStreamVideoMemoryLimitLow);
  EXPECT_EQ(GetDemuxerStreamVideoMemoryLimit(
                Demuxer::DemuxerTypes::kMediaUrlDemuxer, &video_config),
            internal::kDemuxerStreamVideoMemoryLimitLow);

  video_config.Initialize(VideoCodec::kVP9, VIDEO_CODEC_PROFILE_UNKNOWN,
                          VideoDecoderConfig::AlphaMode::kIsOpaque,
                          VideoColorSpace(), kNoTransformation, kCodedSize,
                          kVisibleRect, kNaturalSize, EmptyExtraData(),
                          EncryptionScheme::kUnencrypted);
  EXPECT_EQ(GetDemuxerStreamVideoMemoryLimit(
                Demuxer::DemuxerTypes::kFFmpegDemuxer, &video_config),
            internal::kDemuxerStreamVideoMemoryLimitDefault);
  EXPECT_EQ(GetDemuxerStreamVideoMemoryLimit(
                Demuxer::DemuxerTypes::kChunkDemuxer, &video_config),
            internal::kDemuxerStreamVideoMemoryLimitMedium);
  EXPECT_EQ(GetDemuxerStreamVideoMemoryLimit(
                Demuxer::DemuxerTypes::kMediaUrlDemuxer, &video_config),
            internal::kDemuxerStreamVideoMemoryLimitLow);
}

TEST(DemuxerMemoryLimitCastTest, GetDemuxerMemoryLimit) {
  EXPECT_EQ(GetDemuxerMemoryLimit(Demuxer::DemuxerTypes::kFFmpegDemuxer),
            internal::kDemuxerStreamAudioMemoryLimitLow +
                internal::kDemuxerStreamVideoMemoryLimitDefault);
  EXPECT_EQ(GetDemuxerMemoryLimit(Demuxer::DemuxerTypes::kChunkDemuxer),
            internal::kDemuxerStreamAudioMemoryLimitLow +
                internal::kDemuxerStreamVideoMemoryLimitLow);
  EXPECT_EQ(GetDemuxerMemoryLimit(Demuxer::DemuxerTypes::kMediaUrlDemuxer),
            internal::kDemuxerStreamAudioMemoryLimitLow +
                internal::kDemuxerStreamVideoMemoryLimitLow);
}

}  // namespace media
