// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/services/web_app_origin_association/web_app_origin_association_parser_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/webapps/services/web_app_origin_association/web_app_origin_association_uma_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr char kParseResultHistogram[] =
    "Webapp.WebAppOriginAssociationParseResult";
}

namespace webapps {

class WebAppOriginAssociationParserImplTest : public testing::Test {
 protected:
  WebAppOriginAssociationParserImplTest() {
    mojo::PendingReceiver<mojom::WebAppOriginAssociationParser> receiver;
    parser_ = std::make_unique<WebAppOriginAssociationParserImpl>(
        std::move(receiver));
  }

  void TearDown() override { parser_.reset(); }

  void ParseWebAppOriginAssociation(
      const std::string& raw_json,
      mojom::WebAppOriginAssociationParser::ParseWebAppOriginAssociationCallback
          callback) {
    parser_->ParseWebAppOriginAssociation(raw_json, std::move(callback));
    scoped_task_environment_.RunUntilIdle();
  }

  base::test::TaskEnvironment scoped_task_environment_;
  std::unique_ptr<WebAppOriginAssociationParserImpl> parser_;
  base::HistogramTester histogram_tester_;
};

TEST_F(WebAppOriginAssociationParserImplTest, ParseGoodAssociationFile) {
  std::string raw_json =
      "{\"web_apps\": ["
      "  {"
      "    \"manifest\": \"https://foo.com/manifest.json\","
      "    \"details\": {}"
      "  }"
      "]}";

  base::RunLoop run_loop;
  ParseWebAppOriginAssociation(
      raw_json,
      base::BindLambdaForTesting(
          [&](mojom::WebAppOriginAssociationPtr association,
              std::vector<mojom::WebAppOriginAssociationErrorPtr> errors) {
            ASSERT_FALSE(!association);
            ASSERT_FALSE(association == mojom::WebAppOriginAssociation::New());
            ASSERT_TRUE(errors.empty());

            ASSERT_EQ(1u, association->apps.size());
            EXPECT_EQ("https://foo.com/manifest.json",
                      association->apps[0]->manifest_url);

            histogram_tester_.ExpectBucketCount(
                kParseResultHistogram,
                WebAppOriginAssociationMetrics::ParseResult::kParseSucceeded,
                1);
            run_loop.Quit();
          }));

  run_loop.Run();
}

TEST_F(WebAppOriginAssociationParserImplTest,
       ParseBadAssociationFileNotADictionary) {
  std::string raw_json = "\"invalid\"";

  base::RunLoop run_loop;
  ParseWebAppOriginAssociation(
      raw_json,
      base::BindLambdaForTesting(
          [&](mojom::WebAppOriginAssociationPtr association,
              std::vector<mojom::WebAppOriginAssociationErrorPtr> errors) {
            ASSERT_TRUE(!association);
            ASSERT_FALSE(errors.empty());
            ASSERT_EQ(1u, errors.size());
            EXPECT_EQ("No valid JSON object found.", errors[0]->message);

            histogram_tester_.ExpectBucketCount(
                kParseResultHistogram,
                WebAppOriginAssociationMetrics::ParseResult::
                    kParseFailedNotADictionary,
                1);
            run_loop.Quit();
          }));

  run_loop.Run();
}

TEST_F(WebAppOriginAssociationParserImplTest,
       ParseBadAssociationFileInvalidJson) {
  std::string raw_json = "[1, 2";

  base::RunLoop run_loop;
  ParseWebAppOriginAssociation(
      raw_json,
      base::BindLambdaForTesting(
          [&](mojom::WebAppOriginAssociationPtr association,
              std::vector<mojom::WebAppOriginAssociationErrorPtr> errors) {
            ASSERT_TRUE(!association);
            ASSERT_FALSE(errors.empty());
            ASSERT_EQ(1u, errors.size());
            EXPECT_NE(std::string::npos,
                      errors[0]->message.find("Line: 1, column: 6,"));

            histogram_tester_.ExpectBucketCount(
                kParseResultHistogram,
                WebAppOriginAssociationMetrics::ParseResult::
                    kParseFailedInvalidJson,
                1);
            run_loop.Quit();
          }));

  run_loop.Run();
}

}  // namespace webapps
