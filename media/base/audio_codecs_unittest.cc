// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_codecs.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
#if BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)
TEST(ParseDolbyAc4CodecIdTest, DolbyAc4CodecIds) {
  uint8_t bitstream_version;
  uint8_t presentation_version;
  uint8_t presentation_level;

  EXPECT_TRUE(ParseDolbyAc4CodecId("ac-4.02.01.00", &bitstream_version,
                                   &presentation_version, &presentation_level));
  EXPECT_EQ(bitstream_version, 0x02);
  EXPECT_EQ(presentation_version, 0x01);
  EXPECT_EQ(presentation_level, 0);

  // For IMS case:
  EXPECT_TRUE(ParseDolbyAc4CodecId("ac-4.02.02.00", &bitstream_version,
                                   &presentation_version, &presentation_level));
  EXPECT_EQ(bitstream_version, 0x02);
  EXPECT_EQ(presentation_version, 0x02);
  EXPECT_EQ(presentation_level, 0);

  EXPECT_TRUE(ParseDolbyAc4CodecId("ac-4.02.01.04", &bitstream_version,
                                   &presentation_version, &presentation_level));
  EXPECT_EQ(bitstream_version, 0x02);
  EXPECT_EQ(presentation_version, 0x01);
  EXPECT_EQ(presentation_level, 0x04);

  EXPECT_FALSE(ParseDolbyAc4CodecId("ac-4.00.00.00", &bitstream_version,
                                    &presentation_version,
                                    &presentation_level));
  EXPECT_FALSE(ParseDolbyAc4CodecId("ac-4.01.00.00", &bitstream_version,
                                    &presentation_version,
                                    &presentation_level));
  EXPECT_FALSE(ParseDolbyAc4CodecId("ac-4.02.00.00", &bitstream_version,
                                    &presentation_version,
                                    &presentation_level));
  EXPECT_FALSE(ParseDolbyAc4CodecId("ac-4.02.01.08", &bitstream_version,
                                    &presentation_version,
                                    &presentation_level));
  EXPECT_FALSE(ParseDolbyAc4CodecId("ac4.02.01.00", &bitstream_version,
                                    &presentation_version,
                                    &presentation_level));
  EXPECT_FALSE(ParseDolbyAc4CodecId("ac-4.02.01.00.00", &bitstream_version,
                                    &presentation_version,
                                    &presentation_level));
}
#endif  // BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)

}  // namespace media
