// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/web_navigation_util.h"

#import "base/memory/raw_ptr.h"
#import "base/test/metrics/user_action_tester.h"
#import "components/search_engines/template_url.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/page_transition_types.h"
#import "url/gurl.h"

class WebNavigationUtilsTest : public PlatformTest {
 protected:
  WebNavigationUtilsTest() {
    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    navigation_manager_ = navigation_manager.get();
    web_state_.SetNavigationManager(std::move(navigation_manager));
  }

  web::FakeWebState web_state_;
  raw_ptr<web::FakeNavigationManager> navigation_manager_ = nullptr;
  base::UserActionTester user_action_tester_;
};

TEST_F(WebNavigationUtilsTest, CreateWebLoadParamsWithoutPost) {
  // No post params, check URL and transition.
  GURL url("http://test.test/");
  auto params = web_navigation_util::CreateWebLoadParams(
      url, ui::PageTransition::PAGE_TRANSITION_AUTO_BOOKMARK,
      /*post_data=*/nullptr);
  EXPECT_EQ(url, params.url);
  EXPECT_TRUE(PageTransitionCoreTypeIs(
      params.transition_type,
      ui::PageTransition::PAGE_TRANSITION_AUTO_BOOKMARK));
  // There should be no post data, and no extra headers.
  EXPECT_FALSE(params.post_data);
  EXPECT_FALSE(params.extra_headers);
}

TEST_F(WebNavigationUtilsTest, CreateWebLoadParamsWithPost) {
  // With post params.
  GURL url("http://test.test/");
  std::string post_data = "sphinx of black quartz judge my vow";
  TemplateURLRef::PostContent post_content("text/plain", post_data);
  auto params = web_navigation_util::CreateWebLoadParams(
      url, ui::PageTransition::PAGE_TRANSITION_FORM_SUBMIT,
      /*post_data=*/&post_content);
  EXPECT_EQ(url, params.url);
  EXPECT_TRUE(PageTransitionCoreTypeIs(
      params.transition_type, ui::PageTransition::PAGE_TRANSITION_FORM_SUBMIT));
  // Post data should be the same length as post_data
  EXPECT_EQ(post_data.length(), params.post_data.length);
  EXPECT_NSEQ(@"text/plain", params.extra_headers[@"Content-Type"]);
}

// Tests that GoBack updates the last committed item and log user action.
TEST_F(WebNavigationUtilsTest, GoBack) {
  GURL url1("http:/test1.test/");
  navigation_manager_->AddItem(url1, ui::PageTransition::PAGE_TRANSITION_LINK);
  GURL url2("http:/test2.test/");
  navigation_manager_->AddItem(url2, ui::PageTransition::PAGE_TRANSITION_LINK);
  GURL url3("http:/test3.test/");
  navigation_manager_->AddItem(url3, ui::PageTransition::PAGE_TRANSITION_LINK);
  EXPECT_EQ(2, navigation_manager_->GetLastCommittedItemIndex());
  EXPECT_EQ(0, user_action_tester_.GetActionCount("Back"));
  web_navigation_util::GoBack(&web_state_);
  EXPECT_EQ(1, navigation_manager_->GetLastCommittedItemIndex());
  EXPECT_EQ(1, user_action_tester_.GetActionCount("Back"));
}

// Tests that GoForward updates the last committed item and log user action.
TEST_F(WebNavigationUtilsTest, GoForward) {
  GURL url1("http:/test1.test/");
  navigation_manager_->AddItem(url1, ui::PageTransition::PAGE_TRANSITION_LINK);
  GURL url2("http:/test2.test/");
  navigation_manager_->AddItem(url2, ui::PageTransition::PAGE_TRANSITION_LINK);
  GURL url3("http:/test3.test/");
  navigation_manager_->AddItem(url3, ui::PageTransition::PAGE_TRANSITION_LINK);
  navigation_manager_->SetLastCommittedItemIndex(1);
  EXPECT_EQ(1, navigation_manager_->GetLastCommittedItemIndex());
  EXPECT_EQ(0, user_action_tester_.GetActionCount("Forward"));
  web_navigation_util::GoForward(&web_state_);
  EXPECT_EQ(2, navigation_manager_->GetLastCommittedItemIndex());
  EXPECT_EQ(1, user_action_tester_.GetActionCount("Forward"));
}
