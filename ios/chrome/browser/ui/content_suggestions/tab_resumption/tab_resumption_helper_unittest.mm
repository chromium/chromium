// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/tab_resumption/tab_resumption_helper.h"

#import "base/memory/scoped_refptr.h"
#import "base/test/scoped_feature_list.h"
#import "components/favicon/ios/web_favicon_driver.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_browser_agent.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"
#import "ios/chrome/browser/ui/content_suggestions/tab_resumption/tab_resumption_item.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_recent_tab_browser_agent.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

// The key to store the timestamp when the scene enters into background.
NSString* kStartSurfaceSceneEnterIntoBackgroundTime =
    @"StartSurfaceSceneEnterIntoBackgroundTime";

// Test URL.
const char ktabURL[] = "http://www.example.com";

}  // namespace

// Testing Suite for the TabResumptionHelper.
class TabResumptionHelperTest : public PlatformTest {
 public:
  TabResumptionHelperTest() {
    scoped_feature_list_.InitWithFeatures({kMagicStack, kTabResumption}, {});

    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();

    browser_ = std::make_unique<TestBrowser>(chrome_browser_state_.get());
    web_state_list_ = browser_->GetWebStateList();

    TestingApplicationContext::GetGlobal()->SetLocalState(local_state_.Get());
    scene_state_ = [[SceneState alloc] initWithAppState:nil];

    SceneStateBrowserAgent::CreateForBrowser(browser_.get(), scene_state_);
    StartSurfaceRecentTabBrowserAgent::CreateForBrowser(browser_.get());

    tab_resumption_helper_ = std::make_unique<TabResumptionHelper>(
        TabResumptionHelper(browser_.get()));
  }

 protected:
  std::unique_ptr<web::FakeWebState> CreateWebState(
      GURL url,
      const std::u16string& title = u"") {
    auto test_web_state = std::make_unique<web::FakeWebState>();
    test_web_state->SetTitle(title);
    test_web_state->SetCurrentURL(url);
    test_web_state->SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    test_web_state->SetBrowserState(chrome_browser_state_.get());
    return test_web_state;
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  IOSChromeScopedTestingLocalState local_state_;
  SceneState* scene_state_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<Browser> browser_;
  WebStateList* web_state_list_;

  std::unique_ptr<TabResumptionHelper> tab_resumption_helper_;
};

// Tests that the block handler is not executed when there is no tab available.
TEST_F(TabResumptionHelperTest, TestNoTabResumptionItem) {
  __block bool callback_executed = false;
  tab_resumption_helper_->LastTabResumptionItem(^(TabResumptionItem* item) {
    callback_executed = true;
  });
  EXPECT_FALSE(callback_executed);
}

// Tests that the right TabResumptionItem is returned when a recent tab is
// available.
TEST_F(TabResumptionHelperTest, TestMostRecentTab) {
  NSUserDefaults* user_defaults = [NSUserDefaults standardUserDefaults];
  [user_defaults setObject:[NSDate date]
                    forKey:kStartSurfaceSceneEnterIntoBackgroundTime];
  [user_defaults synchronize];

  GURL tab_url = GURL(ktabURL);
  // Create a non-NTP WebState.
  web_state_list_->InsertWebState(0, CreateWebState(tab_url, u"Recent Tab"),
                                  WebStateList::INSERT_ACTIVATE,
                                  WebStateOpener());
  favicon::WebFaviconDriver::CreateForWebState(
      web_state_list_->GetActiveWebState(),
      /*favicon_service=*/nullptr);
  StartSurfaceRecentTabBrowserAgent* browser_agent =
      StartSurfaceRecentTabBrowserAgent::FromBrowser(browser_.get());
  browser_agent->SaveMostRecentTab();

  // Create the NTP.
  web_state_list_->InsertWebState(1, CreateWebState(GURL(kChromeUINewTabURL)),
                                  WebStateList::INSERT_ACTIVATE,
                                  WebStateOpener());

  __block bool callback_executed = false;
  tab_resumption_helper_->LastTabResumptionItem(^(TabResumptionItem* item) {
    callback_executed = true;
    EXPECT_TRUE(item);
    EXPECT_TRUE([item.tabTitle isEqualToString:@"Recent Tab"]);
    EXPECT_EQ(item.itemType, TabResumptionItemType::kMostRecentTab);
    EXPECT_EQ(item.tabURL, tab_url);
  });
  EXPECT_TRUE(callback_executed);
}
