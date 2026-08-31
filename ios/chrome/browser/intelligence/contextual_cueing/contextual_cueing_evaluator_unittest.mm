// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/contextual_cueing/contextual_cueing_evaluator.h"

#import "components/page_content_annotations/core/page_content_annotation_type.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace contextual_cueing {

using ContextualCueingEvaluatorTest = PlatformTest;

TEST_F(ContextualCueingEvaluatorTest, EligiblePagePassesAllChecks) {
  ContextualCueingCapTrackerService cap_tracker;
  ContextualCueingEvaluator evaluator(&cap_tracker);

  // Note: 0.4f education is a deliberate below-threshold input to confirm only
  // shopping drives eligibility in this test case.
  std::vector<page_content_annotations::Category> categories = {
      {.category_type = page_content_annotations::CategoryType::kEducation,
       .score = 0.4f},
      {.category_type = page_content_annotations::CategoryType::kShopping,
       .score = 0.90f},
  };

  auto result =
      evaluator.Evaluate(GURL("https://example.com/store"), categories);

  EXPECT_TRUE(result.is_eligible());
  EXPECT_EQ(result.decision, ContextualCueingDecision::kSuccess);
  ASSERT_TRUE(result.top_category.has_value());
  EXPECT_EQ(result.top_category->category_type,
            page_content_annotations::CategoryType::kShopping);
  EXPECT_FLOAT_EQ(result.top_category->score, 0.90f);
}

TEST_F(ContextualCueingEvaluatorTest, ScoreBelowThresholdRejected) {
  ContextualCueingCapTrackerService cap_tracker;
  ContextualCueingEvaluator evaluator(&cap_tracker);

  // Score 0.30 is below default 0.50 threshold.
  std::vector<page_content_annotations::Category> categories = {
      {.category_type = page_content_annotations::CategoryType::kShopping,
       .score = 0.30f},
  };

  auto result =
      evaluator.Evaluate(GURL("https://example.com/product/123"), categories);

  EXPECT_FALSE(result.is_eligible());
  EXPECT_EQ(result.decision,
            ContextualCueingDecision::kFailedCategoryClassification);
  EXPECT_FALSE(result.top_category.has_value());
}

TEST_F(ContextualCueingEvaluatorTest, EmptyCategoriesRejected) {
  ContextualCueingCapTrackerService cap_tracker;
  ContextualCueingEvaluator evaluator(&cap_tracker);

  auto result = evaluator.Evaluate(GURL("https://example.com/product/123"), {});

  EXPECT_FALSE(result.is_eligible());
  EXPECT_EQ(result.decision,
            ContextualCueingDecision::kFailedCategoryClassification);
  EXPECT_FALSE(result.top_category.has_value());
}

TEST_F(ContextualCueingEvaluatorTest, UnsupportedMimeTypeRejected) {
  ContextualCueingCapTrackerService cap_tracker;
  ContextualCueingEvaluator evaluator(&cap_tracker);

  std::vector<page_content_annotations::Category> categories = {
      {.category_type = page_content_annotations::CategoryType::kShopping,
       .score = 0.95f},
  };

  auto result = evaluator.Evaluate(GURL("https://example.com/product/123"),
                                   categories, /*mime_type=*/"image/jpeg");

  EXPECT_FALSE(result.is_eligible());
  EXPECT_EQ(result.decision, ContextualCueingDecision::kUrlNotEligible);
}

TEST_F(ContextualCueingEvaluatorTest, GoogleSearchUrlRejected) {
  ContextualCueingCapTrackerService cap_tracker;
  ContextualCueingEvaluator evaluator(&cap_tracker);

  std::vector<page_content_annotations::Category> categories = {
      {.category_type = page_content_annotations::CategoryType::kShopping,
       .score = 0.95f},
  };

  auto result = evaluator.Evaluate(
      GURL("https://www.google.com/search?q=shoes"), categories);

  EXPECT_FALSE(result.is_eligible());
  EXPECT_EQ(result.decision, ContextualCueingDecision::kUrlNotEligible);
}

TEST_F(ContextualCueingEvaluatorTest, HomepageUrlRejected) {
  ContextualCueingCapTrackerService cap_tracker;
  ContextualCueingEvaluator evaluator(&cap_tracker);

  std::vector<page_content_annotations::Category> categories = {
      {.category_type = page_content_annotations::CategoryType::kShopping,
       .score = 0.95f},
  };

  auto result =
      evaluator.Evaluate(GURL("https://example.com/index.html"), categories);

  EXPECT_FALSE(result.is_eligible());
  EXPECT_EQ(result.decision, ContextualCueingDecision::kUrlNotEligible);
}

TEST_F(ContextualCueingEvaluatorTest, PdfMimeTypeRejected) {
  ContextualCueingCapTrackerService cap_tracker;
  ContextualCueingEvaluator evaluator(&cap_tracker);

  std::vector<page_content_annotations::Category> categories = {
      {.category_type = page_content_annotations::CategoryType::kShopping,
       .score = 0.95f},
  };

  auto result =
      evaluator.Evaluate(GURL("https://example.com/catalog.pdf"), categories,
                         /*mime_type=*/"application/pdf");

  // PDFs are unsupported on iOS and rejected as an ineligible MIME type.
  EXPECT_FALSE(result.is_eligible());
  EXPECT_EQ(result.decision, ContextualCueingDecision::kUrlNotEligible);
}

TEST_F(ContextualCueingEvaluatorTest, RateLimitedPageRejected) {
  ContextualCueingCapTrackerService::Config config;
  config.global_cap_count = 1;
  config.min_page_count_between_nudges = 0;
  config.min_time_between_nudges = base::TimeDelta();
  ContextualCueingCapTrackerService cap_tracker(config);
  ContextualCueingEvaluator evaluator(&cap_tracker);

  cap_tracker.RecordCueShown(GURL("https://a.com/page"));

  std::vector<page_content_annotations::Category> categories = {
      {.category_type = page_content_annotations::CategoryType::kShopping,
       .score = 0.95f},
  };

  auto result = evaluator.Evaluate(GURL("https://b.com/page"), categories);

  EXPECT_FALSE(result.is_eligible());
  EXPECT_EQ(result.decision,
            ContextualCueingDecision::kTooManyCuesShownToTheUser);
}

TEST_F(ContextualCueingEvaluatorTest, EvaluatePageEligibilityDirectly) {
  ContextualCueingCapTrackerService cap_tracker;
  ContextualCueingEvaluator evaluator(&cap_tracker);

  EXPECT_EQ(
      evaluator.EvaluatePageEligibility(GURL("https://example.com/store")),
      ContextualCueingDecision::kSuccess);
  EXPECT_EQ(evaluator.EvaluatePageEligibility(GURL("chrome://flags")),
            ContextualCueingDecision::kUrlNotEligible);
  EXPECT_EQ(evaluator.EvaluatePageEligibility(GURL("https://example.com/item"),
                                              "application/pdf"),
            ContextualCueingDecision::kUrlNotEligible);
}

TEST_F(ContextualCueingEvaluatorTest, EvaluateCategoryScoresDirectly) {
  ContextualCueingCapTrackerService cap_tracker;
  ContextualCueingEvaluator evaluator(&cap_tracker);

  std::vector<page_content_annotations::Category> eligible = {
      {.category_type = page_content_annotations::CategoryType::kShopping,
       .score = 0.85f},
  };
  auto result = evaluator.EvaluateCategoryScores(eligible);
  EXPECT_TRUE(result.is_eligible());
  EXPECT_EQ(result.decision, ContextualCueingDecision::kSuccess);

  std::vector<page_content_annotations::Category> ineligible = {
      {.category_type = page_content_annotations::CategoryType::kShopping,
       .score = 0.20f},
  };
  auto result2 = evaluator.EvaluateCategoryScores(ineligible);
  EXPECT_FALSE(result2.is_eligible());
  EXPECT_EQ(result2.decision,
            ContextualCueingDecision::kFailedCategoryClassification);
}

}  // namespace contextual_cueing
