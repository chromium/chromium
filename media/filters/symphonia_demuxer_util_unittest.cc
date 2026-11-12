// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/symphonia_demuxer_util.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace media {

using container_names::MediaContainerName;

TEST(SymphoniaDemuxerUtilTest, AudioCodecs) {
  EXPECT_EQ(SymphoniaCodecToAudioCodec(SymphoniaCodec::Aac), AudioCodec::kAAC);
  EXPECT_EQ(SymphoniaCodecToAudioCodec(SymphoniaCodec::Flac),
            AudioCodec::kFLAC);
  EXPECT_EQ(SymphoniaCodecToAudioCodec(SymphoniaCodec::Mp3), AudioCodec::kMP3);
  EXPECT_EQ(SymphoniaCodecToAudioCodec(SymphoniaCodec::Opus),
            AudioCodec::kOpus);
  EXPECT_EQ(SymphoniaCodecToAudioCodec(SymphoniaCodec::Vorbis),
            AudioCodec::kVorbis);
  EXPECT_EQ(SymphoniaCodecToAudioCodec(SymphoniaCodec::PcmAlaw),
            AudioCodec::kPCM_ALAW);
  EXPECT_EQ(SymphoniaCodecToAudioCodec(SymphoniaCodec::PcmMulaw),
            AudioCodec::kPCM_MULAW);
  EXPECT_EQ(SymphoniaCodecToAudioCodec(SymphoniaCodec::PcmS16be),
            AudioCodec::kPCM_S16BE);
  EXPECT_EQ(SymphoniaCodecToAudioCodec(SymphoniaCodec::PcmS24be),
            AudioCodec::kPCM_S24BE);
  EXPECT_EQ(SymphoniaCodecToAudioCodec(SymphoniaCodec::PcmS16),
            AudioCodec::kPCM);
  EXPECT_EQ(SymphoniaCodecToAudioCodec(SymphoniaCodec::PcmF32Planar),
            AudioCodec::kPCM);
}

TEST(SymphoniaDemuxerUtilTest, UnsupportedCodecsAreUnknown) {
  EXPECT_EQ(SymphoniaCodecToAudioCodec(SymphoniaCodec::Unknown),
            AudioCodec::kUnknown);
  EXPECT_EQ(SymphoniaCodecToAudioCodec(SymphoniaCodec::Vp9),
            AudioCodec::kUnknown);
  EXPECT_EQ(SymphoniaCodecToSampleFormat(SymphoniaCodec::Unknown),
            kUnknownSampleFormat);
  EXPECT_EQ(SymphoniaCodecToSampleFormat(SymphoniaCodec::H264),
            kUnknownSampleFormat);
}

TEST(SymphoniaDemuxerUtilTest, SampleFormats) {
  EXPECT_EQ(SymphoniaCodecToSampleFormat(SymphoniaCodec::PcmU8),
            kSampleFormatU8);
  EXPECT_EQ(SymphoniaCodecToSampleFormat(SymphoniaCodec::PcmU8Planar),
            kSampleFormatPlanarU8);
  EXPECT_EQ(SymphoniaCodecToSampleFormat(SymphoniaCodec::PcmS16be),
            kSampleFormatS16);
  EXPECT_EQ(SymphoniaCodecToSampleFormat(SymphoniaCodec::PcmS16Planar),
            kSampleFormatPlanarS16);
  EXPECT_EQ(SymphoniaCodecToSampleFormat(SymphoniaCodec::PcmS24be),
            kSampleFormatS24);
  EXPECT_EQ(SymphoniaCodecToSampleFormat(SymphoniaCodec::PcmS32Planar),
            kSampleFormatPlanarS32);
  EXPECT_EQ(SymphoniaCodecToSampleFormat(SymphoniaCodec::PcmF32),
            kSampleFormatF32);
  EXPECT_EQ(SymphoniaCodecToSampleFormat(SymphoniaCodec::Flac),
            kSampleFormatS16);
  // G.711 decodes to 16-bit linear PCM, even though it is 8 bits on the wire.
  EXPECT_EQ(SymphoniaCodecToSampleFormat(SymphoniaCodec::PcmAlaw),
            kSampleFormatS16);
  EXPECT_EQ(SymphoniaCodecToSampleFormat(SymphoniaCodec::PcmMulaw),
            kSampleFormatS16);
}

// The names are the `FormatInfo::short_name` values of the vendored Symphonia
// format readers.
TEST(SymphoniaDemuxerUtilTest, ContainerNames) {
  EXPECT_EQ(SymphoniaFormatNameToContainerName("wave"),
            MediaContainerName::kContainerWAV);
  EXPECT_EQ(SymphoniaFormatNameToContainerName("ogg"),
            MediaContainerName::kContainerOgg);
  EXPECT_EQ(SymphoniaFormatNameToContainerName("flac"),
            MediaContainerName::kContainerFLAC);
  EXPECT_EQ(SymphoniaFormatNameToContainerName("mp1"),
            MediaContainerName::kContainerMP3);
  EXPECT_EQ(SymphoniaFormatNameToContainerName("mp2"),
            MediaContainerName::kContainerMP3);
  EXPECT_EQ(SymphoniaFormatNameToContainerName("mp3"),
            MediaContainerName::kContainerMP3);
  EXPECT_EQ(SymphoniaFormatNameToContainerName("aiff"),
            MediaContainerName::kContainerAIFF);
  EXPECT_EQ(SymphoniaFormatNameToContainerName("isomp4"),
            MediaContainerName::kContainerMOV);
  EXPECT_EQ(SymphoniaFormatNameToContainerName("matroska"),
            MediaContainerName::kContainerWEBM);
}

TEST(SymphoniaDemuxerUtilTest, UnknownContainerNames) {
  EXPECT_EQ(SymphoniaFormatNameToContainerName(""),
            MediaContainerName::kContainerUnknown);
  // Not reader names: the WAV reader reports "wave" and the Matroska reader
  // reports "matroska".
  EXPECT_EQ(SymphoniaFormatNameToContainerName("wav"),
            MediaContainerName::kContainerUnknown);
  EXPECT_EQ(SymphoniaFormatNameToContainerName("webm"),
            MediaContainerName::kContainerUnknown);
}

}  // namespace media
