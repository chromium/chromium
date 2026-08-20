// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/contextual_cueing/contextual_cueing_tab_helper.h"

#import <memory>
#import <vector>

#import "base/functional/bind.h"
#import "base/test/scoped_feature_list.h"
#import "components/page_content_annotations/core/page_content_annotation_type.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace contextual_cueing {

namespace {

class TestCueingObserver : public ContextualCueingTabHelper::Observer {
 public:
  void OnPageClassificationCompleted(
      web::WebState* web_state,
      const std::optional<std::vector<page_content_annotations::Category>>&
          categories,
      size_t word_count) override {
    notified_web_state_ = web_state;
    notified_categories_ = categories;
    notified_word_count_ = word_count;
    call_count_++;
  }

  raw_ptr<web::WebState> notified_web_state_ = nullptr;
  std::optional<std::vector<page_content_annotations::Category>>
      notified_categories_;
  size_t notified_word_count_ = 0;
  int call_count_ = 0;
};

}  // namespace

class ContextualCueingTabHelperTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kGeminiContextualSuggestionsCues,
        {{kGeminiContextualSuggestionsCuesOnDeviceClassifierParam, "true"}});

    TestProfileIOS::Builder builder;
    profile_ = std::move(builder).Build();

    web_state_ = std::make_unique<web::FakeWebState>();
    web_state_->SetBrowserState(profile_.get());
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::FakeWebState> web_state_;
};

// Tests that the tab helper can be created and retrieved.
TEST_F(ContextualCueingTabHelperTest, CreateAndRetrieve) {
  EXPECT_EQ(ContextualCueingTabHelper::FromWebState(web_state_.get()), nullptr);

  ContextualCueingTabHelper::CreateForWebState(web_state_.get());

  EXPECT_NE(ContextualCueingTabHelper::FromWebState(web_state_.get()), nullptr);
}

// Tests that classification is ignored for off-the-record profiles.
TEST_F(ContextualCueingTabHelperTest, IgnoresOffTheRecordProfile) {
  ProfileIOS* otr_profile =
      profile_->CreateOffTheRecordProfileWithTestingFactories();
  auto otr_web_state = std::make_unique<web::FakeWebState>();
  otr_web_state->SetBrowserState(otr_profile);
  otr_web_state->SetCurrentURL(GURL("https://example.com"));

  ContextualCueingTabHelper::CreateForWebState(otr_web_state.get());
  auto* tab_helper =
      ContextualCueingTabHelper::FromWebState(otr_web_state.get());

  tab_helper->PageLoaded(otr_web_state.get(),
                         web::PageLoadCompletionStatus::SUCCESS);

  EXPECT_FALSE(tab_helper->GetCategories().has_value());
  EXPECT_EQ(tab_helper->GetExtractedWordCount(), 0u);
}

// Tests that classification is ignored for non-HTTP/HTTPS URLs.
TEST_F(ContextualCueingTabHelperTest, IgnoresNonHttpUrls) {
  web_state_->SetCurrentURL(GURL("chrome://version"));

  ContextualCueingTabHelper::CreateForWebState(web_state_.get());
  auto* tab_helper = ContextualCueingTabHelper::FromWebState(web_state_.get());

  tab_helper->PageLoaded(web_state_.get(),
                         web::PageLoadCompletionStatus::SUCCESS);

  EXPECT_FALSE(tab_helper->GetCategories().has_value());
  EXPECT_EQ(tab_helper->GetExtractedWordCount(), 0u);
}

// Tests that navigation resets classification state.
TEST_F(ContextualCueingTabHelperTest, NavigationResetsState) {
  web_state_->SetCurrentURL(GURL("https://example.com/page1"));
  ContextualCueingTabHelper::CreateForWebState(web_state_.get());
  auto* tab_helper = ContextualCueingTabHelper::FromWebState(web_state_.get());

  TestCueingObserver observer;
  tab_helper->AddObserver(&observer);

  web::FakeNavigationContext nav_context;
  nav_context.SetUrl(GURL("https://example.com/page2"));
  nav_context.SetHasCommitted(true);
  nav_context.SetIsSameDocument(false);
  tab_helper->DidFinishNavigation(web_state_.get(), &nav_context);

  EXPECT_FALSE(tab_helper->GetCategories().has_value());
  EXPECT_EQ(tab_helper->GetExtractedWordCount(), 0u);

  tab_helper->RemoveObserver(&observer);
}

// Tests that hiding a tab resets callbacks.
TEST_F(ContextualCueingTabHelperTest, WasHiddenCancelsClassification) {
  web_state_->SetCurrentURL(GURL("https://example.com"));
  ContextualCueingTabHelper::CreateForWebState(web_state_.get());
  auto* tab_helper = ContextualCueingTabHelper::FromWebState(web_state_.get());

  tab_helper->WasHidden(web_state_.get());
  EXPECT_FALSE(tab_helper->GetCategories().has_value());
  EXPECT_EQ(tab_helper->GetExtractedWordCount(), 0u);
}

}  // namespace contextual_cueing
