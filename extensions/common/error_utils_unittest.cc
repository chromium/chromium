// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/error_utils.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

// Tests ErrorUtils::FormatErrorMessage which takes two placeholders.
TEST(ErrorUtils, FormatErrorMessage_Success) {
  struct {
    base::StringPiece format;
    base::StringPiece s1;
    base::StringPiece s2;
    const char* expected;
  } cases[] = {
      {"Hello * Bye *", "arg1", "arg2", "Hello arg1 Bye arg2"},
      // Ensure substitutions respect the size of the StringPiece.
      {"Hello * Bye *", base::StringPiece("12345", 2), "3", "Hello 12 Bye 3"},
      // Regression test for crbug.com/928415.
      {"Hello * Bye *", "*arg1", "*arg2", "Hello *arg1 Bye *arg2"},
  };

  for (const auto& test_case : cases) {
    SCOPED_TRACE(test_case.format);

    EXPECT_EQ(test_case.expected,
              ErrorUtils::FormatErrorMessage(test_case.format, test_case.s1,
                                             test_case.s2));
    EXPECT_EQ(base::UTF8ToUTF16(test_case.expected),
              ErrorUtils::FormatErrorMessageUTF16(test_case.format,
                                                  test_case.s1, test_case.s2));
  }
}

#if defined(GTEST_HAS_DEATH_TEST)
// Tests that we raise an error if the number of placeholders and substitution
// arguments are not equal.
TEST(ErrorUtils, FormatErrorMessage_Death) {
  struct {
    base::StringPiece format;
    base::StringPiece s1;
    base::StringPiece s2;
    const char* death_message_regex;
  } cases[] = {{"Hello * Bye * *", "arg1", "arg2", "More placeholders"},
               {"Hello * Bye", "arg1", "arg2", "Fewer placeholders"}};

  auto get_death_regex = [](const char* death_message_regex) {
// String arguments aren't passed to CHECK() in official builds.
#if defined(OFFICIAL_BUILD) && defined(NDEBUG)
    return "";
#else
    return death_message_regex;
#endif
  };

  for (const auto& test_case : cases) {
    SCOPED_TRACE(test_case.format);

    EXPECT_DEATH(ErrorUtils::FormatErrorMessage(test_case.format, test_case.s1,
                                                test_case.s2),
                 get_death_regex(test_case.death_message_regex));
    EXPECT_DEATH(ErrorUtils::FormatErrorMessageUTF16(
                     test_case.format, test_case.s1, test_case.s2),
                 get_death_regex(test_case.death_message_regex));
  }
}
#endif  // defined(GTEST_HAS_DEATH_TEST)

}  // namespace
}  // namespace extensions
