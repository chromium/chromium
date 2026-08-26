// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/on_device_category_classifier/on_device_page_classification_service.h"

#import <memory>
#import <vector>

#import "base/functional/bind.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/test_future.h"
#import "components/optimization_guide/core/delivery/test_optimization_guide_model_provider.h"
#import "components/page_content_annotations/core/page_content_annotations_common.h"
#import "components/passage_embeddings/core/passage_embeddings_types.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/on_device_category_classifier/in_process_category_classification_service.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace {

std::unique_ptr<KeyedService> BuildTestInProcessClassificationService(
    ProfileIOS* profile) {
  static base::NoDestructor<
      optimization_guide::TestOptimizationGuideModelProvider>
      test_model_provider;
  return std::make_unique<InProcessCategoryClassificationService>(
      test_model_provider.get());
}

class OnDevicePageClassificationServiceTest : public PlatformTest {
 public:
  OnDevicePageClassificationServiceTest() = default;
  ~OnDevicePageClassificationServiceTest() override = default;

  void SetUp() override {
    PlatformTest::SetUp();

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        InProcessCategoryClassificationService::GetFactory(),
        base::BindRepeating(&BuildTestInProcessClassificationService));
    profile_ = std::move(builder).Build();

    in_process_service_ =
        InProcessCategoryClassificationService::GetForProfile(profile_.get());
    service_ = std::make_unique<OnDevicePageClassificationService>(
        in_process_service_);
  }

  void TearDown() override {
    service_.reset();
    in_process_service_ = nullptr;
    profile_.reset();
    PlatformTest::TearDown();
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<InProcessCategoryClassificationService> in_process_service_ = nullptr;
  std::unique_ptr<OnDevicePageClassificationService> service_;
};

// Tests that ClassifyWebState returns early for null WebState.
TEST_F(OnDevicePageClassificationServiceTest, ClassifyWebStateNullWebState) {
  base::test::TestFuture<
      const std::optional<std::vector<page_content_annotations::Category>>&>
      future;
  service_->ClassifyWebState(nullptr, future.GetCallback());

  EXPECT_FALSE(future.Get().has_value());
}

// Tests that ClassifyWebState returns early for off-the-record profile.
TEST_F(OnDevicePageClassificationServiceTest, ClassifyWebStateOffTheRecord) {
  ProfileIOS* otr_profile =
      profile_->CreateOffTheRecordProfileWithTestingFactories();
  auto otr_web_state = std::make_unique<web::FakeWebState>();
  otr_web_state->SetBrowserState(otr_profile);
  otr_web_state->SetCurrentURL(GURL("https://example.com"));

  base::test::TestFuture<
      const std::optional<std::vector<page_content_annotations::Category>>&>
      future;
  service_->ClassifyWebState(otr_web_state.get(), future.GetCallback());

  EXPECT_FALSE(future.Get().has_value());
}

// Tests that ClassifyWebState returns early for non-HTTP/HTTPS URLs.
TEST_F(OnDevicePageClassificationServiceTest, ClassifyWebStateNonHttpUrl) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetBrowserState(profile_.get());
  web_state->SetCurrentURL(GURL("chrome://version"));

  base::test::TestFuture<
      const std::optional<std::vector<page_content_annotations::Category>>&>
      future;
  service_->ClassifyWebState(web_state.get(), future.GetCallback());

  EXPECT_FALSE(future.Get().has_value());
}

// Tests that CancelClassification safely handles null and active WebStates.
TEST_F(OnDevicePageClassificationServiceTest, CancelClassification) {
  service_->CancelClassification(nullptr);

  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetBrowserState(profile_.get());
  web_state->SetCurrentURL(GURL("https://example.com"));

  service_->CancelClassification(web_state.get());
}

// Tests that ClassifyWebState in Title & URL mode uses title and URL directly.
TEST_F(OnDevicePageClassificationServiceTest,
       ClassifyWebStateTitleAndUrlOnlyMode) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{kPageActionMenu, {}},
       {kGeminiContextualSuggestionsCues,
        {{kGeminiContextualSuggestionsCuesTitleAndUrlOnlyParam, "true"}}}},
      {});

  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetBrowserState(profile_.get());
  web_state->SetCurrentURL(GURL("https://example.com"));
  web_state->SetTitle(u"Example Title");

  base::test::TestFuture<
      const std::optional<std::vector<page_content_annotations::Category>>&>
      future;
  service_->ClassifyWebState(web_state.get(), future.GetCallback());

  EXPECT_FALSE(future.IsReady());

  in_process_service_->OnPassageEmbedderLoadedForTesting(1, 768,
                                                         /*success=*/true);

  EXPECT_TRUE(future.Get().has_value());
}

// Tests that ClassifyWebState reuses cached embeddings when available.
TEST_F(OnDevicePageClassificationServiceTest,
       ClassifyWebStateWithCachedEmbeddings) {
  const GURL url("https://example.com");
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetBrowserState(profile_.get());
  web_state->SetCurrentURL(url);

  std::vector<float> title_vec(768, 0.0f);
  title_vec[0] = 1.0f;
  InProcessCategoryClassificationService::CachedEmbeddings cached_embeddings{
      .title_url_embedding =
          passage_embeddings::Embedding(std::move(title_vec)),
  };
  in_process_service_->SetCachedEmbeddingsForTesting(
      url, std::move(cached_embeddings));

  base::test::TestFuture<
      const std::optional<std::vector<page_content_annotations::Category>>&>
      future;
  service_->ClassifyWebState(web_state.get(), future.GetCallback());

  EXPECT_TRUE(future.Get().has_value());
}

}  // namespace
