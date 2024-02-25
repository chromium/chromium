// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/content_language_parser.h"
#include <iostream>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::UnorderedElementsAre;

namespace network {

TEST(ContentLanguageTest, ParseContentLanguages) {
  std::optional<std::vector<std::string>> result;

  // Empty is OK.
  result = ParseContentLanguages(" ");
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result.value().empty());

  // Normal case.
  result = ParseContentLanguages("en, zh ");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), std::vector<std::string>({"en", "zh"}));

  // Must be a list of tokens, not other things.
  result = ParseContentLanguages("\"en\", \"zh\"");
  EXPECT_FALSE(result.has_value());

  // Parameters to the tokens are ignored.
  result = ParseContentLanguages("en;q=1.0, zh");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), std::vector<std::string>({"en", "zh"}));

  // Matching is case-insensitive.
  result = ParseContentLanguages("de-DE, en-CA ");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), std::vector<std::string>({"de-de", "en-ca"}));

  result = ParseContentLanguages("en, fr (This is a dictionary)");
  ASSERT_FALSE(result.has_value());
}

}  // namespace network
