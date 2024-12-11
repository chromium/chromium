// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cookie_insight_list/cookie_insight_list_handler.h"

#include <optional>
#include <string>

#include "content/browser/cookie_insight_list/cookie_insight_list.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

using testing::Optional;

class CookieInsightListHandlerTest : public ::testing::Test {
 public:
  CookieInsightListHandlerTest() = default;

  void SetUp() override {
    CookieInsightListHandler::GetInstance().set_insight_list("");
  }
};

TEST_F(CookieInsightListHandlerTest, GetInsight_GitHubResource_ListUpdate) {
  const std::string json_content = R"({"entries": [
    {
      "domains": [
          "example.com",
      ],
      "tableEntryUrl": "url"
    }]
  })";
  CookieInsightListHandler::GetInstance().set_insight_list(json_content);

  EXPECT_THAT(
      CookieInsightListHandler::GetInstance().GetInsight("example.com", {}),
      Optional(CookieInsightList::CookieIssueInsight{
          CookieInsightList::InsightType::kGitHubResource,
          CookieInsightList::DomainInfo{"url"}}));

  const std::string new_json_content = R"({"entries": [
    {
      "domains": [
          "example.com",
      ],
      "tableEntryUrl": "newUrl"
    }]
  })";
  CookieInsightListHandler::GetInstance().set_insight_list(new_json_content);

  EXPECT_THAT(
      CookieInsightListHandler::GetInstance().GetInsight("example.com", {}),
      Optional(CookieInsightList::CookieIssueInsight{
          CookieInsightList::InsightType::kGitHubResource,
          CookieInsightList::DomainInfo{"newUrl"}}));
}

}  // namespace content
