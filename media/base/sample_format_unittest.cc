// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/sample_format.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(SampleFormatTest, MutuallyExclusiveFormatProperties) {
  for (int i = 1; i <= kMaxValue; ++i) {
    auto format = static_cast<SampleFormat>(i);
    const bool is_planar = IsPlanar(format);
    const bool is_interleaved = IsInterleaved(format);
    const bool is_bitstream = IsBitstream(format);

    int true_count = 0;
    if (is_planar) {
      true_count++;
    }
    if (is_interleaved) {
      true_count++;
    }
    if (is_bitstream) {
      true_count++;
    }

    EXPECT_EQ(true_count, 1)
        << "Sample format " << SampleFormatToString(format) << " (" << i
        << ") must return true for exactly one of IsPlanar (" << is_planar
        << "), IsInterleaved (" << is_interleaved << "), or IsBitstream ("
        << is_bitstream << ").";
  }

  EXPECT_FALSE(IsPlanar(kUnknownSampleFormat));
  EXPECT_FALSE(IsInterleaved(kUnknownSampleFormat));
  EXPECT_FALSE(IsBitstream(kUnknownSampleFormat));
}

TEST(SampleFormatTest, FormatClassification) {
  EXPECT_TRUE(IsInterleaved(kSampleFormatU8));
  EXPECT_TRUE(IsInterleaved(kSampleFormatS16));
  EXPECT_TRUE(IsInterleaved(kSampleFormatS24));
  EXPECT_TRUE(IsInterleaved(kSampleFormatS32));
  EXPECT_TRUE(IsInterleaved(kSampleFormatF32));

  EXPECT_TRUE(IsPlanar(kSampleFormatPlanarU8));
  EXPECT_TRUE(IsPlanar(kSampleFormatPlanarS16));
  EXPECT_TRUE(IsPlanar(kSampleFormatPlanarF32));
  EXPECT_TRUE(IsPlanar(kSampleFormatPlanarS32));

  EXPECT_TRUE(IsBitstream(kSampleFormatAc3));
  EXPECT_TRUE(IsBitstream(kSampleFormatEac3));
  EXPECT_TRUE(IsBitstream(kSampleFormatMpegHAudio));
  EXPECT_TRUE(IsBitstream(kSampleFormatDts));
  EXPECT_TRUE(IsBitstream(kSampleFormatDtsxP2));
  EXPECT_TRUE(IsBitstream(kSampleFormatIECDts));
  EXPECT_TRUE(IsBitstream(kSampleFormatDtse));
}

}  // namespace media
