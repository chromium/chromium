// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/synced_sessions/model/synced_sessions_util.h"

#import "base/time/time.h"
#import "components/sync_device_info/device_info.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/synced_sessions/model/distant_session.h"
#import "ios/chrome/browser/synced_sessions/model/distant_tab.h"
#import "ios/chrome/browser/url_loading/model/fake_url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

namespace {

using synced_sessions::DistantSession;

// Creates a distant session with `tab_count` number of tabs.
std::unique_ptr<DistantSession> DistantSessionWithTabCount(int tab_count) {
  auto distant_session = std::make_unique<DistantSession>();
  distant_session->tag = "session";
  distant_session->name = "session";
  distant_session->modified_time = base::Time::Now();
  distant_session->form_factor = syncer::DeviceInfo::FormFactor::kDesktop;

  for (int i = 0; i < tab_count; i++) {
    auto tab = std::make_unique<synced_sessions::DistantTab>();
    tab->session_tag = distant_session->tag;
    tab->tab_id = SessionID::FromSerializedValue(i);
    tab->title = u"Tab Title";
    tab->virtual_url = GURL("https://url");
    distant_session->tabs.push_back(std::move(tab));
  }
  return distant_session;
}

}  // namespace

// Test fixture for different ways to open distant sessions.
class SyncedSessionUtilTest : public PlatformTest {
 protected:
  SyncedSessionUtilTest() : PlatformTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    UrlLoadingNotifierBrowserAgent::CreateForBrowser(browser_.get());
    FakeUrlLoadingBrowserAgent::InjectForBrowser(browser_.get());
  }

  FakeUrlLoadingBrowserAgent* GetTestUrlLoader() {
    return FakeUrlLoadingBrowserAgent::FromUrlLoadingBrowserAgent(
        UrlLoadingBrowserAgent::FromBrowser(browser_.get()));
  }

 private:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
};

// Tests that when `first_tab_load_strategy` is
// UrlLoadStrategy::ALWAYS_NEW_FOREGROUND_TAB, the first tab is opened on the
// foreground.
TEST_F(SyncedSessionUtilTest, LoadOneTabWithForegroundStrategy) {
  std::unique_ptr<DistantSession> distant_session =
      DistantSessionWithTabCount(1);
  FakeUrlLoadingBrowserAgent* loader = GetTestUrlLoader();
  OpenDistantSessionInBackground(distant_session.get(),
                                 /*in_incognito=*/NO, 2, loader,
                                 UrlLoadStrategy::ALWAYS_NEW_FOREGROUND_TAB);
  EXPECT_EQ(loader->last_params.load_strategy,
            UrlLoadStrategy::ALWAYS_NEW_FOREGROUND_TAB);
}

// Tests that when `first_tab_load_strategy` is
// UrlLoadStrategy::ALWAYS_NEW_FOREGROUND_TAB, the second tab (and subsequent
// ones) is opened in the background.
TEST_F(SyncedSessionUtilTest, LoadMoreTabsWithForegroundStrategy) {
  std::unique_ptr<DistantSession> distant_session =
      DistantSessionWithTabCount(2);
  FakeUrlLoadingBrowserAgent* loader = GetTestUrlLoader();
  OpenDistantSessionInBackground(distant_session.get(),
                                 /*in_incognito=*/NO, 2, loader,
                                 UrlLoadStrategy::ALWAYS_NEW_FOREGROUND_TAB);
  EXPECT_EQ(loader->last_params.load_strategy, UrlLoadStrategy::NORMAL);
}

// Tests that when `first_tab_load_strategy` is UrlLoadStrategy::NORMAL, the
// first tab is opened in the background.
TEST_F(SyncedSessionUtilTest, LoadOneTabWithBackgroundStrategy) {
  std::unique_ptr<DistantSession> distant_session =
      DistantSessionWithTabCount(1);
  FakeUrlLoadingBrowserAgent* loader = GetTestUrlLoader();
  OpenDistantSessionInBackground(distant_session.get(),
                                 /*in_incognito=*/NO, 2, loader,
                                 UrlLoadStrategy::NORMAL);
  EXPECT_EQ(loader->last_params.load_strategy, UrlLoadStrategy::NORMAL);
}

// Tests that when the number of tabs in the session exceeds the instant load
// threshold, the tabs with index exceeding the threshold will be opened on the
// background but not loaded.
TEST_F(SyncedSessionUtilTest, LazyLoadManyTabs) {
  std::unique_ptr<DistantSession> distant_session =
      DistantSessionWithTabCount(3);
  FakeUrlLoadingBrowserAgent* loader = GetTestUrlLoader();
  OpenDistantSessionInBackground(distant_session.get(),
                                 /*in_incognito=*/NO, 2, loader,
                                 UrlLoadStrategy::ALWAYS_NEW_FOREGROUND_TAB);
  EXPECT_EQ(loader->last_params.load_strategy, UrlLoadStrategy::NORMAL);
  EXPECT_FALSE(loader->last_params.instant_load);
}

// Tests that tabs are opened in incognito mode when `in_cognito` is YES.
TEST_F(SyncedSessionUtilTest, LoadIncognitoTab) {
  std::unique_ptr<DistantSession> distant_session =
      DistantSessionWithTabCount(3);
  FakeUrlLoadingBrowserAgent* loader = GetTestUrlLoader();
  OpenDistantSessionInBackground(distant_session.get(),
                                 /*in_incognito=*/YES, 2, loader,
                                 UrlLoadStrategy::ALWAYS_NEW_FOREGROUND_TAB);
  EXPECT_EQ(loader->last_params.load_strategy, UrlLoadStrategy::NORMAL);
  EXPECT_FALSE(loader->last_params.instant_load);
}
