// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_suggestion_handler.h"

#import <optional>
#import <string>
#import <vector>

#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_tab_helper.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "mojo/public/cpp/bindings/remote.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

class GeminiSuggestionHandlerTest : public PlatformTest {
 protected:
  GeminiSuggestionHandlerTest() {
    scoped_feature_list_.InitWithFeatures(
        {kPageActionMenu, kZeroStateSuggestions}, {});
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    handler_ = [[GeminiSuggestionHandler alloc]
        initWithWebStateList:browser_->GetWebStateList()];
  }

  // Helper to set cached suggestions in BwgTabHelper using the friend class
  // access.
  void SetSuggestions(BwgTabHelper* tab_helper,
                      const std::vector<std::string>& suggestions) {
    ASSERT_TRUE(tab_helper->zero_state_suggestions_);
    tab_helper->zero_state_suggestions_->can_apply = true;
    tab_helper->zero_state_suggestions_->suggestions = suggestions;
  }

  // Helper to set current URL in BwgTabHelper using the friend class access.
  void SetCurrentUrl(BwgTabHelper* tab_helper, const GURL& url) {
    tab_helper->current_url_ = url;
  }

  // Helper to create a fake web state, optionally attach BwgTabHelper, and
  // insert it into the web state list.
  BwgTabHelper* CreateAndInsertWebState(bool attach_tab_helper,
                                        const GURL& url = GURL()) {
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetBrowserState(profile_.get());
    if (!url.is_empty()) {
      web_state->SetCurrentURL(url);
    }

    BwgTabHelper* tab_helper = nullptr;
    if (attach_tab_helper) {
      BwgTabHelper::CreateForWebState(web_state.get());
      tab_helper = BwgTabHelper::FromWebState(web_state.get());
      if (!url.is_empty()) {
        SetCurrentUrl(tab_helper, url);
      }
    }

    browser_->GetWebStateList()->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate(true));

    return tab_helper;
  }

  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  GeminiSuggestionHandler* handler_;
};

// Tests that fetchZeroStateSuggestions calls completion with nil when there is
// no active web state.
TEST_F(GeminiSuggestionHandlerTest,
       FetchZeroStateSuggestions_NoActiveWebState) {
  __block BOOL completion_called = NO;
  __block NSArray<NSString*>* returned_suggestions = nil;

  [handler_ fetchZeroStateSuggestions:^(NSArray<NSString*>* suggestions) {
    completion_called = YES;
    returned_suggestions = suggestions;
  }];

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool() {
        return completion_called;
      }));
  EXPECT_EQ(nil, returned_suggestions);
}

// Tests that fetchZeroStateSuggestions calls completion with nil when the
// active web state has no BwgTabHelper.
TEST_F(GeminiSuggestionHandlerTest, FetchZeroStateSuggestions_NoTabHelper) {
  CreateAndInsertWebState(false);

  __block BOOL completion_called = NO;
  __block NSArray<NSString*>* returned_suggestions = nil;

  [handler_ fetchZeroStateSuggestions:^(NSArray<NSString*>* suggestions) {
    completion_called = YES;
    returned_suggestions = suggestions;
  }];

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool() {
        return completion_called;
      }));
  EXPECT_EQ(nil, returned_suggestions);
}

// Tests that fetchZeroStateSuggestions calls completion with suggestions when
// available.
TEST_F(GeminiSuggestionHandlerTest, FetchZeroStateSuggestions_Success) {
  GURL url("https://www.example.com");
  BwgTabHelper* tab_helper = CreateAndInsertWebState(true, url);

  // Set up the expected suggestions using helper method.
  SetSuggestions(tab_helper, {"suggestion1", "suggestion2"});

  __block BOOL completion_called = NO;
  __block NSArray<NSString*>* returned_suggestions = nil;

  // Call the handler and verify that the completion block receives the
  // suggestions.
  [handler_ fetchZeroStateSuggestions:^(NSArray<NSString*>* suggestions) {
    completion_called = YES;
    returned_suggestions = suggestions;
  }];

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool() {
        return completion_called;
      }));
  ASSERT_NE(nil, returned_suggestions);
  EXPECT_EQ(2u, returned_suggestions.count);
  EXPECT_NSEQ(@"suggestion1", returned_suggestions[0]);
  EXPECT_NSEQ(@"suggestion2", returned_suggestions[1]);
}

// Tests that fetchZeroStateSuggestions calls completion with empty array when
// suggestions are empty.
TEST_F(GeminiSuggestionHandlerTest, FetchZeroStateSuggestions_Empty) {
  GURL url("https://www.example.com");
  BwgTabHelper* tab_helper = CreateAndInsertWebState(true, url);

  // Set up empty suggestions using helper methods.
  SetSuggestions(tab_helper, std::vector<std::string>());

  __block BOOL completion_called = NO;
  __block NSArray<NSString*>* returned_suggestions = nil;

  // Call the handler and verify that the completion block receives an empty
  // array.
  [handler_ fetchZeroStateSuggestions:^(NSArray<NSString*>* suggestions) {
    completion_called = YES;
    returned_suggestions = suggestions;
  }];

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool() {
        return completion_called;
      }));
  ASSERT_NE(nil, returned_suggestions);
  EXPECT_EQ(0u, returned_suggestions.count);
}
