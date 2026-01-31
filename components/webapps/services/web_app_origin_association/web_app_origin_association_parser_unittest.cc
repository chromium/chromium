// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/services/web_app_origin_association/web_app_origin_association_parser.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::FieldsAre;
using ::testing::IsEmpty;

namespace webapps {

class WebAppOriginAssociationParserTest : public testing::Test {
 protected:
  WebAppOriginAssociationParserTest() {
    associate_origin_ = url::Origin::Create(GetAssociateOriginUrl());
  }
  ~WebAppOriginAssociationParserTest() override = default;

  base::expected<ParsedAssociations, std::string> ParseAssociationData(
      const std::string& data) {
    return ParseWebAppOriginAssociations(data, GetAssociateOrigin());
  }

  const url::Origin& GetAssociateOrigin() { return associate_origin_; }
  GURL GetAssociateOriginUrl() { return GURL(associate_origin_str_); }
  const std::string& GetAssociateOriginString() {
    return associate_origin_str_;
  }

 private:
  const std::string associate_origin_str_ = "https://example.co.uk";
  url::Origin associate_origin_;
};

TEST_F(WebAppOriginAssociationParserTest, EmptyStringNull) {
  auto result = ParseAssociationData("");

  // This association is not a valid JSON object, it's a parsing error.
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            "line 1, column 0: EOF while parsing a value at line 1 column 0");
}

TEST_F(WebAppOriginAssociationParserTest, NoContentParses) {
  auto result = ParseAssociationData("{}");

  // Parsing succeeds for valid JSON.
  ASSERT_TRUE(result.has_value());
  // But no associated apps.
  EXPECT_THAT(result->apps, IsEmpty());
  EXPECT_THAT(result->warnings,
              ElementsAre(kWebAppOriginAssociationParserFormatError));
}

TEST_F(WebAppOriginAssociationParserTest, InvalidScopeType) {
  auto result = ParseAssociationData("{\"https://example.com/app\": [] }");

  ASSERT_TRUE(result.has_value());
  // "web_apps" is specified but invalid, check associated apps list is empty.
  ASSERT_THAT(result->apps, IsEmpty());
  EXPECT_THAT(result->warnings, ElementsAre(kInvalidValueType));
}

TEST_F(WebAppOriginAssociationParserTest, ValidEmptyScope) {
  auto result = ParseAssociationData("{\"https://example.com/app\": {} }");

  EXPECT_TRUE(result.has_value());
  EXPECT_THAT(result->apps,
              ElementsAre(FieldsAre(
                  /*web_app_identity=*/GURL("https://example.com/app"),
                  /*scope=*/_,
                  /*allow_migration=*/false)));
}

TEST_F(WebAppOriginAssociationParserTest, AllowMigration) {
  auto result = ParseAssociationData(
      R"({"https://example.com/app": {"allow_migration": true} })");

  ASSERT_TRUE(result.has_value());
  EXPECT_THAT(result->apps,
              ElementsAre(FieldsAre(
                  /*web_app_identity=*/GURL("https://example.com/app"),
                  /*scope=*/_,
                  /*allow_migration=*/true)));
}

TEST_F(WebAppOriginAssociationParserTest, InvalidScopeUrlWithAllowMigration) {
  auto result = ParseAssociationData(
      R"({"https://example.com/app": {"scope": "https://other.com", "allow_migration": true} })");

  ASSERT_TRUE(result.has_value());
  EXPECT_THAT(result->apps,
              ElementsAre(FieldsAre(
                  /*web_app_identity=*/GURL("https://example.com/app"),
                  /*scope=*/GURL(),
                  /*allow_migration=*/true)));
  EXPECT_THAT(result->warnings, ElementsAre(kInvalidScopeUrl));
}

TEST_F(WebAppOriginAssociationParserTest, MultipleErrorsReporting) {
  std::string json =
      R"({
      "https://foo.com": { "scope": "https://bar.com" },
      "https://foo.com/index": [],
      "not-a-valid-url": {}
      })";

  auto result = ParseAssociationData(json);
  ASSERT_TRUE(result.has_value());
  EXPECT_THAT(result->apps, ElementsAre(FieldsAre(
                                /*web_app_identity=*/GURL("https://foo.com"),
                                /*scope=*/GURL(),
                                /*allow_migration=*/false)));
  EXPECT_THAT(result->warnings, ElementsAre(kInvalidScopeUrl, kInvalidValueType,
                                            kInvalidManifestId));
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

  auto result = ParseAssociationData(json);
  ASSERT_TRUE(result.has_value());

  EXPECT_THAT(result->warnings, IsEmpty());

  EXPECT_THAT(
      result->apps,
      ElementsAre(
          FieldsAre(/*web_app_identity=*/GURL("https://foo.app/index"),
                    /*scope=*/GetAssociateOriginUrl(),
                    /*allow_migration=*/false),
          FieldsAre(/*web_app_identity=*/GURL("https://foo.com"),
                    /*scope=*/GURL(GetAssociateOriginString() + "/qwerty"),
                    /*allow_migration=*/false),
          FieldsAre(/*web_app_identity=*/GURL("https://foo.com/index"),
                    /*scope=*/GURL(GetAssociateOriginString() + "/app"),
                    /*allow_migration=*/false),
          FieldsAre(/*web_app_identity=*/GURL("https://foo.dev/index"),
                    /*scope=*/GetAssociateOriginUrl(),
                    /*allow_migration=*/false),
          FieldsAre(/*web_app_identity=*/GURL("https://zed.com/index"),
                    /*scope=*/GetAssociateOriginUrl(),
                    /*allow_migration=*/false)));
}

TEST_F(WebAppOriginAssociationParserTest, IgnoreInvalidAndValidateTwosApps) {
  std::string json =
      R"({
       "https://foo.com/index": {},
       "invalid-app-url": {},
       "https://foo.dev": {}
      })";

  auto result = ParseAssociationData(json);
  ASSERT_TRUE(result.has_value());

  EXPECT_THAT(result->warnings, ElementsAre(kInvalidManifestId));

  EXPECT_THAT(
      result->apps,
      ElementsAre(FieldsAre(/*web_app_identity=*/GURL("https://foo.com/index"),
                            /*scope=*/GetAssociateOriginUrl(),
                            /*allow_migration=*/false),
                  FieldsAre(/*web_app_identity=*/GURL("https://foo.dev"),
                            /*scope=*/GetAssociateOriginUrl(),
                            /*allow_migration=*/false)));
}

}  // namespace webapps
