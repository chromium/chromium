// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/font_size/font_size_tab_helper.h"

#import <UIKit/UIKit.h>

#include "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "base/test/scoped_feature_list.h"
#include "components/sync_preferences/pref_service_mock_factory.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/prefs/browser_prefs.h"
#import "ios/chrome/browser/web/chrome_web_test.h"
#include "ios/chrome/browser/web/features.h"
#import "ios/public/provider/chrome/browser/font_size_java_script_feature.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using sync_preferences::PrefServiceMockFactory;
using sync_preferences::PrefServiceSyncable;
using user_prefs::PrefRegistrySyncable;

// Test fixture for FontSizeTabHelper class which creates a FakeWebState.
class FontSizeTabHelperFakeWebStateTest : public PlatformTest {
 public:
  FontSizeTabHelperFakeWebStateTest() {
    scoped_feature_list_.InitAndEnableFeature(
        {web::kWebPageDefaultZoomFromDynamicType});

    FontSizeTabHelper::CreateForWebState(&web_state_);
  }
  ~FontSizeTabHelperFakeWebStateTest() override {}

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  web::FakeWebState web_state_;

  DISALLOW_COPY_AND_ASSIGN(FontSizeTabHelperFakeWebStateTest);
};

// Tests that zoom is only enabled if the page content is html.
TEST_F(FontSizeTabHelperFakeWebStateTest, CanZoomContent) {
  FontSizeTabHelper* font_size_tab_helper =
      FontSizeTabHelper::FromWebState(&web_state_);

  web_state_.SetContentIsHTML(false);
  EXPECT_FALSE(font_size_tab_helper->CurrentPageSupportsTextZoom());

  web_state_.SetContentIsHTML(true);
  EXPECT_TRUE(font_size_tab_helper->CurrentPageSupportsTextZoom());
}

// Test fixture for FontSizeTabHelper class with a real WebState.
class FontSizeTabHelperTest : public ChromeWebTest {
 public:
  FontSizeTabHelperTest()
      : ChromeWebTest(std::make_unique<web::FakeWebClient>()),
        application_(OCMPartialMock([UIApplication sharedApplication])) {
    scoped_feature_list_.InitAndEnableFeature(
        {web::kWebPageDefaultZoomFromDynamicType});

    OCMStub([application_ preferredContentSizeCategory])
        .andDo(^(NSInvocation* invocation) {
          [invocation setReturnValue:&preferred_content_size_category_];
        });
  }
  ~FontSizeTabHelperTest() override { [application_ stopMocking]; }

  void SetUp() override {
    ChromeWebTest::SetUp();

    web::FakeWebClient* web_client =
        static_cast<web::FakeWebClient*>(WebTestWithWebState::GetWebClient());
    web_client->SetJavaScriptFeatures(
        {FontSizeJavaScriptFeature::GetInstance()});

    FontSizeTabHelper::CreateForWebState(web_state());
  }

  void SetPreferredContentSizeCategory(UIContentSizeCategory category) {
    preferred_content_size_category_ = category;
  }

  void SendUIContentSizeCategoryDidChangeNotification() {
    [NSNotificationCenter.defaultCenter
        postNotificationName:UIContentSizeCategoryDidChangeNotification
                      object:nil
                    userInfo:nil];

    base::test::ios::SpinRunLoopWithMinDelay(
        base::TimeDelta::FromSecondsD(base::test::ios::kSpinDelaySeconds));
  }

  std::string ZoomMultiplierPrefKey(UIContentSizeCategory category, GURL url) {
    return base::StringPrintf(
        "%s.%s", base::SysNSStringToUTF8(category).c_str(), url.host().c_str());
  }

  std::unique_ptr<PrefServiceSyncable> CreatePrefService() {
    scoped_refptr<PrefRegistrySyncable> registry =
        base::MakeRefCounted<PrefRegistrySyncable>();
    // Registers Translate and Language related prefs.
    RegisterBrowserStatePrefs(registry.get());
    PrefServiceMockFactory factory;
    return factory.CreateSyncable(registry.get());
  }

  void LoadWebpage() {
    GURL test_url("https://test.url/");
    LoadWebpage(test_url);
  }

  void LoadWebpage(const GURL& url) {
    LoadHtml(@"<html><body>Content</body></html>", url);

    base::test::ios::SpinRunLoopWithMinDelay(
        base::TimeDelta::FromSecondsD(base::test::ios::kSpinDelaySeconds));
  }

  // Returns the current value of the WebKit text size adjustment style.
  NSString* GetMainFrameTextSizeAdjustment() {
    id result = ExecuteJavaScript(@"document.body.style.webkitTextSizeAdjust");
    return static_cast<NSString*>(result);
  }

  // Returns the current value of the WebKit text size adjustment style.
  NSString* GetIframeTextSizeAdjusment() {
    id result = ExecuteJavaScript(
        @"window.frames[0].document.body.style.webkitTextSizeAdjust");
    return static_cast<NSString*>(result);
  }

  int GetCurrentFontSizeFromString(NSString* adjustment) {
    if (!adjustment || !adjustment.length) {
      return 0;
    }
    NSRange percentCharRange = NSMakeRange(adjustment.length - 1, 1);
    NSString* fontSizePercentage =
        [adjustment stringByReplacingCharactersInRange:percentCharRange
                                            withString:@""];
    return [fontSizePercentage intValue];
  }

  // Waits for the text size adjustment value of the main frame to be
  // |adjustment|. Returns true if the value matches |adjustment| within
  // |kWaitForJSCompletionTimeout|, false otherwise.
  bool WaitForMainFrameTextSizeAdjustmentEqualTo(int adjustment) {
    return base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForJSCompletionTimeout, ^bool {
          return GetCurrentFontSizeFromString(
                     GetMainFrameTextSizeAdjustment()) == adjustment;
        });
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  UIContentSizeCategory preferred_content_size_category_ =
      UIContentSizeCategoryLarge;
  id application_ = nil;

  DISALLOW_COPY_AND_ASSIGN(FontSizeTabHelperTest);
};

// Tests that a web page's font size is set properly in a procedure started
// with default |UIApplication.sharedApplication.preferredContentSizeCategory|.
TEST_F(FontSizeTabHelperTest, PageLoadedWithDefaultFontSize) {
  LoadWebpage();
  ASSERT_EQ(0ul, [GetMainFrameTextSizeAdjustment() length]);

  // Change PreferredContentSizeCategory and send
  // UIContentSizeCategoryDidChangeNotification.
  preferred_content_size_category_ = UIContentSizeCategoryExtraLarge;
  SendUIContentSizeCategoryDidChangeNotification();

  EXPECT_TRUE(WaitForMainFrameTextSizeAdjustmentEqualTo(112));

  // Reload web page, font size should be set to the same previous value.
  LoadWebpage();

  EXPECT_TRUE(WaitForMainFrameTextSizeAdjustmentEqualTo(112));
}

// Tests that a web page's font size is set properly in a procedure started
// with special |UIApplication.sharedApplication.preferredContentSizeCategory|.
TEST_F(FontSizeTabHelperTest, PageLoadedWithExtraLargeFontSize) {
  preferred_content_size_category_ = UIContentSizeCategoryExtraLarge;

  LoadWebpage();
  EXPECT_TRUE(WaitForMainFrameTextSizeAdjustmentEqualTo(112));

  // Change PreferredContentSizeCategory and send
  // UIContentSizeCategoryDidChangeNotification.
  preferred_content_size_category_ = UIContentSizeCategoryExtraExtraLarge;
  SendUIContentSizeCategoryDidChangeNotification();
  EXPECT_TRUE(WaitForMainFrameTextSizeAdjustmentEqualTo(124));

  // Reload web page, font size should be set to the same previous value.
  LoadWebpage();
  EXPECT_TRUE(WaitForMainFrameTextSizeAdjustmentEqualTo(124));
}

// Tests that UMA log is sent when
// |UIApplication.sharedApplication.preferredContentSizeCategory| returns an
// unrecognizable category.
TEST_F(FontSizeTabHelperTest, PageLoadedWithUnrecognizableFontSize) {
  preferred_content_size_category_ = @"This is a new Category";

  LoadWebpage();
  ASSERT_EQ(0ul, [GetMainFrameTextSizeAdjustment() length]);

  // Change PreferredContentSizeCategory and send
  // UIContentSizeCategoryDidChangeNotification.
  preferred_content_size_category_ = UIContentSizeCategoryExtraExtraLarge;
  SendUIContentSizeCategoryDidChangeNotification();

  EXPECT_TRUE(WaitForMainFrameTextSizeAdjustmentEqualTo(124));

  // Reload web page, font size should be set to the same previous value.
  LoadWebpage();

  EXPECT_TRUE(WaitForMainFrameTextSizeAdjustmentEqualTo(124));
}

// Tests that the font size is changed in all frames on the page.
TEST_F(FontSizeTabHelperTest, ZoomInAllFrames) {
  preferred_content_size_category_ = UIContentSizeCategoryExtraLarge;

  LoadHtml(@"<html><body>Content<iframe srcdoc=\"<html>iFrame "
           @"Content</html>\"></iframe></body></html>");

  EXPECT_TRUE(WaitForMainFrameTextSizeAdjustmentEqualTo(112));

  EXPECT_EQ(112, GetCurrentFontSizeFromString(GetIframeTextSizeAdjusment()));
}

// Tests that the user can zoom in, and that after zooming in, the correct
// Javascript has been executed, and the zoom value is stored in the user pref
// under the correct key.
TEST_F(FontSizeTabHelperTest, ZoomIn) {
  GURL test_url("https://test.url/");
  LoadWebpage(test_url);

  FontSizeTabHelper* font_size_tab_helper =
      FontSizeTabHelper::FromWebState(web_state());
  font_size_tab_helper->UserZoom(ZOOM_IN);

  EXPECT_TRUE(font_size_tab_helper->CanUserZoomIn());

  EXPECT_TRUE(WaitForMainFrameTextSizeAdjustmentEqualTo(110));

  // Check that new zoom value is also saved in the pref under the right key.
  std::string pref_key =
      ZoomMultiplierPrefKey(preferred_content_size_category_, test_url);
  const base::Value* pref =
      chrome_browser_state_->GetPrefs()->Get(prefs::kIosUserZoomMultipliers);
  EXPECT_EQ(1.1, pref->FindDoublePath(pref_key));
}

// Tests that the user can zoom out, and that after zooming out, the correct
// Javascript has been executed, and the zoom value is stored in the user pref
// under the correct key.
TEST_F(FontSizeTabHelperTest, ZoomOut) {
  GURL test_url("https://test.url/");
  LoadWebpage(test_url);

  FontSizeTabHelper* font_size_tab_helper =
      FontSizeTabHelper::FromWebState(web_state());
  font_size_tab_helper->UserZoom(ZOOM_OUT);

  EXPECT_TRUE(font_size_tab_helper->CanUserZoomOut());

  EXPECT_TRUE(WaitForMainFrameTextSizeAdjustmentEqualTo(90));

  // Check that new zoom value is also saved in the pref under the right key.
  std::string pref_key =
      ZoomMultiplierPrefKey(preferred_content_size_category_, test_url);
  const base::Value* pref =
      chrome_browser_state_->GetPrefs()->Get(prefs::kIosUserZoomMultipliers);
  EXPECT_EQ(0.9, pref->FindDoublePath(pref_key));
}

// Tests after resetting the zoom level, the correct Javascript has been
// executed, and the value in the user prefs for the key has ben cleared.
TEST_F(FontSizeTabHelperTest, ResetZoom) {
  GURL test_url("https://test.url/");
  LoadWebpage(test_url);

  // First zoom in to setup the reset.
  FontSizeTabHelper* font_size_tab_helper =
      FontSizeTabHelper::FromWebState(web_state());
  font_size_tab_helper->UserZoom(ZOOM_IN);
  std::string pref_key =
      ZoomMultiplierPrefKey(preferred_content_size_category_, test_url);
  const base::Value* pref =
      chrome_browser_state_->GetPrefs()->Get(prefs::kIosUserZoomMultipliers);
  EXPECT_EQ(1.1, pref->FindDoublePath(pref_key));

  // Then reset. The pref key should be removed from the dictionary.
  font_size_tab_helper->UserZoom(ZOOM_RESET);

  EXPECT_TRUE(WaitForMainFrameTextSizeAdjustmentEqualTo(100));
  EXPECT_FALSE(pref->FindDoublePath(pref_key));
}

// Tests that when the user changes the accessibility content size category and
// zooms, the resulting zoom level is the multiplication of both parts.
TEST_F(FontSizeTabHelperTest, ZoomAndAccessibilityTextSize) {
  preferred_content_size_category_ = UIContentSizeCategoryExtraLarge;

  GURL test_url("https://test.url/");
  LoadWebpage(test_url);

  FontSizeTabHelper* font_size_tab_helper =
      FontSizeTabHelper::FromWebState(web_state());
  font_size_tab_helper->UserZoom(ZOOM_IN);

  std::string pref_key =
      ZoomMultiplierPrefKey(preferred_content_size_category_, test_url);
  // 1.12 from accessibility * 1.1 from zoom
  EXPECT_TRUE(WaitForMainFrameTextSizeAdjustmentEqualTo(123));
  // Only the user zoom portion is stored in the preferences.
  const base::Value* pref =
      chrome_browser_state_->GetPrefs()->Get(prefs::kIosUserZoomMultipliers);
  EXPECT_EQ(1.1, pref->FindDoublePath(pref_key));
}

// Tests that the user pref is cleared when requested.
TEST_F(FontSizeTabHelperTest, ClearUserZoomPrefs) {
  GURL test_url("https://test.url/");
  LoadWebpage(test_url);

  FontSizeTabHelper* font_size_tab_helper =
      FontSizeTabHelper::FromWebState(web_state());
  font_size_tab_helper->UserZoom(ZOOM_IN);
  base::test::ios::SpinRunLoopWithMinDelay(
      base::TimeDelta::FromSecondsD(base::test::ios::kSpinDelaySeconds));

  // Make sure the first value is stored in the pref store.
  const base::Value* pref =
      chrome_browser_state_->GetPrefs()->Get(prefs::kIosUserZoomMultipliers);
  std::string pref_key =
      ZoomMultiplierPrefKey(preferred_content_size_category_, test_url);
  EXPECT_EQ(1.1, pref->FindDoublePath(pref_key));

  FontSizeTabHelper::ClearUserZoomPrefs(chrome_browser_state_->GetPrefs());

  EXPECT_TRUE(chrome_browser_state_->GetPrefs()
                  ->Get(prefs::kIosUserZoomMultipliers)
                  ->DictEmpty());
}

TEST_F(FontSizeTabHelperTest, GoogleCachedAMPPageHasSeparateKey) {
  // First, zoom in on a regular Google url.
  GURL google_url("https://www.google.com/");
  LoadWebpage(google_url);
  FontSizeTabHelper* font_size_tab_helper =
      FontSizeTabHelper::FromWebState(web_state());
  font_size_tab_helper->UserZoom(ZOOM_IN);
  std::string google_pref_key =
      ZoomMultiplierPrefKey(preferred_content_size_category_, google_url);

  // Next, zoom out on a Google AMP url and make sure both states are saved
  // separately.
  GURL google_amp_url("https://www.google.com/amp/s/www.france24.com");
  LoadWebpage(google_amp_url);
  font_size_tab_helper->UserZoom(ZOOM_OUT);
  // Google AMP pages use a different key for the URL part.
  std::string google_amp_pref_key = base::StringPrintf(
      "%s.%s",
      base::SysNSStringToUTF8(preferred_content_size_category_).c_str(),
      "www.google.com/amp");

  EXPECT_NE(google_pref_key, google_amp_pref_key);

  const base::Value* pref =
      chrome_browser_state_->GetPrefs()->Get(prefs::kIosUserZoomMultipliers);
  EXPECT_EQ(1.1, pref->FindDoublePath(google_pref_key));
  EXPECT_EQ(0.9, pref->FindDoublePath(google_amp_pref_key));
}
