// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/page_classification/optimization_guide_page_classification_service.h"

#import <memory>
#import <string>
#import <string_view>
#import <vector>

#import "base/memory/raw_ptr.h"
#import "base/test/test_future.h"
#import "base/values.h"
#import "components/optimization_guide/core/hints/optimization_guide_decider.h"
#import "components/optimization_guide/proto/hints.pb.h"
#import "components/optimization_guide/proto/page_entities_metadata.pb.h"
#import "ios/chrome/browser/intelligence/page_classification/education_eligibility_vertical.h"
#import "ios/chrome/browser/intelligence/page_classification/education_java_script_feature.h"
#import "ios/chrome/browser/intelligence/page_classification/page_classification_service.h"
#import "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

constexpr std::string_view kTestCategoryPrefixes[] = {
    "/Science",
    "/Reference",
};

constexpr std::string_view kTestAcademicMIDs[] = {
    "/m/06mq7",
};

class FakeOptimizationGuideDecider
    : public optimization_guide::OptimizationGuideDecider {
 public:
  FakeOptimizationGuideDecider() = default;
  ~FakeOptimizationGuideDecider() override = default;

  void RegisterOptimizationTypes(
      const std::vector<optimization_guide::proto::OptimizationType>&
          optimization_types) override {
    registered_types_ = optimization_types;
  }

  void CanApplyOptimization(
      const GURL& url,
      optimization_guide::proto::OptimizationType optimization_type,
      optimization_guide::OptimizationGuideDecisionCallback callback) override {
    last_url_ = url;
    last_type_ = optimization_type;
    std::move(callback).Run(decision_, metadata_);
  }

  optimization_guide::OptimizationGuideDecision CanApplyOptimization(
      const GURL& url,
      optimization_guide::proto::OptimizationType optimization_type,
      optimization_guide::OptimizationMetadata* optimization_metadata)
      override {
    return optimization_guide::OptimizationGuideDecision::kFalse;
  }

  void CanApplyOptimizationOnDemand(
      const std::vector<GURL>& urls,
      const base::flat_set<optimization_guide::proto::OptimizationType>&
          optimization_types,
      optimization_guide::proto::RequestContext request_context,
      optimization_guide::OnDemandOptimizationGuideDecisionRepeatingCallback
          callback,
      std::optional<optimization_guide::proto::RequestContextMetadata>
          request_context_metadata = std::nullopt) override {}

  void SetResponse(optimization_guide::OptimizationGuideDecision decision,
                   const optimization_guide::OptimizationMetadata& metadata) {
    decision_ = decision;
    metadata_ = metadata;
  }

  const std::vector<optimization_guide::proto::OptimizationType>&
  registered_types() const {
    return registered_types_;
  }

  const GURL& last_url() const { return last_url_; }
  optimization_guide::proto::OptimizationType last_type() const {
    return last_type_;
  }

 private:
  std::vector<optimization_guide::proto::OptimizationType> registered_types_;
  GURL last_url_;
  optimization_guide::proto::OptimizationType last_type_ =
      optimization_guide::proto::TYPE_UNSPECIFIED;
  optimization_guide::OptimizationGuideDecision decision_ =
      optimization_guide::OptimizationGuideDecision::kFalse;
  optimization_guide::OptimizationMetadata metadata_;
};

}  // namespace

class OptimizationGuidePageClassificationServiceTest : public PlatformTest {
 public:
  OptimizationGuidePageClassificationServiceTest() = default;
  ~OptimizationGuidePageClassificationServiceTest() override = default;

 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    scoped_categories_ = std::make_unique<ScopedEducationCategoriesForTesting>(
        kTestCategoryPrefixes, kTestAcademicMIDs);
    web::test::OverrideJavaScriptFeatures(
        &fake_browser_state_, {EducationJavaScriptFeature::GetInstance()});
    fake_web_state_.SetBrowserState(&fake_browser_state_);

    auto frames_manager = std::make_unique<web::FakeWebFramesManager>();
    auto main_frame = web::FakeWebFrame::CreateMainWebFrame();
    main_frame->set_browser_state(&fake_browser_state_);
    main_frame_ = main_frame.get();
    frames_manager->AddWebFrame(std::move(main_frame));
    fake_web_state_.SetWebFramesManager(
        EducationJavaScriptFeature::GetInstance()->GetSupportedContentWorld(),
        std::move(frames_manager));

    service_ =
        std::make_unique<OptimizationGuidePageClassificationService>(&decider_);
  }

  void TearDown() override {
    service_->Shutdown();
    PlatformTest::TearDown();
  }

  void SetOptimizationGuideEducationResponse(std::string_view category_id,
                                             float score) {
    optimization_guide::proto::PageEntitiesMetadata page_entities;
    auto* cat = page_entities.add_categories();
    cat->set_category_id(std::string(category_id));
    cat->set_score(score);

    optimization_guide::proto::Any any_metadata;
    any_metadata.set_type_url(
        "type.googleapis.com/optimization_guide.proto.PageEntitiesMetadata");
    page_entities.SerializeToString(any_metadata.mutable_value());

    optimization_guide::OptimizationMetadata metadata;
    metadata.set_any_metadata(any_metadata);
    decider_.SetResponse(optimization_guide::OptimizationGuideDecision::kTrue,
                         metadata);
  }

  void SetDOMFeaturesResponse(int word_count, int heading_count) {
    base::DictValue response_dict;
    response_dict.Set("word_count", word_count);
    response_dict.Set("heading_count", heading_count);
    dom_response_value_ =
        std::make_unique<base::Value>(std::move(response_dict));
    main_frame_->AddJsResultForFunctionCall(
        dom_response_value_.get(),
        "education_page_detector.extractDOMFeatures");
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<ScopedEducationCategoriesForTesting> scoped_categories_;
  web::FakeBrowserState fake_browser_state_;
  web::FakeWebState fake_web_state_;
  raw_ptr<web::FakeWebFrame> main_frame_ = nullptr;
  std::unique_ptr<base::Value> dom_response_value_;
  FakeOptimizationGuideDecider decider_;
  std::unique_ptr<OptimizationGuidePageClassificationService> service_;
};

// Tests that the service registers PAGE_ENTITIES optimization type on init.
TEST_F(OptimizationGuidePageClassificationServiceTest,
       TestRegistersOptimizationType) {
  ASSERT_EQ(1u, decider_.registered_types().size());
  EXPECT_EQ(optimization_guide::proto::PAGE_ENTITIES,
            decider_.registered_types()[0]);
}

// Tests that null WebState returns an empty result.
TEST_F(OptimizationGuidePageClassificationServiceTest, TestNullWebState) {
  base::test::TestFuture<const PageClassificationResult&> future;
  service_->ClassifyWebState(nullptr, future.GetCallback());

  const PageClassificationResult& result = future.Get();
  EXPECT_FALSE(result.IsEligibleForCategory(
      page_content_annotations::CategoryType::kEducation));
  EXPECT_TRUE(result.category_results.empty());
}

// Tests that ineligible URL (e.g. chrome://) returns an empty result.
TEST_F(OptimizationGuidePageClassificationServiceTest, TestIneligibleUrl) {
  fake_web_state_.SetCurrentURL(GURL("chrome://version"));

  base::test::TestFuture<const PageClassificationResult&> future;
  service_->ClassifyWebState(&fake_web_state_, future.GetCallback());

  const PageClassificationResult& result = future.Get();
  EXPECT_FALSE(result.IsEligibleForCategory(
      page_content_annotations::CategoryType::kEducation));
  EXPECT_TRUE(result.category_results.empty());
}

// Tests that OptimizationGuide decision kFalse returns an empty result.
TEST_F(OptimizationGuidePageClassificationServiceTest,
       TestOptimizationGuideDecisionFalse) {
  fake_web_state_.SetCurrentURL(
      GURL("https://en.wikipedia.org/wiki/Quantum_mechanics"));
  decider_.SetResponse(optimization_guide::OptimizationGuideDecision::kFalse,
                       optimization_guide::OptimizationMetadata());

  base::test::TestFuture<const PageClassificationResult&> future;
  service_->ClassifyWebState(&fake_web_state_, future.GetCallback());

  const PageClassificationResult& result = future.Get();
  EXPECT_FALSE(result.IsEligibleForCategory(
      page_content_annotations::CategoryType::kEducation));
  EXPECT_TRUE(result.category_results.empty());
}

// Tests that non-educational category is rejected.
TEST_F(OptimizationGuidePageClassificationServiceTest,
       TestNonEducationalCategory) {
  fake_web_state_.SetCurrentURL(GURL("https://example.com/shopping"));
  SetOptimizationGuideEducationResponse("/Shopping/Apparel", 0.95f);

  base::test::TestFuture<const PageClassificationResult&> future;
  service_->ClassifyWebState(&fake_web_state_, future.GetCallback());

  const PageClassificationResult& result = future.Get();
  EXPECT_FALSE(result.IsEligibleForCategory(
      page_content_annotations::CategoryType::kEducation));
  EXPECT_TRUE(result.category_results.empty());
}

// Tests that successful classification and DOM extraction returns eligible.
TEST_F(OptimizationGuidePageClassificationServiceTest,
       TestSuccessfulClassificationAndScoreFusion) {
  fake_web_state_.SetCurrentURL(
      GURL("https://en.wikipedia.org/wiki/Quantum_mechanics"));
  SetOptimizationGuideEducationResponse("/Science/Physics", 0.85f);
  SetDOMFeaturesResponse(1000, 5);

  base::test::TestFuture<const PageClassificationResult&> future;
  service_->ClassifyWebState(&fake_web_state_, future.GetCallback());

  const PageClassificationResult& result = future.Get();
  EXPECT_TRUE(result.IsEligibleForCategory(
      page_content_annotations::CategoryType::kEducation));
  auto edu_result = result.GetResultForCategory(
      page_content_annotations::CategoryType::kEducation);
  ASSERT_TRUE(edu_result.has_value());
  EXPECT_TRUE(edu_result->is_eligible);
  EXPECT_FLOAT_EQ(0.85f, edu_result->score);
}

// Tests that insufficient DOM features reject eligibility.
TEST_F(OptimizationGuidePageClassificationServiceTest,
       TestInsufficientDOMFeaturesRejects) {
  fake_web_state_.SetCurrentURL(
      GURL("https://en.wikipedia.org/wiki/Quantum_mechanics"));
  SetOptimizationGuideEducationResponse("/Science/Physics", 0.85f);
  SetDOMFeaturesResponse(200, 1);  // Less than 250 words minimum

  base::test::TestFuture<const PageClassificationResult&> future;
  service_->ClassifyWebState(&fake_web_state_, future.GetCallback());

  const PageClassificationResult& result = future.Get();
  EXPECT_FALSE(result.IsEligibleForCategory(
      page_content_annotations::CategoryType::kEducation));
  auto edu_result = result.GetResultForCategory(
      page_content_annotations::CategoryType::kEducation);
  ASSERT_TRUE(edu_result.has_value());
  EXPECT_FALSE(edu_result->is_eligible);
  EXPECT_FLOAT_EQ(0.0f, edu_result->score);
}

// Tests that CancelClassification cancels active request.
TEST_F(OptimizationGuidePageClassificationServiceTest,
       TestCancelClassification) {
  fake_web_state_.SetCurrentURL(
      GURL("https://en.wikipedia.org/wiki/Quantum_mechanics"));
  service_->CancelClassification(&fake_web_state_);
  // Safe to call multiple times without crashing.
  service_->CancelClassification(&fake_web_state_);
}
