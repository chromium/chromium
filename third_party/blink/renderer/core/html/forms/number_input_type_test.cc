// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/number_input_type.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

TEST(NumberInputTypeTest, NormalizeFullWidthNumberChars) {
  const auto& normalize = NumberInputType::NormalizeFullWidthNumberChars;

  // Returns empty string unchanged
  EXPECT_EQ(normalize(""), "");

  // Converts long sound mark (ー, U+30FC) alone to ASCII minus
  EXPECT_EQ(normalize(u"ー"), "-");

  // Converts full-width minus sign (－, U+FF0D) to ASCII minus
  EXPECT_EQ(normalize(u"－"), "-");

  // Converts full-width period (．, U+FF0E) to ASCII dot
  EXPECT_EQ(normalize(u"．"), ".");

  // Converts full-width minus, digits, and period in sequence
  EXPECT_EQ(normalize(u"ー－０１２３４．５６７８９"), "--01234.56789");

  // Keeps non-numeric ASCII characters unchanged
  EXPECT_EQ(normalize("abc"), "abc");
  EXPECT_EQ(normalize("33-4"), "33-4");

  // Keeps unrelated full-width punctuation (／, U+FF0F) unchanged
  EXPECT_EQ(normalize(u"／"), u"／");

  // Keeps unrelated full-width punctuation (：, U+FF1A) unchanged
  EXPECT_EQ(normalize(u"："), u"：");
}

}  // namespace blink
