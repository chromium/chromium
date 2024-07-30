// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/avail_language_header_parser.h"

#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::UnorderedElementsAre;

namespace network {

TEST(AvailLanguageTest, ParseAvailLanguage) {
  std::optional<std::vector<std::string>> result;

  // Empty is OK.
  result = ParseAvailLanguage(" ");
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result.value().empty());

  // Normal case.
  result = ParseAvailLanguage("en, zh ");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), std::vector<std::string>({"en", "zh"}));

  // Must be a list of tokens, not other things.
  result = ParseAvailLanguage("\"en\", \"zh\"");
  EXPECT_FALSE(result.has_value());

  // Parameters to the tokens are ignored.
  result = ParseAvailLanguage("en;q=1.0, zh");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), std::vector<std::string>({"en", "zh"}));

  // Parameters with nested lists not allowed.
  result = ParseAvailLanguage("(en jp), (zh es)");
  EXPECT_FALSE(result.has_value());

  // Parameters to with default.
  result = ParseAvailLanguage("en, zh;d");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), std::vector<std::string>({"zh", "en"}));

  // Parameters to with two defaults.
  result = ParseAvailLanguage("en, zh;d, ja;d");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), std::vector<std::string>({"zh", "ja", "en"}));

  // Parameters to with other pattern are ignored.
  result = ParseAvailLanguage("en, zh;d=1");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), std::vector<std::string>({"en", "zh"}));

  // Parameters to with other boolean value are ignored.
  result = ParseAvailLanguage("en, zh;d=?0");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), std::vector<std::string>({"en", "zh"}));

  // Matching is case-insensitive.
  result = ParseAvailLanguage("de-DE, en-CA ");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), std::vector<std::string>({"de-de", "en-ca"}));

  result = ParseAvailLanguage("en, fr (This is a dictionary)");
  ASSERT_FALSE(result.has_value());
}

}  // namespace network
