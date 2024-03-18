// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/mf_audio_encoder.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// Most unit tests are in the "AudioDecoderTest" class in another file.
TEST(MfAudioEncoderTest, ClampAccCodecBitrate) {
  EXPECT_EQ(96000u, MFAudioEncoder::ClampAccCodecBitrate(96000u));
  EXPECT_EQ(128000u, MFAudioEncoder::ClampAccCodecBitrate(128000u));
  EXPECT_EQ(160000u, MFAudioEncoder::ClampAccCodecBitrate(160000u));
  EXPECT_EQ(192000u, MFAudioEncoder::ClampAccCodecBitrate(192000u));

  EXPECT_EQ(96000u, MFAudioEncoder::ClampAccCodecBitrate(90000));
  EXPECT_EQ(128000u, MFAudioEncoder::ClampAccCodecBitrate(100000));
  EXPECT_EQ(160000u, MFAudioEncoder::ClampAccCodecBitrate(150000));
  EXPECT_EQ(192000u, MFAudioEncoder::ClampAccCodecBitrate(180000));
  EXPECT_EQ(192000u, MFAudioEncoder::ClampAccCodecBitrate(300000));
}

}  // namespace media
