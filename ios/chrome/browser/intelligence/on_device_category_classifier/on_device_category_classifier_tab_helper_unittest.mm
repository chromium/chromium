// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/on_device_category_classifier/on_device_category_classifier_tab_helper.h"

#import <memory>

#import "base/functional/bind.h"
#import "base/types/expected.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "services/metrics/public/cpp/ukm_source_id.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

class OnDeviceCategoryClassifierTabHelperTest : public PlatformTest {
 protected:
  OnDeviceCategoryClassifierTabHelperTest() = default;

  void SetUp() override {
    PlatformTest::SetUp();
    profile_ = TestProfileIOS::Builder().Build();
    web_state_ = std::make_unique<web::FakeWebState>();
    web_state_->SetBrowserState(profile_.get());
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
  nav_context.SetHasCommitted(true);
  nav_context.SetIsSameDocument(false);
  tab_helper->DidFinishNavigation(web_state_.get(), &nav_context);

  // Same-document navigation triggers extraction.
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

// Tests OnCategoriesClassified invocation.
TEST_F(OnDeviceCategoryClassifierTabHelperTest,
       OnCategoriesClassifiedHandling) {
  web_state_->SetCurrentURL(GURL("https://example.com"));
  OnDeviceCategoryClassifierTabHelper::CreateForWebState(web_state_.get());
  auto* tab_helper =
      OnDeviceCategoryClassifierTabHelper::FromWebState(web_state_.get());

  CallOnCategoriesClassified(tab_helper, ukm::SourceId(), {});
}
