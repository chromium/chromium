// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/services/web_app_origin_association/web_app_origin_association_parser_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
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
      "{"
      "  \"web_apps\": [{"
      "    \"web_app_identity\":\"https://foo.com/\""
      "    }]"
      "}";
  base::test::TestFuture<mojom::WebAppOriginAssociationPtr,
                         std::vector<mojom::WebAppOriginAssociationErrorPtr>>
      future;

  ParseWebAppOriginAssociation(raw_json, future.GetCallback());
  auto [association, errors] = future.Take();

  ASSERT_FALSE(!association);
  ASSERT_FALSE(association == mojom::WebAppOriginAssociation::New());
  ASSERT_TRUE(errors.empty());

  ASSERT_EQ(1u, association->apps.size());
  EXPECT_EQ(GURL("https://foo.com/"), association->apps[0]->web_app_identity);

  histogram_tester_.ExpectBucketCount(
      kParseResultHistogram,
      WebAppOriginAssociationMetrics::ParseResult::kParseSucceeded, 1);
}

TEST_F(WebAppOriginAssociationParserImplTest,
       ParseBadAssociationFileNotADictionary) {
  std::string raw_json = "\"invalid\"";
  base::test::TestFuture<mojom::WebAppOriginAssociationPtr,
                         std::vector<mojom::WebAppOriginAssociationErrorPtr>>
      future;

  ParseWebAppOriginAssociation(raw_json, future.GetCallback());
  auto [association, errors] = future.Take();

  ASSERT_TRUE(!association);
  ASSERT_FALSE(errors.empty());
  ASSERT_EQ(1u, errors.size());
  EXPECT_EQ("No valid JSON object found.", errors[0]->message);

  histogram_tester_.ExpectBucketCount(
      kParseResultHistogram,
      WebAppOriginAssociationMetrics::ParseResult::kParseFailedNotADictionary,
      1);
}

TEST_F(WebAppOriginAssociationParserImplTest,
       ParseBadAssociationFileInvalidJson) {
  std::string raw_json = "[1, 2";
  base::test::TestFuture<mojom::WebAppOriginAssociationPtr,
                         std::vector<mojom::WebAppOriginAssociationErrorPtr>>
      future;

  ParseWebAppOriginAssociation(raw_json, future.GetCallback());
  auto [association, errors] = future.Take();

  ASSERT_TRUE(!association);
  ASSERT_FALSE(errors.empty());
  ASSERT_EQ(1u, errors.size());
  if (base::JSONReader::UsingRust()) {
    EXPECT_EQ(errors[0]->message,
              "EOF while parsing a list at line 1 column 5");
  } else {
    EXPECT_EQ(errors[0]->message, "Line: 1, column: 6, Syntax error.");
  }

  histogram_tester_.ExpectBucketCount(
      kParseResultHistogram,
      WebAppOriginAssociationMetrics::ParseResult::kParseFailedInvalidJson, 1);
}

}  // namespace webapps
