// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/device_sharing/model/device_sharing_browser_agent.h"

#import <memory>

#import "components/handoff/handoff_manager.h"
#import "ios/chrome/browser/device_sharing/model/device_sharing_manager.h"
#import "ios/chrome/browser/device_sharing/model/device_sharing_manager_factory.h"
#import "ios/chrome/browser/device_sharing/model/device_sharing_manager_impl.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "net/base/apple/url_conversions.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

class DeviceSharingBrowserAgentTest : public PlatformTest {
 public:
  DeviceSharingBrowserAgentTest()
      : url_1_("http://www.test.com/1.html"),
        url_2_("http://www.test.com/2.html"),
        url_3_("http://www.test.com/3.html"),
        url_4_("http://www.test.com/4.html") {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(DeviceSharingManagerFactory::GetInstance(),
                              DeviceSharingManagerFactory::GetDefaultFactory());

    profile_ = std::move(builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    other_browser_ = std::make_unique<TestBrowser>(profile_.get());
    incognito_browser_ =
        std::make_unique<TestBrowser>(profile_->GetOffTheRecordProfile());
  }

  NSURL* ActiveHandoffUrl() {
    DeviceSharingManagerImpl* sharing_manager =
        static_cast<DeviceSharingManagerImpl*>(
            DeviceSharingManagerFactory::GetForProfile(profile_.get()));
    return [sharing_manager->handoff_manager_ userActivityWebpageURL];
  }

  web::FakeWebState* AppendNewWebState(Browser* browser,
                                       const GURL url,
                                       bool activate = true) {
    auto fake_web_state = std::make_unique<web::FakeWebState>();
    fake_web_state->SetCurrentURL(url);
    web::FakeWebState* inserted_web_state = fake_web_state.get();
    browser->GetWebStateList()->InsertWebState(
        std::move(fake_web_state),
        WebStateList::InsertionParams::Automatic().Activate(activate));
    return inserted_web_state;
  }

  const GURL url_1_;
  const GURL url_2_;
  const GURL url_3_;
  const GURL url_4_;

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<Browser> other_browser_;
  std::unique_ptr<Browser> incognito_browser_;
};

TEST_F(DeviceSharingBrowserAgentTest, UpdateEmptyBrowser) {
  EXPECT_NSEQ(ActiveHandoffUrl(), nil);
  DeviceSharingBrowserAgent::CreateForBrowser(browser_.get());
  DeviceSharingBrowserAgent::FromBrowser(browser_.get())
      ->UpdateForActiveBrowser();
  EXPECT_NSEQ(ActiveHandoffUrl(), nil);
}

TEST_F(DeviceSharingBrowserAgentTest, UpdatePopulatedBrowser) {
  EXPECT_NSEQ(ActiveHandoffUrl(), nil);
  AppendNewWebState(browser_.get(), url_1_);
  DeviceSharingBrowserAgent::CreateForBrowser(browser_.get());
  // `browser_` isn't the active browser in the device manager yet, so expect
  // the active URL hasn't yet changed.
  EXPECT_NSEQ(ActiveHandoffUrl(), nil);
  DeviceSharingBrowserAgent::FromBrowser(browser_.get())
      ->UpdateForActiveBrowser();
  EXPECT_NSEQ(ActiveHandoffUrl(), net::NSURLWithGURL(url_1_));
}

TEST_F(DeviceSharingBrowserAgentTest, ActivateInBrowser) {
  DeviceSharingBrowserAgent::CreateForBrowser(browser_.get());
  DeviceSharingBrowserAgent::FromBrowser(browser_.get())
      ->UpdateForActiveBrowser();
  // As the active browser, newly actiated web states will change the active
  // URL.
  AppendNewWebState(browser_.get(), url_1_);
  EXPECT_NSEQ(ActiveHandoffUrl(), net::NSURLWithGURL(url_1_));
  // Appending a web state without activating will not change the active URL.
  AppendNewWebState(browser_.get(), url_2_, /*activate=*/false);
  EXPECT_NSEQ(ActiveHandoffUrl(), net::NSURLWithGURL(url_1_));
}

TEST_F(DeviceSharingBrowserAgentTest, NavigateInBrowser) {
  DeviceSharingBrowserAgent::CreateForBrowser(browser_.get());
  web::FakeWebState* web_state = AppendNewWebState(browser_.get(), url_1_);
  DeviceSharingBrowserAgent::FromBrowser(browser_.get())
      ->UpdateForActiveBrowser();
  EXPECT_NSEQ(ActiveHandoffUrl(), net::NSURLWithGURL(url_1_));

  web_state->SetVisibleURL(url_2_);
  web_state->OnNavigationFinished(nullptr);
  // Navigating the active web state should update the active URL.
  EXPECT_NSEQ(ActiveHandoffUrl(), net::NSURLWithGURL(url_2_));
}

TEST_F(DeviceSharingBrowserAgentTest, NavigateInactiveInBrowser) {
  DeviceSharingBrowserAgent::CreateForBrowser(browser_.get());
  web::FakeWebState* web_state =
      AppendNewWebState(browser_.get(), url_1_, /*activate=*/false);
  AppendNewWebState(browser_.get(), url_2_);

  DeviceSharingBrowserAgent::FromBrowser(browser_.get())
      ->UpdateForActiveBrowser();
  EXPECT_NSEQ(ActiveHandoffUrl(), net::NSURLWithGURL(url_2_));

  web_state->SetVisibleURL(url_3_);
  web_state->OnNavigationFinished(nullptr);
  // Navigating the non-active web state should not update the active URL.
  EXPECT_NSEQ(ActiveHandoffUrl(), net::NSURLWithGURL(url_2_));
}

TEST_F(DeviceSharingBrowserAgentTest, DestroyBrowser) {
  DeviceSharingBrowserAgent::CreateForBrowser(browser_.get());
  DeviceSharingBrowserAgent::FromBrowser(browser_.get())
      ->UpdateForActiveBrowser();
  AppendNewWebState(browser_.get(), url_1_);
  EXPECT_NSEQ(ActiveHandoffUrl(), net::NSURLWithGURL(url_1_));
  browser_.reset();
  // Destroying the active browser should clear the active URL.
  EXPECT_NSEQ(ActiveHandoffUrl(), nil);
}

TEST_F(DeviceSharingBrowserAgentTest, UpdatePopulatedIncognitoBrowser) {
  EXPECT_NSEQ(ActiveHandoffUrl(), nil);
  AppendNewWebState(incognito_browser_.get(), url_1_);
  DeviceSharingBrowserAgent::CreateForBrowser(incognito_browser_.get());
  DeviceSharingBrowserAgent::FromBrowser(incognito_browser_.get())
      ->UpdateForActiveBrowser();
  // The incognito browser, when active, should never update the active URL.
  EXPECT_NSEQ(ActiveHandoffUrl(), nil);
}

TEST_F(DeviceSharingBrowserAgentTest, ActivateInIncognitoBrowser) {
  DeviceSharingBrowserAgent::CreateForBrowser(incognito_browser_.get());
  DeviceSharingBrowserAgent::FromBrowser(incognito_browser_.get())
      ->UpdateForActiveBrowser();
  AppendNewWebState(incognito_browser_.get(), url_1_);
  // The incognito browser, when active, should never update the active URL.
  EXPECT_NSEQ(ActiveHandoffUrl(), nil);
}

TEST_F(DeviceSharingBrowserAgentTest, NavigateInIncognitoBrowser) {
  DeviceSharingBrowserAgent::CreateForBrowser(incognito_browser_.get());
  web::FakeWebState* incognito_web_state =
      AppendNewWebState(incognito_browser_.get(), url_1_);

  incognito_web_state->SetVisibleURL(url_2_);
  incognito_web_state->OnNavigationFinished(nullptr);
  // Navigating the non-active web state should not update the active URL.
  EXPECT_NSEQ(ActiveHandoffUrl(), nil);
}

TEST_F(DeviceSharingBrowserAgentTest, UpdateTwoBrowsersOneEmpty) {
  AppendNewWebState(browser_.get(), url_1_);
  DeviceSharingBrowserAgent::CreateForBrowser(browser_.get());
  DeviceSharingBrowserAgent::FromBrowser(browser_.get())
      ->UpdateForActiveBrowser();
  EXPECT_NSEQ(ActiveHandoffUrl(), net::NSURLWithGURL(url_1_));
  // Activate an empty browser.
  DeviceSharingBrowserAgent::CreateForBrowser(other_browser_.get());
  DeviceSharingBrowserAgent::FromBrowser(other_browser_.get())
      ->UpdateForActiveBrowser();
  EXPECT_NSEQ(ActiveHandoffUrl(), nil);
  // Switch back.
  DeviceSharingBrowserAgent::FromBrowser(browser_.get())
      ->UpdateForActiveBrowser();
  EXPECT_NSEQ(ActiveHandoffUrl(), net::NSURLWithGURL(url_1_));
}

TEST_F(DeviceSharingBrowserAgentTest, UpdateTwoPopulatedBrowsers) {
  AppendNewWebState(browser_.get(), url_1_);
  DeviceSharingBrowserAgent::CreateForBrowser(browser_.get());
  AppendNewWebState(other_browser_.get(), url_2_);
  DeviceSharingBrowserAgent::CreateForBrowser(other_browser_.get());
  EXPECT_NSEQ(ActiveHandoffUrl(), nil);

  DeviceSharingBrowserAgent::FromBrowser(browser_.get())
      ->UpdateForActiveBrowser();
  EXPECT_NSEQ(ActiveHandoffUrl(), net::NSURLWithGURL(url_1_));

  DeviceSharingBrowserAgent::FromBrowser(other_browser_.get())
      ->UpdateForActiveBrowser();
  EXPECT_NSEQ(ActiveHandoffUrl(), net::NSURLWithGURL(url_2_));

  // Append to `browser_`, but expect no change in active URL as `browser_`
  // isn't active.
  AppendNewWebState(browser_.get(), url_3_);
  EXPECT_NSEQ(ActiveHandoffUrl(), net::NSURLWithGURL(url_2_));

  // Now make `browser_` active and its URL should be the active one.
  DeviceSharingBrowserAgent::FromBrowser(browser_.get())
      ->UpdateForActiveBrowser();
  EXPECT_NSEQ(ActiveHandoffUrl(), net::NSURLWithGURL(url_3_));

  // Destroy `browser_` and the ative browser should be cleared.
  browser_.reset();
  EXPECT_NSEQ(ActiveHandoffUrl(), nil);

  // Activate `other_browser_` and its URL should become active again.
  DeviceSharingBrowserAgent::FromBrowser(other_browser_.get())
      ->UpdateForActiveBrowser();
  EXPECT_NSEQ(ActiveHandoffUrl(), net::NSURLWithGURL(url_2_));
}

TEST_F(DeviceSharingBrowserAgentTest, UpdateAndNavigateTwoBrowsers) {
  web::FakeWebState* web_state = AppendNewWebState(browser_.get(), url_1_);
  DeviceSharingBrowserAgent::CreateForBrowser(browser_.get());
  web::FakeWebState* other_web_state =
      AppendNewWebState(other_browser_.get(), url_2_);
  DeviceSharingBrowserAgent::CreateForBrowser(other_browser_.get());
  EXPECT_NSEQ(ActiveHandoffUrl(), nil);

  DeviceSharingBrowserAgent::FromBrowser(browser_.get())
      ->UpdateForActiveBrowser();
  EXPECT_NSEQ(ActiveHandoffUrl(), net::NSURLWithGURL(url_1_));

  DeviceSharingBrowserAgent::FromBrowser(other_browser_.get())
      ->UpdateForActiveBrowser();
  EXPECT_NSEQ(ActiveHandoffUrl(), net::NSURLWithGURL(url_2_));

  // Navigate in `browser_`, but expect no change in active URL as `browser_`
  // isn't active.
  web_state->SetVisibleURL(url_3_);
  web_state->OnNavigationFinished(nullptr);
  EXPECT_NSEQ(ActiveHandoffUrl(), net::NSURLWithGURL(url_2_));

  // Now make `browser_` active and its URL should be the active one.
  DeviceSharingBrowserAgent::FromBrowser(browser_.get())
      ->UpdateForActiveBrowser();
  EXPECT_NSEQ(ActiveHandoffUrl(), net::NSURLWithGURL(url_3_));

  // Navigate in `other_browser_`, but again expect no change because it isn't
  // active.
  other_web_state->SetVisibleURL(url_4_);
  other_web_state->OnNavigationFinished(nullptr);
  EXPECT_NSEQ(ActiveHandoffUrl(), net::NSURLWithGURL(url_3_));

  // Activate `other_browser` and observe the navigated URL
  DeviceSharingBrowserAgent::FromBrowser(other_browser_.get())
      ->UpdateForActiveBrowser();
  EXPECT_NSEQ(ActiveHandoffUrl(), net::NSURLWithGURL(url_4_));
}

TEST_F(DeviceSharingBrowserAgentTest, UpdateRegularAndIncognitoBrowsers) {
  AppendNewWebState(browser_.get(), url_1_);
  DeviceSharingBrowserAgent::CreateForBrowser(browser_.get());
  AppendNewWebState(other_browser_.get(), url_2_);
  DeviceSharingBrowserAgent::CreateForBrowser(incognito_browser_.get());
  EXPECT_NSEQ(ActiveHandoffUrl(), nil);

  DeviceSharingBrowserAgent::FromBrowser(browser_.get())
      ->UpdateForActiveBrowser();
  EXPECT_NSEQ(ActiveHandoffUrl(), net::NSURLWithGURL(url_1_));

  DeviceSharingBrowserAgent::FromBrowser(incognito_browser_.get())
      ->UpdateForActiveBrowser();
  EXPECT_NSEQ(ActiveHandoffUrl(), nil);

  // Append to `browser_`, but expect no change in active URL as `browser_`
  // isn't active.
  AppendNewWebState(browser_.get(), url_3_);
  EXPECT_NSEQ(ActiveHandoffUrl(), nil);

  // Now make `browser_` active and its URL should be the active one.
  DeviceSharingBrowserAgent::FromBrowser(browser_.get())
      ->UpdateForActiveBrowser();
  EXPECT_NSEQ(ActiveHandoffUrl(), net::NSURLWithGURL(url_3_));
}

TEST_F(DeviceSharingBrowserAgentTest, NavigateInRegularAndIncognitoBrowsers) {
  web::FakeWebState* web_state = AppendNewWebState(browser_.get(), url_1_);
  DeviceSharingBrowserAgent::CreateForBrowser(browser_.get());
  web::FakeWebState* incognito_web_state =
      AppendNewWebState(incognito_browser_.get(), url_2_);
  DeviceSharingBrowserAgent::CreateForBrowser(incognito_browser_.get());
  EXPECT_NSEQ(ActiveHandoffUrl(), nil);

  DeviceSharingBrowserAgent::FromBrowser(browser_.get())
      ->UpdateForActiveBrowser();
  EXPECT_NSEQ(ActiveHandoffUrl(), net::NSURLWithGURL(url_1_));

  DeviceSharingBrowserAgent::FromBrowser(incognito_browser_.get())
      ->UpdateForActiveBrowser();
  EXPECT_NSEQ(ActiveHandoffUrl(), nil);

  // Navigate in `browser_`, but expect no change in active URL as `browser_`
  // isn't active.
  web_state->SetVisibleURL(url_3_);
  web_state->OnNavigationFinished(nullptr);
  EXPECT_NSEQ(ActiveHandoffUrl(), nil);

  // Now make `browser_` active and its URL should be the active one.
  DeviceSharingBrowserAgent::FromBrowser(browser_.get())
      ->UpdateForActiveBrowser();
  EXPECT_NSEQ(ActiveHandoffUrl(), net::NSURLWithGURL(url_3_));

  // Navigate in `incognito_browser_`, but again expect no change because it
  // isn't active.
  incognito_web_state->SetVisibleURL(url_4_);
  incognito_web_state->OnNavigationFinished(nullptr);
  EXPECT_NSEQ(ActiveHandoffUrl(), net::NSURLWithGURL(url_3_));

  // Activate `incognito_browser_` and observe no URL.
  DeviceSharingBrowserAgent::FromBrowser(incognito_browser_.get())
      ->UpdateForActiveBrowser();
  EXPECT_NSEQ(ActiveHandoffUrl(), nil);
}
