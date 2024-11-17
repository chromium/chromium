// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/services/web_app_origin_association/web_app_origin_association_parser.h"

#include "base/json/json_reader.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

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
  WebAppOriginAssociationParserTest() {
    associate_origin_ = url::Origin::Create(GetAssociateOriginUrl());
  }
  ~WebAppOriginAssociationParserTest() override = default;

  mojom::WebAppOriginAssociationPtr ParseAssociationData(
      const std::string& data) {
    WebAppOriginAssociationParser parser;
    mojom::WebAppOriginAssociationPtr association =
        parser.Parse(data, GetAssociateOrigin());
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

  const url::Origin GetAssociateOrigin() { return associate_origin_; }
  const GURL GetAssociateOriginUrl() { return GURL(associate_origin_str_); }
  const std::string GetAssociateOriginString() { return associate_origin_str_; }

 private:
  const std::string associate_origin_str_ = "https://example.co.uk";
  url::Origin associate_origin_;
  std::vector<std::string> errors_;
  bool failed_;
};

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
  EXPECT_EQ(kWebAppOriginAssociationParserFormatError, errors()[0]);
}

TEST_F(WebAppOriginAssociationParserTest, InvalidScopeType) {
  auto association = ParseAssociationData("{\"https://example.com/app\": [] }");

  ASSERT_FALSE(failed());
  EXPECT_EQ(1u, GetErrorCount());
  EXPECT_EQ(kInvalidValueType, errors()[0]);

  // "web_apps" is specified but invalid, check associated apps list is empty.
  EXPECT_FALSE(IsAssociationNull(association));
  EXPECT_TRUE(IsAssociationEmpty(association));
}

TEST_F(WebAppOriginAssociationParserTest, ValidEmptyScope) {
  auto association = ParseAssociationData("{\"https://example.com/app\": {} }");

  ASSERT_FALSE(failed());
  EXPECT_EQ(0u, GetErrorCount());

  EXPECT_FALSE(IsAssociationNull(association));
  EXPECT_FALSE(IsAssociationEmpty(association));
  EXPECT_EQ(GURL("https://example.com/app"),
            association->apps[0]->web_app_identity);
}

TEST_F(WebAppOriginAssociationParserTest, MultipleErrorsReporting) {
  std::string json =
      R"({
      "https://foo.com": { "scope": "https://bar.com" },
      "https://foo.com/index": [],
      "not-a-valid-url": {}
      })";

  auto association = ParseAssociationData(json);
  ASSERT_FALSE(failed());
  ASSERT_FALSE(IsAssociationNull(association));
  ASSERT_TRUE(IsAssociationEmpty(association));
  ASSERT_EQ(3u, GetErrorCount());

  EXPECT_EQ(kInvalidScopeUrl, errors()[0]);
  EXPECT_EQ(kInvalidValueType, errors()[1]);
  EXPECT_EQ(kInvalidManifestId, errors()[2]);
}

TEST_F(WebAppOriginAssociationParserTest, MultipleValidApps) {
  std::string json =
      R"({
       "https://foo.app/index": {},
       "https://foo.com": { "scope": "qwerty" },
       "https://foo.com/index": { "scope": "/app" },
       "https://foo.dev/index": { "scope": "/" },
       "https://zed.com/index": { "scope": "https://example.co.uk" }
      })";

  auto association = ParseAssociationData(json);
  ASSERT_FALSE(failed());
  ASSERT_FALSE(IsAssociationNull(association));
  ASSERT_FALSE(IsAssociationEmpty(association));
  ASSERT_EQ(0u, GetErrorCount());
  ASSERT_EQ(5u, association->apps.size());

  EXPECT_EQ(GURL("https://foo.app/index"),
            association->apps[0]->web_app_identity);
  EXPECT_EQ(GetAssociateOriginUrl(), association->apps[0]->scope);

  EXPECT_EQ(GURL("https://foo.com"), association->apps[1]->web_app_identity);
  EXPECT_EQ(GURL(GetAssociateOriginString() + "/qwerty"),
            association->apps[1]->scope);

  EXPECT_EQ(GURL("https://foo.com/index"),
            association->apps[2]->web_app_identity);
  EXPECT_EQ(GURL(GetAssociateOriginString() + "/app"),
            association->apps[2]->scope);

  EXPECT_EQ(GURL("https://foo.dev/index"),
            association->apps[3]->web_app_identity);
  EXPECT_EQ(GetAssociateOriginUrl(), association->apps[3]->scope);

  EXPECT_EQ(GURL("https://zed.com/index"),
            association->apps[4]->web_app_identity);
  EXPECT_EQ(GetAssociateOriginUrl(), association->apps[4]->scope);
}

TEST_F(WebAppOriginAssociationParserTest, IgnoreInvalidAndValidateTwosApps) {
  std::string json =
      R"({
       "https://foo.com/index": {},
       "invalid-app-url": {},
       "https://foo.dev": {}
      })";

  auto association = ParseAssociationData(json);
  ASSERT_FALSE(failed());
  ASSERT_FALSE(IsAssociationNull(association));
  ASSERT_FALSE(IsAssociationEmpty(association));

  ASSERT_EQ(1u, GetErrorCount());
  EXPECT_EQ(kInvalidManifestId, errors()[0]);

  ASSERT_EQ(2u, association->apps.size());
  EXPECT_EQ(GURL("https://foo.com/index"),
            association->apps[0]->web_app_identity);
  EXPECT_EQ(GetAssociateOriginUrl(), association->apps[0]->scope);

  EXPECT_EQ(GURL("https://foo.dev"), association->apps[1]->web_app_identity);
  EXPECT_EQ(GetAssociateOriginUrl(), association->apps[1]->scope);
}

}  // namespace webapps
