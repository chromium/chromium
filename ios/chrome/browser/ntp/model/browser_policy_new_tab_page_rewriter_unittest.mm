// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/model/browser_policy_new_tab_page_rewriter.h"

#import "base/test/gtest_util.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

class BrowserPolicyNewTabPageRewriterTest : public PlatformTest {
 public:
  void SetUp() override {
    TestProfileIOS::Builder builder;
    profile_ = std::move(builder).Build();
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
};

// Tests that chrome://newtab is re-written to the custom NTP Location URL when
// it is set by the policy.
TEST_F(BrowserPolicyNewTabPageRewriterTest, CustomNtpUrl) {
  std::string custom_url = "https://store.google.com";
  profile_->GetPrefs()->SetString(prefs::kNewTabPageLocationOverride,
                                  custom_url);
  GURL url = GURL(kChromeUINewTabURL);

  EXPECT_TRUE(WillHandleWebBrowserNewTabPageURLForPolicy(&url, profile_.get()));
  EXPECT_EQ(url, GURL(custom_url));
}

// Tests that chrome://newtab is not re-written if the custom NTP Location URL
// set by the policy is the same.
TEST_F(BrowserPolicyNewTabPageRewriterTest, SameNtpUrl) {
  std::string custom_url = kChromeUINewTabURL;
  profile_->GetPrefs()->SetString(prefs::kNewTabPageLocationOverride,
                                  custom_url);

  GURL url = GURL(kChromeUINewTabURL);

  EXPECT_FALSE(
      WillHandleWebBrowserNewTabPageURLForPolicy(&url, profile_.get()));
  EXPECT_EQ(url, GURL(kChromeUINewTabURL));
}

// Tests that chrome://newtab is not re-written if the custom NTP Location URL
// set by the policy is not valid.
TEST_F(BrowserPolicyNewTabPageRewriterTest, InvalidCustomNtpUrl) {
  std::string custom_url = "blabla";
  profile_->GetPrefs()->SetString(prefs::kNewTabPageLocationOverride,
                                  custom_url);
  GURL url = GURL(kChromeUINewTabURL);

  EXPECT_FALSE(
      WillHandleWebBrowserNewTabPageURLForPolicy(&url, profile_.get()));
  EXPECT_EQ(url, GURL(kChromeUINewTabURL));
}

// Tests that chrome://newtab is not re-written when there is no custom NTP
// Location URL set by the policy.
TEST_F(BrowserPolicyNewTabPageRewriterTest, NoCustomNtpUrl) {
  GURL url = GURL(kChromeUINewTabURL);
  EXPECT_FALSE(
      WillHandleWebBrowserNewTabPageURLForPolicy(&url, profile_.get()));
  EXPECT_EQ(url, GURL(kChromeUINewTabURL));
}

// Tests that chrome://newtab is not re-written when it is in incognito mode.
TEST_F(BrowserPolicyNewTabPageRewriterTest, IncognitoMode) {
  web::FakeBrowserState fake_profile;
  fake_profile.SetOffTheRecord(true);
  GURL url = GURL(kChromeUINewTabURL);
  EXPECT_FALSE(WillHandleWebBrowserNewTabPageURLForPolicy(&url, &fake_profile));
}
