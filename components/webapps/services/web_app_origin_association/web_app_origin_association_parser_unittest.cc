// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/services/web_app_origin_association/web_app_origin_association_parser.h"

#include "base/json/json_reader.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace webapps {

bool IsAssociationNull(const mojom::WebAppOriginAssociationPtr& association) {
  return !association;
}

// A WebAppOriginAssociation is empty if there are no AssociatedWebApps.
bool IsAssociationEmpty(const mojom::WebAppOriginAssociationPtr& association) {
  return association->apps.empty();
}

class WebAppOriginAssociationParserTest : public testing::Test {
 protected:
  WebAppOriginAssociationParserTest() = default;
  ~WebAppOriginAssociationParserTest() override = default;

  mojom::WebAppOriginAssociationPtr ParseAssociationData(
      const std::string& data) {
    WebAppOriginAssociationParser parser;
    mojom::WebAppOriginAssociationPtr association = parser.Parse(data);
    auto errors = parser.GetErrors();
    errors_.clear();
    for (auto& error : errors)
      errors_.push_back(std::move(error->message));
    failed_ = parser.failed();
    return association;
  }

  const std::vector<std::string>& errors() const { return errors_; }

  unsigned int GetErrorCount() const { return errors_.size(); }

  bool failed() const { return failed_; }

 private:
  std::vector<std::string> errors_;
  bool failed_;
};

TEST_F(WebAppOriginAssociationParserTest, CrashTest) {
  // Passing temporary variables should not crash.
  auto association = ParseAssociationData("{\"web_apps\": []}");

  // Parse() should have been call without crashing and succeeded.
  ASSERT_FALSE(failed());
  EXPECT_EQ(0u, GetErrorCount());
  EXPECT_FALSE(IsAssociationNull(association));
  EXPECT_TRUE(IsAssociationEmpty(association));
}

TEST_F(WebAppOriginAssociationParserTest, EmptyStringNull) {
  auto association = ParseAssociationData("");

  // This association is not a valid JSON object, it's a parsing error.
  ASSERT_TRUE(failed());
  EXPECT_TRUE(IsAssociationNull(association));
  EXPECT_EQ(1u, GetErrorCount());

  if (base::JSONReader::UsingRust()) {
    EXPECT_EQ(errors()[0], "EOF while parsing a value at line 1 column 0");
  } else {
    EXPECT_EQ(errors()[0], "Line: 1, column: 1, Unexpected token.");
  }
}

TEST_F(WebAppOriginAssociationParserTest, NoContentParses) {
  auto association = ParseAssociationData("{}");

  // Parsing succeeds for valid JSON.
  ASSERT_FALSE(failed());
  // No associated apps.
  ASSERT_FALSE(IsAssociationNull(association));
  ASSERT_TRUE(IsAssociationEmpty(association));
  EXPECT_EQ(1u, GetErrorCount());
  EXPECT_EQ(
      "Origin association ignored. Required property 'web_apps' expected.",
      errors()[0]);
}

TEST_F(WebAppOriginAssociationParserTest, InvalidWebApps) {
  // Error when "web_apps" is not an array.
  auto association = ParseAssociationData("{\"web_apps\": \"not-an-array\"}");

  ASSERT_FALSE(failed());
  EXPECT_EQ(1u, GetErrorCount());
  EXPECT_EQ("Property 'web_apps' ignored, type array expected.", errors()[0]);

  // "web_apps" is specified but invalid, check associated apps list is empty.
  EXPECT_FALSE(IsAssociationNull(association));
  EXPECT_TRUE(IsAssociationEmpty(association));
}

TEST_F(WebAppOriginAssociationParserTest, NoWebApps) {
  auto association = ParseAssociationData("{\"web_apps\": []}");

  ASSERT_FALSE(failed());
  EXPECT_EQ(0u, GetErrorCount());

  // "web_apps" specified but is empty, check associated apps list is empty.
  EXPECT_FALSE(IsAssociationNull(association));
  EXPECT_TRUE(IsAssociationEmpty(association));
}

TEST_F(WebAppOriginAssociationParserTest, ValidEmptyDetails) {
  std::string json =
      "{\"web_apps\": ["
      "  {"
      "    \"web_app_identity\": \"https://foo.com/index\""
      "  }"
      "]}";

  auto association = ParseAssociationData(json);
  ASSERT_FALSE(failed());
  ASSERT_FALSE(IsAssociationNull(association));
  ASSERT_FALSE(IsAssociationEmpty(association));
  EXPECT_EQ(0u, GetErrorCount());

  ASSERT_EQ(1u, association->apps.size());
}

TEST_F(WebAppOriginAssociationParserTest, MultipleErrorsReporting) {
  std::string json =
      "{\"web_apps\": ["
      // 1st app
      "  {\"no_web_app_identity_field\": 1},"
      // 2st app
      "  {\"web_app_identity\": 1},"
      // 3rd app
      "  {\"web_app_identity\": \"not-a-valid-url\"}"
      "]}";

  auto association = ParseAssociationData(json);
  ASSERT_FALSE(failed());
  ASSERT_FALSE(IsAssociationNull(association));
  ASSERT_TRUE(IsAssociationEmpty(association));
  ASSERT_EQ(3u, GetErrorCount());

  // 1st app
  EXPECT_EQ(
      "Associated app ignored. Required property 'web_app_identity' does "
      "not exist.",
      errors()[0]);
  // 2nd app
  EXPECT_EQ(
      "Associated app ignored. Required property 'web_app_identity' is not "
      "a string.",
      errors()[1]);
  // 3rd app
  EXPECT_EQ(
      "Associated app ignored. Required property 'web_app_identity' is not a "
      "valid URL.",
      errors()[2]);
}

TEST_F(WebAppOriginAssociationParserTest, MoreThanOneValidApp) {
  std::string json =
      "{\"web_apps\": ["
      // 1st app
      "  {"
      "    \"web_app_identity\": \"https://foo.com/index\""
      "  },"
      // 2nd app
      "  {"
      "    \"web_app_identity\": \"https://foo.app/index\""
      "  },"
      // 3rd app
      "  {"
      "    \"web_app_identity\": \"https://foo.dev\""
      "  }"
      "]}";

  auto association = ParseAssociationData(json);
  ASSERT_FALSE(failed());
  ASSERT_FALSE(IsAssociationNull(association));
  ASSERT_FALSE(IsAssociationEmpty(association));
  ASSERT_EQ(0u, GetErrorCount());

  // 1st app
  EXPECT_EQ(GURL("https://foo.com/index"),
            association->apps[0]->web_app_identity);
  // 2nd app
  EXPECT_EQ(GURL("https://foo.app/index"),
            association->apps[1]->web_app_identity);
  // 3rd app
  EXPECT_EQ(GURL("https://foo.dev"), association->apps[2]->web_app_identity);
}

TEST_F(WebAppOriginAssociationParserTest, IgnoreInvalidAndValidateTwosApps) {
  std::string json =
      "{\"web_apps\": ["
      // 1st app
      "  {"
      "    \"web_app_identity\": \"https://foo.com/index\""
      "  },"
      // 2nd app
      "  {"
      "    \"web_app_identity\": \"https://foo.app/index\""
      "  },"
      // 3rd app
      "  {"
      "    \"web_app_identity\": \"not-a-valid-url\""
      "  }"
      "]}";

  auto association = ParseAssociationData(json);
  ASSERT_FALSE(failed());
  ASSERT_FALSE(IsAssociationNull(association));
  ASSERT_FALSE(IsAssociationEmpty(association));
  ASSERT_EQ(1u, GetErrorCount());

  ASSERT_EQ(2u, association->apps.size());

  // 1st app
  EXPECT_EQ(GURL("https://foo.com/index"),
            association->apps[0]->web_app_identity);
  // 2nd app
  EXPECT_EQ(GURL("https://foo.app/index"),
            association->apps[1]->web_app_identity);
  // 3rd app
  EXPECT_EQ(
      "Associated app ignored. Required property 'web_app_identity' is not a "
      "valid URL.",
      errors()[0]);
}

}  // namespace webapps
