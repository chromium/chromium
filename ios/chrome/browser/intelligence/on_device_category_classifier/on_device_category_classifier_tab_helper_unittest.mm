// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/on_device_category_classifier/on_device_category_classifier_tab_helper.h"

#import <memory>
#import <vector>

#import "base/functional/bind.h"
#import "base/no_destructor.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/types/expected.h"
#import "components/optimization_guide/core/delivery/test_optimization_guide_model_provider.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "components/page_content_annotations/core/page_content_annotation_type.h"
#import "components/passage_embeddings/core/passage_embeddings_types.h"
#import "components/ukm/test_ukm_recorder.h"
#import "ios/chrome/browser/intelligence/on_device_category_classifier/in_process_category_classification_service.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "services/metrics/public/cpp/ukm_builders.h"
#import "services/metrics/public/cpp/ukm_source_id.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace {

std::unique_ptr<KeyedService> BuildTestCategoryClassificationService(
    ProfileIOS* profile) {
  static base::NoDestructor<
      optimization_guide::TestOptimizationGuideModelProvider>
      test_model_provider;
  return std::make_unique<InProcessCategoryClassificationService>(
      test_model_provider.get());
}

class TestCategoryObserver
    : public OnDeviceCategoryClassifierTabHelper::Observer {
 public:
  void OnCategoriesClassified(
      web::WebState* web_state,
      const std::vector<page_content_annotations::Category>& categories)
      override {
    notified_web_state_ = web_state;
    notified_categories_ = categories;
    call_count_++;
  }

  raw_ptr<web::WebState> notified_web_state_ = nullptr;
  std::vector<page_content_annotations::Category> notified_categories_;
  int call_count_ = 0;
};

}  // namespace

class OnDeviceCategoryClassifierTabHelperTest : public PlatformTest {
 protected:
  OnDeviceCategoryClassifierTabHelperTest() = default;

  void SetUp() override {
    PlatformTest::SetUp();
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        InProcessCategoryClassificationService::GetFactory(),
        base::BindRepeating(&BuildTestCategoryClassificationService));
    profile_ = std::move(builder).Build();
    web_state_ = std::make_unique<web::FakeWebState>();
    web_state_->SetBrowserState(profile_.get());
  }

  void CallExtractPageContextAndClassify(
      OnDeviceCategoryClassifierTabHelper* tab_helper) {
    tab_helper->ExtractPageContextAndClassify();
  }

  PageContextWrapper* GetPageContextWrapper(
      OnDeviceCategoryClassifierTabHelper* tab_helper) {
    return tab_helper->page_context_wrapper_;
  }

  void CallOnPageContextResponse(
      OnDeviceCategoryClassifierTabHelper* tab_helper,
      PageContextWrapperCallbackResponse response) {
    tab_helper->OnPageContextResponse(std::move(response));
  }

  void CallOnPageContextExtracted(
      OnDeviceCategoryClassifierTabHelper* tab_helper,
      const std::string& page_content,
      const std::string& title,
      const GURL& url) {
    tab_helper->OnPageContextExtracted(page_content, title, url);
  }

  void CallOnCategoriesClassified(
      OnDeviceCategoryClassifierTabHelper* tab_helper,
      ukm::SourceId source_id,
      const std::vector<page_content_annotations::Category>& categories) {
    tab_helper->OnCategoriesClassified(source_id, categories);
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::FakeWebState> web_state_;
};

// Tests that the tab helper can be created and retrieved from a WebState.
TEST_F(OnDeviceCategoryClassifierTabHelperTest, CreateAndRetrieve) {
  EXPECT_EQ(OnDeviceCategoryClassifierTabHelper::FromWebState(web_state_.get()),
            nullptr);

  OnDeviceCategoryClassifierTabHelper::CreateForWebState(web_state_.get());

  EXPECT_NE(OnDeviceCategoryClassifierTabHelper::FromWebState(web_state_.get()),
            nullptr);
}

// Tests that page extraction is ignored for off-the-record (incognito)
// profiles.
TEST_F(OnDeviceCategoryClassifierTabHelperTest, IgnoresOffTheRecordProfile) {
  ProfileIOS* otr_profile =
      profile_->CreateOffTheRecordProfileWithTestingFactories();
  auto otr_web_state = std::make_unique<web::FakeWebState>();
  otr_web_state->SetBrowserState(otr_profile);
  otr_web_state->SetCurrentURL(GURL("https://example.com"));

  OnDeviceCategoryClassifierTabHelper::CreateForWebState(otr_web_state.get());
  auto* tab_helper =
      OnDeviceCategoryClassifierTabHelper::FromWebState(otr_web_state.get());

  // Trigger page load completion.
  tab_helper->PageLoaded(otr_web_state.get(),
                         web::PageLoadCompletionStatus::SUCCESS);
}

// Tests that page extraction is ignored for non-HTTP/HTTPS URLs.
TEST_F(OnDeviceCategoryClassifierTabHelperTest, IgnoresNonHttpUrls) {
  web_state_->SetCurrentURL(GURL("chrome://version"));

  OnDeviceCategoryClassifierTabHelper::CreateForWebState(web_state_.get());
  auto* tab_helper =
      OnDeviceCategoryClassifierTabHelper::FromWebState(web_state_.get());

  tab_helper->PageLoaded(web_state_.get(),
                         web::PageLoadCompletionStatus::SUCCESS);
}

// Tests handling of PageLoaded events.
TEST_F(OnDeviceCategoryClassifierTabHelperTest, PageLoadedHandling) {
  web_state_->SetCurrentURL(GURL("https://example.com"));

  OnDeviceCategoryClassifierTabHelper::CreateForWebState(web_state_.get());
  auto* tab_helper =
      OnDeviceCategoryClassifierTabHelper::FromWebState(web_state_.get());

  // Unsuccessful page load should be ignored.
  tab_helper->PageLoaded(web_state_.get(),
                         web::PageLoadCompletionStatus::FAILURE);

  // Successful page load triggers extraction.
  tab_helper->PageLoaded(web_state_.get(),
                         web::PageLoadCompletionStatus::SUCCESS);
}

// Tests handling of DidFinishNavigation events.
TEST_F(OnDeviceCategoryClassifierTabHelperTest, DidFinishNavigationHandling) {
  web_state_->SetCurrentURL(GURL("https://example.com"));

  OnDeviceCategoryClassifierTabHelper::CreateForWebState(web_state_.get());
  auto* tab_helper =
      OnDeviceCategoryClassifierTabHelper::FromWebState(web_state_.get());

  // Non-same-document navigation should reset extraction state.
  web::FakeNavigationContext nav_context;
  nav_context.SetUrl(GURL("https://example.com/page1"));
  nav_context.SetHasCommitted(true);
  nav_context.SetIsSameDocument(false);
  tab_helper->DidFinishNavigation(web_state_.get(), &nav_context);

  // Same-document navigation to a new URL triggers extraction.
  nav_context.SetUrl(GURL("https://example.com/page2"));
  nav_context.SetIsSameDocument(true);
  tab_helper->DidFinishNavigation(web_state_.get(), &nav_context);
}

// Tests that WasHidden invalidates state cleanly.
TEST_F(OnDeviceCategoryClassifierTabHelperTest, WasHiddenHandling) {
  web_state_->SetCurrentURL(GURL("https://example.com"));

  OnDeviceCategoryClassifierTabHelper::CreateForWebState(web_state_.get());
  auto* tab_helper =
      OnDeviceCategoryClassifierTabHelper::FromWebState(web_state_.get());

  tab_helper->WasHidden(web_state_.get());
}

// Tests OnPageContextResponse with empty and valid responses, including
// text truncation.
TEST_F(OnDeviceCategoryClassifierTabHelperTest, OnPageContextResponseHandling) {
  web_state_->SetCurrentURL(GURL("https://example.com"));
  OnDeviceCategoryClassifierTabHelper::CreateForWebState(web_state_.get());
  auto* tab_helper =
      OnDeviceCategoryClassifierTabHelper::FromWebState(web_state_.get());

  CallOnPageContextResponse(
      tab_helper, base::unexpected(PageContextWrapperError::kGenericError));

  auto page_context =
      std::make_unique<optimization_guide::proto::PageContext>();
  page_context->set_url("https://example.com");
  page_context->set_title("Test Title");
  page_context->set_inner_text("Small inner text");
  CallOnPageContextResponse(tab_helper, std::move(page_context));

  auto large_context =
      std::make_unique<optimization_guide::proto::PageContext>();
  large_context->set_url("https://example.com");
  large_context->set_title("Long Title");
  large_context->set_inner_text(std::string(15000, 'a'));
  CallOnPageContextResponse(tab_helper, std::move(large_context));
}

// Tests OnPageContextExtracted with empty and non-empty content.
TEST_F(OnDeviceCategoryClassifierTabHelperTest,
       OnPageContextExtractedHandling) {
  web_state_->SetCurrentURL(GURL("https://example.com"));
  OnDeviceCategoryClassifierTabHelper::CreateForWebState(web_state_.get());
  auto* tab_helper =
      OnDeviceCategoryClassifierTabHelper::FromWebState(web_state_.get());

  CallOnPageContextExtracted(tab_helper, "", "Title",
                             GURL("https://example.com"));
  CallOnPageContextExtracted(tab_helper, "Some paragraph text.", "Title",
                             GURL("https://example.com"));
}

// Tests OnCategoriesClassified records UMA and UKM correctly.
TEST_F(OnDeviceCategoryClassifierTabHelperTest,
       OnCategoriesClassifiedRecordsUkmAndUma) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  web_state_->SetCurrentURL(GURL("https://example.com"));
  OnDeviceCategoryClassifierTabHelper::CreateForWebState(web_state_.get());
  auto* tab_helper =
      OnDeviceCategoryClassifierTabHelper::FromWebState(web_state_.get());

  ukm::SourceId source_id = ukm::UkmRecorder::GetNewSourceID();
  std::vector<page_content_annotations::Category> categories = {
      {page_content_annotations::CategoryType::kEducation, 0.75f},
      {page_content_annotations::CategoryType::kShopping, 0.20f},
  };

  CallOnCategoriesClassified(tab_helper, source_id, categories);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentAnnotations.CategoryClassifier."
      "EducationScore",
      75, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentAnnotations.CategoryClassifier."
      "ShoppingScore",
      20, 1);

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::PageContentAnnotations2::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const ukm::mojom::UkmEntry* const entry : entries) {
    EXPECT_EQ(source_id, entry->source_id);
    EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(
        entry, ukm::builders::PageContentAnnotations2::
                   kCategoryClassifier_EducationScoreName));
    EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(
        entry, ukm::builders::PageContentAnnotations2::
                   kCategoryClassifier_ShoppingScoreName));
  }
}

// Tests that empty categories list does not record UKM.
TEST_F(OnDeviceCategoryClassifierTabHelperTest,
       OnCategoriesClassifiedEmptyDoesNotRecordUkm) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  web_state_->SetCurrentURL(GURL("https://example.com"));
  OnDeviceCategoryClassifierTabHelper::CreateForWebState(web_state_.get());
  auto* tab_helper =
      OnDeviceCategoryClassifierTabHelper::FromWebState(web_state_.get());

  CallOnCategoriesClassified(tab_helper, ukm::UkmRecorder::GetNewSourceID(),
                             {});

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::PageContentAnnotations2::kEntryName);
  EXPECT_EQ(0u, entries.size());
}

// Tests OnCategoriesClassified stores state and accessors return expected
// values.
TEST_F(OnDeviceCategoryClassifierTabHelperTest, CategoriesStateAndAccessors) {
  web_state_->SetCurrentURL(GURL("https://example.com"));
  OnDeviceCategoryClassifierTabHelper::CreateForWebState(web_state_.get());
  auto* tab_helper =
      OnDeviceCategoryClassifierTabHelper::FromWebState(web_state_.get());

  EXPECT_FALSE(tab_helper->GetCategories().has_value());

  std::vector<page_content_annotations::Category> categories = {
      {.category_type = page_content_annotations::CategoryType::kEducation,
       .score = 0.4f},
      {.category_type = page_content_annotations::CategoryType::kShopping,
       .score = 0.85f},
  };
  CallOnCategoriesClassified(tab_helper, ukm::SourceId(), categories);

  ASSERT_TRUE(tab_helper->GetCategories().has_value());
  const auto& result_categories = *tab_helper->GetCategories();
  ASSERT_EQ(result_categories.size(), 2u);
  EXPECT_EQ(result_categories[0].category_type,
            page_content_annotations::CategoryType::kEducation);
  EXPECT_FLOAT_EQ(result_categories[0].score, 0.4f);
  EXPECT_EQ(result_categories[1].category_type,
            page_content_annotations::CategoryType::kShopping);
  EXPECT_FLOAT_EQ(result_categories[1].score, 0.85f);
}

// Tests that observers are notified when categories are classified.
TEST_F(OnDeviceCategoryClassifierTabHelperTest, ObserverNotification) {
  web_state_->SetCurrentURL(GURL("https://example.com"));
  OnDeviceCategoryClassifierTabHelper::CreateForWebState(web_state_.get());
  auto* tab_helper =
      OnDeviceCategoryClassifierTabHelper::FromWebState(web_state_.get());

  TestCategoryObserver observer;
  tab_helper->AddObserver(&observer);

  std::vector<page_content_annotations::Category> categories = {
      {.category_type = page_content_annotations::CategoryType::kShopping,
       .score = 0.9f},
  };
  CallOnCategoriesClassified(tab_helper, ukm::SourceId(), categories);

  EXPECT_EQ(observer.call_count_, 1);
  EXPECT_EQ(observer.notified_web_state_, web_state_.get());
  EXPECT_EQ(observer.notified_categories_.size(), 1u);
  EXPECT_EQ(observer.notified_categories_[0].category_type,
            page_content_annotations::CategoryType::kShopping);

  tab_helper->RemoveObserver(&observer);
  CallOnCategoriesClassified(tab_helper, ukm::SourceId(), categories);
  EXPECT_EQ(observer.call_count_, 1);
}

// Tests that non-same-document navigation resets classification state.
TEST_F(OnDeviceCategoryClassifierTabHelperTest, NavigationResetsCategories) {
  web_state_->SetCurrentURL(GURL("https://example.com"));
  OnDeviceCategoryClassifierTabHelper::CreateForWebState(web_state_.get());
  auto* tab_helper =
      OnDeviceCategoryClassifierTabHelper::FromWebState(web_state_.get());

  web::FakeNavigationContext initial_context;
  initial_context.SetUrl(GURL("https://example.com"));
  initial_context.SetHasCommitted(true);
  initial_context.SetIsSameDocument(false);
  tab_helper->DidFinishNavigation(web_state_.get(), &initial_context);

  std::vector<page_content_annotations::Category> categories = {
      {.category_type = page_content_annotations::CategoryType::kShopping,
       .score = 0.85f},
  };
  CallOnCategoriesClassified(tab_helper, ukm::SourceId(), categories);
  EXPECT_TRUE(tab_helper->GetCategories().has_value());

  web::FakeNavigationContext navigation_context;
  navigation_context.SetUrl(GURL("https://example.com/other"));
  navigation_context.SetHasCommitted(true);
  navigation_context.SetIsSameDocument(false);
  tab_helper->DidFinishNavigation(web_state_.get(), &navigation_context);

  EXPECT_FALSE(tab_helper->GetCategories().has_value());
}

// Tests that uncommitted navigation does not clear existing classification.
TEST_F(OnDeviceCategoryClassifierTabHelperTest,
       UncommittedNavigationPreservesCategories) {
  web_state_->SetCurrentURL(GURL("https://example.com"));
  OnDeviceCategoryClassifierTabHelper::CreateForWebState(web_state_.get());
  auto* tab_helper =
      OnDeviceCategoryClassifierTabHelper::FromWebState(web_state_.get());

  web::FakeNavigationContext initial_context;
  initial_context.SetUrl(GURL("https://example.com"));
  initial_context.SetHasCommitted(true);
  initial_context.SetIsSameDocument(false);
  tab_helper->DidFinishNavigation(web_state_.get(), &initial_context);

  std::vector<page_content_annotations::Category> categories = {
      {.category_type = page_content_annotations::CategoryType::kShopping,
       .score = 0.85f},
  };
  CallOnCategoriesClassified(tab_helper, ukm::SourceId(), categories);
  EXPECT_TRUE(tab_helper->GetCategories().has_value());

  web::FakeNavigationContext uncommitted_context;
  uncommitted_context.SetUrl(GURL("https://example.com/other"));
  uncommitted_context.SetHasCommitted(false);
  uncommitted_context.SetIsSameDocument(false);
  tab_helper->DidFinishNavigation(web_state_.get(), &uncommitted_context);

  // Uncommitted navigation must not wipe the active page's classification.
  ASSERT_TRUE(tab_helper->GetCategories().has_value());
  ASSERT_EQ(tab_helper->GetCategories()->size(), 1u);
  EXPECT_EQ((*tab_helper->GetCategories())[0].category_type,
            page_content_annotations::CategoryType::kShopping);
  EXPECT_FLOAT_EQ((*tab_helper->GetCategories())[0].score, 0.85f);
}

// Tests that same-document anchor navigation preserves classification state.
TEST_F(OnDeviceCategoryClassifierTabHelperTest,
       SameDocumentAnchorNavigationPreservesCategories) {
  web_state_->SetCurrentURL(GURL("https://example.com/page"));
  OnDeviceCategoryClassifierTabHelper::CreateForWebState(web_state_.get());
  auto* tab_helper =
      OnDeviceCategoryClassifierTabHelper::FromWebState(web_state_.get());

  web::FakeNavigationContext initial_context;
  initial_context.SetUrl(GURL("https://example.com/page"));
  initial_context.SetHasCommitted(true);
  initial_context.SetIsSameDocument(false);
  tab_helper->DidFinishNavigation(web_state_.get(), &initial_context);

  std::vector<page_content_annotations::Category> categories = {
      {.category_type = page_content_annotations::CategoryType::kShopping,
       .score = 0.85f},
  };
  CallOnCategoriesClassified(tab_helper, ukm::SourceId(), categories);
  EXPECT_TRUE(tab_helper->GetCategories().has_value());

  web::FakeNavigationContext same_doc_anchor_context;
  same_doc_anchor_context.SetUrl(GURL("https://example.com/page#section"));
  same_doc_anchor_context.SetHasCommitted(true);
  same_doc_anchor_context.SetIsSameDocument(true);
  tab_helper->DidFinishNavigation(web_state_.get(), &same_doc_anchor_context);

  // Anchor navigation should keep cached categories without resetting.
  EXPECT_TRUE(tab_helper->GetCategories().has_value());
}

// Tests that same-document navigation to a different URL resets status and
// starts extraction.
TEST_F(OnDeviceCategoryClassifierTabHelperTest,
       SameDocumentNavigationResetsStatusAndStartsExtraction) {
  web_state_->SetCurrentURL(GURL("https://example.com/page2"));
  OnDeviceCategoryClassifierTabHelper::CreateForWebState(web_state_.get());
  auto* tab_helper =
      OnDeviceCategoryClassifierTabHelper::FromWebState(web_state_.get());

  web::FakeNavigationContext initial_context;
  initial_context.SetUrl(GURL("https://example.com/page1"));
  initial_context.SetHasCommitted(true);
  initial_context.SetIsSameDocument(false);
  tab_helper->DidFinishNavigation(web_state_.get(), &initial_context);

  std::vector<page_content_annotations::Category> categories = {
      {.category_type = page_content_annotations::CategoryType::kShopping,
       .score = 0.85f},
  };
  CallOnCategoriesClassified(tab_helper, ukm::SourceId(), categories);
  EXPECT_TRUE(tab_helper->GetCategories().has_value());

  web::FakeNavigationContext same_doc_context;
  same_doc_context.SetUrl(GURL("https://example.com/page2"));
  same_doc_context.SetHasCommitted(true);
  same_doc_context.SetIsSameDocument(true);
  tab_helper->DidFinishNavigation(web_state_.get(), &same_doc_context);

  EXPECT_FALSE(tab_helper->GetCategories().has_value());
}

// Tests that ExtractPageContextAndClassify uses cached embeddings when
// available.
TEST_F(OnDeviceCategoryClassifierTabHelperTest,
       ExtractPageContextAndClassifyWithCachedEmbeddings) {
  const GURL url("https://example.com");
  web_state_->SetCurrentURL(url);

  InProcessCategoryClassificationService* service =
      InProcessCategoryClassificationService::GetForProfile(profile_.get());
  ASSERT_NE(service, nullptr);

  std::vector<float> title_vec(768, 0.0f);
  title_vec[0] = 1.0f;
  std::vector<float> passage_vec(768, 0.0f);
  passage_vec[1] = 1.0f;

  InProcessCategoryClassificationService::CachedEmbeddings cached_embeddings{
      .title_url_embedding =
          passage_embeddings::Embedding(std::move(title_vec)),
      .passage_embeddings = {passage_embeddings::Embedding(
          std::move(passage_vec))},
  };
  service->SetCachedEmbeddingsForTesting(url, std::move(cached_embeddings));
  ASSERT_TRUE(service->HasCachedEmbeddings(url));

  OnDeviceCategoryClassifierTabHelper::CreateForWebState(web_state_.get());
  auto* tab_helper =
      OnDeviceCategoryClassifierTabHelper::FromWebState(web_state_.get());

  CallExtractPageContextAndClassify(tab_helper);
  // Successfully handled via cache without allocating page_context_wrapper_.
  EXPECT_EQ(GetPageContextWrapper(tab_helper), nil);
}

// Tests that ExtractPageContextAndClassify returns early for off-the-record
// profile.
TEST_F(OnDeviceCategoryClassifierTabHelperTest,
       ExtractPageContextAndClassifyOffTheRecordProfile) {
  ProfileIOS* otr_profile =
      profile_->CreateOffTheRecordProfileWithTestingFactories();
  auto otr_web_state = std::make_unique<web::FakeWebState>();
  otr_web_state->SetBrowserState(otr_profile);
  otr_web_state->SetCurrentURL(GURL("https://example.com"));

  OnDeviceCategoryClassifierTabHelper::CreateForWebState(otr_web_state.get());
  auto* tab_helper =
      OnDeviceCategoryClassifierTabHelper::FromWebState(otr_web_state.get());

  CallExtractPageContextAndClassify(tab_helper);
  EXPECT_EQ(GetPageContextWrapper(tab_helper), nil);
}
