// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/services/web_app_origin_association/web_app_origin_association_parser.h"

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
  EXPECT_NE(std::string::npos, errors()[0].find("Line: 1, column: 1,"));
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
      "    \"manifest\": \"https://foo.com/manifest.json\","
      "    \"details\": {}"
      "  }"
      "]}";

  auto association = ParseAssociationData(json);
  ASSERT_FALSE(failed());
  ASSERT_FALSE(IsAssociationNull(association));
  ASSERT_FALSE(IsAssociationEmpty(association));
  EXPECT_EQ(0u, GetErrorCount());

  ASSERT_EQ(1u, association->apps.size());

  EXPECT_FALSE(association->apps[0]->paths.has_value());
  EXPECT_FALSE(association->apps[0]->exclude_paths.has_value());
}

TEST_F(WebAppOriginAssociationParserTest, MultipleErrorsReporting) {
  std::string json =
      "{\"web_apps\": ["
      // 1st app
      "  {\"no-manifest-field\": 1},"
      // 2st app
      "  {\"manifest\": 1},"
      // 3rd app
      "  {\"manifest\": \"not-a-valid-url\"},"
      // 4th app
      "  {"
      "    \"manifest\": \"https://foo.com/manifest.json\","
      "    \"details\": \"not-an-array\""
      "  },"
      // 5th app
      "  {"
      "    \"manifest\": \"https://foo.com/manifest.json\","
      "    \"details\": {"
      "      \"paths\": \"not-an-array\","
      "      \"exclude_paths\": \"not-an-array\""
      "     }"
      "  },"
      // 6th app
      "  {"
      "    \"manifest\": \"https://foo.com/manifest.json\","
      "    \"details\": {"
      "      \"paths\": [1]"
      "     }"
      "  },"
      // 7th app
      "  {"
      "    \"manifest\": \"https://foo.com/manifest.json\","
      "    \"details\": {"
      "      \"exclude_paths\": [1]"
      "     }"
      "  }"
      "]}";

  auto association = ParseAssociationData(json);
  ASSERT_FALSE(failed());
  ASSERT_FALSE(IsAssociationNull(association));
  ASSERT_FALSE(IsAssociationEmpty(association));
  ASSERT_EQ(8u, GetErrorCount());

  // 1st app
  EXPECT_EQ(
      "Associated app ignored. Required property 'manifest' does not exist.",
      errors()[0]);
  // 2nd app
  EXPECT_EQ(
      "Associated app ignored. Required property 'manifest' is not a string.",
      errors()[1]);
  // 3rd app
  EXPECT_EQ(
      "Associated app ignored. Required property 'manifest' is not a valid "
      "URL.",
      errors()[2]);
  // 4th app
  EXPECT_EQ("Property 'details' ignored, type dictionary expected.",
            errors()[3]);
  // 5th app
  EXPECT_EQ("Property 'paths' ignored, type array expected.", errors()[4]);
  EXPECT_EQ("Property 'exclude_paths' ignored, type array expected.",
            errors()[5]);
  // 6th app
  EXPECT_EQ("paths entry ignored, type string expected.", errors()[6]);
  // 7th app
  EXPECT_EQ("exclude_paths entry ignored, type string expected.", errors()[7]);
}

TEST_F(WebAppOriginAssociationParserTest, MoreThanOneValidApp) {
  std::string json =
      "{\"web_apps\": ["
      // 1st app
      "  {"
      "    \"manifest\": \"https://foo.com/manifest.json\""
      "  },"
      // 2nd app
      "  {"
      "    \"manifest\": \"https://foo.com/manifest.json\","
      "    \"details\": {"
      "      \"paths\": [\"/blog\"]"
      "    }"
      "  },"
      // 3rd app
      "  {"
      "    \"manifest\": \"https://foo.com/manifest.json\","
      "    \"details\": {"
      "      \"paths\": [\"/*\"],"
      "      \"exclude_paths\": [\"/blog/data\"]"
      "    }"
      "  }"
      "]}";

  auto association = ParseAssociationData(json);
  ASSERT_FALSE(failed());
  ASSERT_FALSE(IsAssociationNull(association));
  ASSERT_FALSE(IsAssociationEmpty(association));
  ASSERT_EQ(0u, GetErrorCount());

  ASSERT_EQ(3u, association->apps.size());

  // 1st app
  EXPECT_EQ(GURL("https://foo.com/manifest.json"),
            association->apps[0]->manifest_url);
  // 2nd app
  EXPECT_TRUE(association->apps[1]->paths.has_value());
  EXPECT_EQ("/blog", association->apps[1]->paths.value()[0]);
  // 3rd app
  EXPECT_TRUE(association->apps[2]->paths.has_value());
  EXPECT_EQ("/*", association->apps[2]->paths.value()[0]);
  EXPECT_TRUE(association->apps[2]->exclude_paths.has_value());
  EXPECT_EQ("/blog/data", association->apps[2]->exclude_paths.value()[0]);
}

TEST_F(WebAppOriginAssociationParserTest, MoreThanOnePath) {
  std::string json =
      "{\"web_apps\": ["
      "  {"
      "    \"manifest\": \"https://foo.com/manifest.json\","
      "    \"details\": {"
      "      \"paths\": [\"/blog/*\", \"/about\", \"/public/data\"]"
      "     }"
      "  }"
      "]}";

  auto association = ParseAssociationData(json);
  ASSERT_FALSE(failed());
  ASSERT_FALSE(IsAssociationNull(association));
  ASSERT_FALSE(IsAssociationEmpty(association));
  EXPECT_EQ(0u, GetErrorCount());

  ASSERT_EQ(1u, association->apps.size());
  ASSERT_TRUE(association->apps[0]->paths.has_value());
  auto& paths = association->apps[0]->paths.value();
  ASSERT_EQ(3u, paths.size());
  EXPECT_EQ("/blog/*", paths[0]);
  EXPECT_EQ("/about", paths[1]);
  EXPECT_EQ("/public/data", paths[2]);
}

TEST_F(WebAppOriginAssociationParserTest, MoreThanOneExcludePath) {
  std::string json =
      "{\"web_apps\": ["
      "  {"
      "    \"manifest\": \"https://foo.com/manifest.json\","
      "    \"details\": {"
      "      \"exclude_paths\": [\"/blog/*\", \"/about\", \"/public/data\"]"
      "     }"
      "  }"
      "]}";

  auto association = ParseAssociationData(json);
  ASSERT_FALSE(failed());
  ASSERT_FALSE(IsAssociationNull(association));
  ASSERT_FALSE(IsAssociationEmpty(association));
  EXPECT_EQ(0u, GetErrorCount());

  ASSERT_EQ(1u, association->apps.size());
  ASSERT_TRUE(association->apps[0]->exclude_paths.has_value());
  auto& exclude_paths = association->apps[0]->exclude_paths.value();
  ASSERT_EQ(3u, exclude_paths.size());
  EXPECT_EQ("/blog/*", exclude_paths[0]);
  EXPECT_EQ("/about", exclude_paths[1]);
  EXPECT_EQ("/public/data", exclude_paths[2]);
}

TEST_F(WebAppOriginAssociationParserTest, ValidAndInvalidPaths) {
  std::string json =
      "{\"web_apps\": ["
      "  {"
      "    \"manifest\": \"https://foo.com/manifest.json\","
      "    \"details\": {"
      "      \"exclude_paths\": [\"/blog/*\", 1, \"/public/data\", true]"
      "     }"
      "  }"
      "]}";

  auto association = ParseAssociationData(json);
  ASSERT_FALSE(failed());
  ASSERT_FALSE(IsAssociationNull(association));
  ASSERT_FALSE(IsAssociationEmpty(association));

  // Check that there are two errors from parsing invalid exclude paths.
  EXPECT_EQ(2u, GetErrorCount());
  EXPECT_EQ("exclude_paths entry ignored, type string expected.", errors()[0]);
  EXPECT_EQ("exclude_paths entry ignored, type string expected.", errors()[1]);

  ASSERT_EQ(1u, association->apps.size());
  ASSERT_TRUE(association->apps[0]->exclude_paths.has_value());
  auto& exclude_paths = association->apps[0]->exclude_paths.value();
  // Check that the invalid entries are skipped and the valid ones are parsed.
  ASSERT_EQ(2u, exclude_paths.size());
  EXPECT_EQ("/blog/*", exclude_paths[0]);
  EXPECT_EQ("/public/data", exclude_paths[1]);
}

}  // namespace webapps
