// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/muxers/mp4_type_conversion.h"

#include "base/big_endian.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

struct LanguageAndCode {
  std::string language;
  uint16_t expected_code;
};

TEST(Mp4TypeConversionTest, LanguageCode) {
  LanguageAndCode test_data[] = {
      {"ara", 0x0641},
      {"zho", 0x690f},
      {"eng", 0x15c7},
      {"fra", 0x1a41},
      {"kor", 0x2df2},
      {"und", kUndefinedLanguageCode},
      {"", kUndefinedLanguageCode},
      {"ENG", kUndefinedLanguageCode},
      {"english", kUndefinedLanguageCode},
  };

  for (const auto& data : test_data) {
    uint16_t language_code = ConvertIso639LanguageCodeToU16(data.language);
    EXPECT_EQ(data.expected_code, language_code);
  }
}

TEST(Mp4TypeConversionTest, TimescaleConversion) {
  base::TimeDelta time_diff = base::Milliseconds(321);
  EXPECT_EQ(ConvertToTimescale(time_diff, 30000), 9630u);
  EXPECT_EQ(ConvertToTimescale(time_diff, 44100), 14156u);

  base::TimeDelta time_diff2 = base::Microseconds(321000);
  EXPECT_EQ(ConvertToTimescale(time_diff2, 30000), 9630u);
  EXPECT_EQ(ConvertToTimescale(time_diff2, 44100), 14156u);
}

}  // namespace media
