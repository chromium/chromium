// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cookie_insight_list/cookie_readiness_list/cookie_readiness_list_parser.h"

#include <string>

#include "content/browser/cookie_insight_list/cookie_insight_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

using testing::IsEmpty;
using testing::Optional;
using testing::Pair;
using testing::UnorderedElementsAre;

TEST(CookieReadinessListParserTest, ParseReadinessList_InvalidFile_NoEntries) {
  const std::string json_content = "Invalid";

  EXPECT_EQ(CookieReadinessListParser::ParseReadinessList(json_content),
            CookieInsightList());
}

TEST(CookieReadinessListParserTest,
     ParseReadinessList_InvalidFile_InvalidDomain) {
  const std::string json_content = R"({"entries": [
    {
      "domains": [
          "example",
      ],
      "tableEntryUrl": "url"
    }]
  })";

  EXPECT_EQ(CookieReadinessListParser::ParseReadinessList(json_content),
            CookieInsightList());
}

TEST(CookieReadinessListParserTest, ParseReadinessList_ValidFile) {
  const std::string json_content = R"({"entries": [
    {
      "domains": [
          "example.com",
      ],
      "tableEntryUrl": "url"
    }]
  })";

  EXPECT_EQ(CookieReadinessListParser::ParseReadinessList(json_content),
            CookieInsightList({{"example.com", {"url"}}}));
}

TEST(CookieReadinessListParserTest, ParseReadinessList_DuplicateDomain) {
  const std::string json_content = R"({"entries": [
    {
      "domains": [
        "example.com",
      ],
      "tableEntryUrl": "url"
    },
    {
      "domains": [
        "example.com",
      ],
      "tableEntryUrl": "newUrl"
    }]
  })";

  EXPECT_EQ(CookieReadinessListParser::ParseReadinessList(json_content),
            CookieInsightList({{"example.com", {"url"}}}));
}

}  // namespace content
