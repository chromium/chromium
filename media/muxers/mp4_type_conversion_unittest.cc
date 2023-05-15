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

TEST(Mp4TypeConversionTest, Default) {
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

}  // namespace media
