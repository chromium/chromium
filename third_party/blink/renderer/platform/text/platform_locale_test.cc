// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/text/platform_locale.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(PlatformLocaleTest, StripInvalidNumberCharacters) {
  // Arabic does not use Arabic-Indic digit any more per
  // Google's customized CLDR 33.1, but ar-EG does. Persian
  // uses Extened Arabic-Indic digits.
  std::unique_ptr<Locale> locale = Locale::Create("fa");
  String result = locale->StripInvalidNumberCharacters(
      String::FromUTF8("abc\u06F0ghi"), "0123456789");
  // EXTENDED ARABIC-INDIC DIGIT ZERO U+06F0
  EXPECT_EQ(String::FromUTF8("\u06F0"), result);
}
}
