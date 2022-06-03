// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/device_sharing/device_sharing_manager.h"

#include <memory>

#import "components/handoff/handoff_manager.h"
#import "components/handoff/pref_names_ios.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/device_sharing/device_sharing_manager_factory.h"
#import "ios/chrome/browser/device_sharing/device_sharing_manager_impl.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/web/public/test/web_task_environment.h"
#import "net/base/mac/url_conversions.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class DeviceSharingManagerImplTest : public PlatformTest {
 protected:
  DeviceSharingManagerImplTest()
      : PlatformTest(),
        test_url_1_("http://test_sharing_1.html"),
        test_url_2_("http://test_sharing_2.html"),
        test_nsurl_1_(net::NSURLWithGURL(test_url_1_)),
        test_nsurl_2_(net::NSURLWithGURL(test_url_2_)) {
    TestChromeBrowserState::Builder mainBrowserStateBuilder;
    chrome_browser_state_ = mainBrowserStateBuilder.Build();
    sharing_manager_ = static_cast<DeviceSharingManagerImpl*>(
        DeviceSharingManagerFactory::GetForBrowserState(
            chrome_browser_state_.get()));
    browser_ = std::make_unique<TestBrowser>(chrome_browser_state_.get());
    sharing_manager_->SetActiveBrowser(browser_.get());
  }

  NSURL* ActiveHandoffUrl() {
    return [sharing_manager_->handoff_manager_ userActivityWebpageURL];
  }

  const GURL test_url_1_;
  const GURL test_url_2_;
  __strong NSURL* test_nsurl_1_;
  __strong NSURL* test_nsurl_2_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  DeviceSharingManagerImpl* sharing_manager_;
  std::unique_ptr<TestBrowser> browser_;
};

TEST_F(DeviceSharingManagerImplTest, SameServiceForIncognito) {
  EXPECT_EQ(DeviceSharingManagerFactory::GetForBrowserState(
                chrome_browser_state_->GetOffTheRecordChromeBrowserState()),
            sharing_manager_);
}

TEST_F(DeviceSharingManagerImplTest, ShareOneUrl) {
  // Expect that initially no URL is shared.
  EXPECT_NSEQ(ActiveHandoffUrl(), nil);
  // Share one URL, expect it's shared by the handoff manager.
  sharing_manager_->UpdateActiveUrl(browser_.get(), test_url_1_);
  EXPECT_NSEQ(ActiveHandoffUrl(), test_nsurl_1_);
}

TEST_F(DeviceSharingManagerImplTest, ShareTwoUrls) {
  // Share one URL, expect it's shared by the handoff manager.
  sharing_manager_->UpdateActiveUrl(browser_.get(), test_url_1_);
  EXPECT_NSEQ(ActiveHandoffUrl(), test_nsurl_1_);
  // Share a second URL, expect it replaces the prior one.
  sharing_manager_->UpdateActiveUrl(browser_.get(), test_url_2_);
  EXPECT_NSEQ(ActiveHandoffUrl(), test_nsurl_2_);
}

TEST_F(DeviceSharingManagerImplTest, ShareThenClear) {
  // Share one URL, expect it's shared by the handoff manager.
  sharing_manager_->UpdateActiveUrl(browser_.get(), test_url_1_);
  EXPECT_NSEQ(ActiveHandoffUrl(), test_nsurl_1_);
  // Clear the shared URL, expect that the handoff manager shares nothing.
  sharing_manager_->ClearActiveUrl(browser_.get());
  EXPECT_NSEQ(ActiveHandoffUrl(), nil);
  // Share another URL, expect it's shared by the handoff manager.
  sharing_manager_->UpdateActiveUrl(browser_.get(), test_url_2_);
  EXPECT_NSEQ(ActiveHandoffUrl(), test_nsurl_2_);
}

TEST_F(DeviceSharingManagerImplTest, SharingWhenDisabled) {
  // Disable sharing. Expect the handoff manager shares nothing.
  sync_preferences::TestingPrefServiceSyncable* prefs =
      chrome_browser_state_->GetTestingPrefService();
  prefs->SetBoolean(prefs::kIosHandoffToOtherDevices, false);
  sharing_manager_->UpdateActiveUrl(browser_.get(), test_url_1_);
  EXPECT_NSEQ(ActiveHandoffUrl(), nil);
}

TEST_F(DeviceSharingManagerImplTest, SharingThenDisabled) {
  // Share one URL, expect it's shared by the handoff manager.
  sharing_manager_->UpdateActiveUrl(browser_.get(), test_url_1_);
  EXPECT_NSEQ(ActiveHandoffUrl(), test_nsurl_1_);

  // Disable, expaect URL is no longer shared.
  sync_preferences::TestingPrefServiceSyncable* prefs =
      chrome_browser_state_->GetTestingPrefService();
  prefs->SetBoolean(prefs::kIosHandoffToOtherDevices, false);
  EXPECT_NSEQ(ActiveHandoffUrl(), nil);

  // Re-enable. Expect URL is still not shared, but a new URL is sharable.
  prefs->SetBoolean(prefs::kIosHandoffToOtherDevices, true);
  EXPECT_NSEQ(ActiveHandoffUrl(), nil);
  sharing_manager_->UpdateActiveUrl(browser_.get(), test_url_2_);
  EXPECT_NSEQ(ActiveHandoffUrl(), test_nsurl_2_);
}

TEST_F(DeviceSharingManagerImplTest, SwitchActiveBrowser) {
  auto browser_2 = std::make_unique<TestBrowser>(chrome_browser_state_.get());
  // Share one URL, expect it's shared by the handoff manager.
  sharing_manager_->UpdateActiveUrl(browser_.get(), test_url_1_);
  EXPECT_NSEQ(ActiveHandoffUrl(), test_nsurl_1_);
  // Set a new active browser.
  sharing_manager_->SetActiveBrowser(browser_2.get());
  // Share a second URL from the original browser, expect it does not replace
  // the prior one.
  sharing_manager_->UpdateActiveUrl(browser_.get(), test_url_2_);
  EXPECT_NSEQ(ActiveHandoffUrl(), test_nsurl_1_);

  // Share a second URL from the now-active browser, expect it does replace
  // the prior one.
  sharing_manager_->UpdateActiveUrl(browser_2.get(), test_url_2_);
  EXPECT_NSEQ(ActiveHandoffUrl(), test_nsurl_2_);

  // Clear the URL from the original active browser, expect it doesn't clear
  // the active URL.
  sharing_manager_->ClearActiveUrl(browser_.get());
  EXPECT_NSEQ(ActiveHandoffUrl(), test_nsurl_2_);

  // Switch back to the original active browser. The active URL should not
  // change.
  sharing_manager_->SetActiveBrowser(browser_.get());
  EXPECT_NSEQ(ActiveHandoffUrl(), test_nsurl_2_);

  // Now clear the URL for the original browser; it should clear.
  sharing_manager_->ClearActiveUrl(browser_.get());
  EXPECT_NSEQ(ActiveHandoffUrl(), nil);
}
