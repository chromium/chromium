// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_utils.h"

#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

class TabSwitcherUtilsTest : public PlatformTest {
 public:
  TabSwitcherUtilsTest() {
    profile_ = TestProfileIOS::Builder().Build();

    // Creates all kind of browsers.
    browser_regular_ = std::make_unique<TestBrowser>(profile_.get());
    browser_inactive_ = std::make_unique<TestBrowser>(profile_.get());
    browser_incognito_ =
        std::make_unique<TestBrowser>(profile_->GetOffTheRecordProfile());

    // Add them to the apropriate lists.
    browser_list_ = BrowserListFactory::GetForProfile(profile_.get());
    browser_list_->AddBrowser(browser_regular_.get());
    browser_list_->AddBrowser(browser_inactive_.get());
    browser_list_->AddBrowser(browser_incognito_.get());

    browsers_.push_back(browser_regular_.get());
    browsers_.push_back(browser_inactive_.get());
    browsers_.push_back(browser_incognito_.get());
  }

 protected:
  // Appends a new web state in the web state list of `browser`.
  web::FakeWebState* AddTabToBrowser(Browser* browser) {
    std::unique_ptr<web::FakeWebState> fake_web_state =
        std::make_unique<web::FakeWebState>();
    web::FakeWebState* inserted_web_state = fake_web_state.get();
    browser->GetWebStateList()->InsertWebState(
        std::move(fake_web_state),
        WebStateList::InsertionParams::Automatic().Activate());
    return inserted_web_state;
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_regular_;
  std::unique_ptr<TestBrowser> browser_inactive_;
  std::unique_ptr<TestBrowser> browser_incognito_;
  std::vector<Browser*> browsers_;
  raw_ptr<BrowserList> browser_list_;
};

TEST_F(TabSwitcherUtilsTest, GetTheCorrectBrowser) {
  for (Browser* browser : browsers_) {
    ProfileIOS* profile = browser->GetProfile();
    BOOL incognito = profile->IsOffTheRecord();
    web::WebState* inserted_web_state = AddTabToBrowser(browser);
    ASSERT_EQ(browser,
              GetBrowserForTabWithId(browser_list_.get(),
                                     inserted_web_state->GetUniqueIdentifier(),
                                     /*is_otr_tab*/ incognito));
  }
}
