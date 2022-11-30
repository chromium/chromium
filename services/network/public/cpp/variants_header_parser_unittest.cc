// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/variants_header_parser.h"
#include <iostream>

#include "net/http/http_response_headers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::UnorderedElementsAre;

namespace network {

TEST(VariantsHeaderTest, ParseVariantsHeader) {
  absl::optional<std::vector<mojom::VariantsHeaderPtr>> result;

  // Empty is OK.
  result = ParseVariantsHeaders(" ");
  EXPECT_TRUE(result.has_value());
  ASSERT_EQ(result.value().size(), 0UL);

  // Normal case.
  result = ParseVariantsHeaders("accept-language=(en)");
  EXPECT_TRUE(result.has_value());
  ASSERT_EQ(result.value().size(), 1UL);
  EXPECT_EQ(result.value()[0]->name, "accept-language");
  EXPECT_EQ(result.value()[0]->available_values,
            (std::vector<std::string>){"en"});

  // Case with no values.
  result = ParseVariantsHeaders("Accept-Language;de;en;jp");
  EXPECT_TRUE(result.has_value());
  ASSERT_EQ(result.value().size(), 1UL);
  EXPECT_EQ(result.value()[0]->name, "accept-language");
  EXPECT_EQ(result.value()[0]->available_values, (std::vector<std::string>){});

  // Value list is empty
  result = ParseVariantsHeaders("accept-language=()");
  EXPECT_TRUE(result.has_value());
  ASSERT_EQ(result.value().size(), 1UL);
  EXPECT_EQ(result.value()[0]->name, "accept-language");
  EXPECT_EQ(result.value()[0]->available_values, (std::vector<std::string>){});

  // Must be a list of tokens, not other things.
  result = ParseVariantsHeaders(
      "\"accept-language=(en)\", \"accept-encoding=(gzip)\"");
  EXPECT_FALSE(result.has_value());

  // Unknown tokens are fine, since this meant to be extensible.
  result =
      ParseVariantsHeaders("accept-language=(en),  accept-encoding=(gzip)");
  EXPECT_TRUE(result.has_value());
  ASSERT_EQ(result.value().size(), 1UL);
  EXPECT_EQ(result.value()[0]->name, "accept-language");
  EXPECT_EQ(result.value()[0]->available_values,
            (std::vector<std::string>){"en"});

  // Matching is case-insensitive.
  result = ParseVariantsHeaders("Accept-Language=(en)");
  EXPECT_TRUE(result.has_value());
  ASSERT_EQ(result.value().size(), 1UL);
  EXPECT_EQ(result.value()[0]->name, "accept-language");
  EXPECT_EQ(result.value()[0]->available_values,
            (std::vector<std::string>){"en"});

  // Matching can find a one or more tokens.
  result = ParseVariantsHeaders("Accept-Language=(en de zh)");
  EXPECT_TRUE(result.has_value());
  ASSERT_EQ(result.value().size(), 1UL);
  EXPECT_EQ(result.value()[0]->name, "accept-language");
  EXPECT_EQ(result.value()[0]->available_values,
            std::vector<std::string>({"en", "de", "zh"}));

  // Only matching first comes value pair.
  result = ParseVariantsHeaders("accept-language=(en), accept-language=(de)");
  EXPECT_TRUE(result.has_value());
  ASSERT_EQ(result.value().size(), 1UL);
  EXPECT_EQ(result.value()[0]->name, "accept-language");
  EXPECT_EQ(result.value()[0]->available_values,
            std::vector<std::string>({"de"}));
}

}  // namespace network
