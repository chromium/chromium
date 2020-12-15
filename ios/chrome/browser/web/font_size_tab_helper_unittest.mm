// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/font_size_tab_helper.h"

#import <UIKit/UIKit.h>

#include "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/sync_preferences/pref_service_mock_factory.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/prefs/browser_prefs.h"
#include "ios/chrome/browser/web/features.h"
#include "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
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

// Test fixture for FontSizeTabHelper class.
class FontSizeTabHelperTest : public PlatformTest {
 public:
  FontSizeTabHelperTest()
      : application_(OCMPartialMock([UIApplication sharedApplication])) {
    scoped_feature_list_.InitAndEnableFeature(
        {web::kWebPageDefaultZoomFromDynamicType});

    OCMStub([application_ preferredContentSizeCategory])
        .andDo(^(NSInvocation* invocation) {
          [invocation setReturnValue:&preferred_content_size_category_];
        });
    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.SetPrefService(CreatePrefService());
    chrome_browser_state_ = test_cbs_builder.Build();
    web_state_.SetBrowserState(chrome_browser_state_.get());

    auto frames_manager = std::make_unique<web::FakeWebFramesManager>();
    fake_web_frames_manager_ = frames_manager.get();
    web_state_.SetWebFramesManager(std::move(frames_manager));

    GURL url("https://example.com");
    web_state_.SetCurrentURL(url);
    auto main_frame = std::make_unique<web::FakeMainWebFrame>(url);
    fake_main_frame_ = main_frame.get();
    AddWebFrame(std::move(main_frame));

    FontSizeTabHelper::CreateForWebState(&web_state_);
  }
  ~FontSizeTabHelperTest() override { [application_ stopMocking]; }

  void SetPreferredContentSizeCategory(UIContentSizeCategory category) {
    preferred_content_size_category_ = category;
  }

  void SendUIContentSizeCategoryDidChangeNotification() {
    [NSNotificationCenter.defaultCenter
        postNotificationName:UIContentSizeCategoryDidChangeNotification
                      object:nil
                    userInfo:nil];
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

  void AddWebFrame(std::unique_ptr<web::WebFrame> frame) {
    web::WebFrame* frame_ptr = frame.get();
    fake_web_frames_manager_->AddWebFrame(std::move(frame));
    web_state_.OnWebFrameDidBecomeAvailable(frame_ptr);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  web::FakeWebState web_state_;
  web::FakeWebFrame* fake_main_frame_ = nullptr;
  web::FakeWebFramesManager* fake_web_frames_manager_ = nullptr;
  UIContentSizeCategory preferred_content_size_category_ =
      UIContentSizeCategoryLarge;
  id application_ = nil;

  DISALLOW_COPY_AND_ASSIGN(FontSizeTabHelperTest);
};

// Tests that a web page's font size is set properly in a procedure started
// with default |UIApplication.sharedApplication.preferredContentSizeCategory|.
TEST_F(FontSizeTabHelperTest, PageLoadedWithDefaultFontSize) {
  std::vector<std::string> expected_js_call_history;

  // Load web page.
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  EXPECT_EQ(expected_js_call_history,
            fake_main_frame_->GetJavaScriptCallHistory());

  // Change PreferredContentSizeCategory and send
  // UIContentSizeCategoryDidChangeNotification.
  preferred_content_size_category_ = UIContentSizeCategoryExtraLarge;
  SendUIContentSizeCategoryDidChangeNotification();
  expected_js_call_history.push_back(
      "__gCrWeb.accessibility.adjustFontSize(112);");
  EXPECT_EQ(expected_js_call_history,
            fake_main_frame_->GetJavaScriptCallHistory());

  // Reload web page.
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  expected_js_call_history.push_back(
      "__gCrWeb.accessibility.adjustFontSize(112);");
  EXPECT_EQ(expected_js_call_history,
            fake_main_frame_->GetJavaScriptCallHistory());
}

// Tests that a web page's font size is set properly in a procedure started
// with special |UIApplication.sharedApplication.preferredContentSizeCategory|.
TEST_F(FontSizeTabHelperTest, PageLoadedWithExtraLargeFontSize) {
  std::vector<std::string> expected_js_call_history;
  preferred_content_size_category_ = UIContentSizeCategoryExtraLarge;

  // Load web page.
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  expected_js_call_history.push_back(
      "__gCrWeb.accessibility.adjustFontSize(112);");
  EXPECT_EQ(expected_js_call_history,
            fake_main_frame_->GetJavaScriptCallHistory());

  // Change PreferredContentSizeCategory and send
  // UIContentSizeCategoryDidChangeNotification.
  preferred_content_size_category_ = UIContentSizeCategoryExtraExtraLarge;
  SendUIContentSizeCategoryDidChangeNotification();
  expected_js_call_history.push_back(
      "__gCrWeb.accessibility.adjustFontSize(124);");
  EXPECT_EQ(expected_js_call_history,
            fake_main_frame_->GetJavaScriptCallHistory());

  // Reload web page.
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  expected_js_call_history.push_back(
      "__gCrWeb.accessibility.adjustFontSize(124);");
  EXPECT_EQ(expected_js_call_history,
            fake_main_frame_->GetJavaScriptCallHistory());
}

// Tests that UMA log is sent when
// |UIApplication.sharedApplication.preferredContentSizeCategory| returns an
// unrecognizable category.
TEST_F(FontSizeTabHelperTest, PageLoadedWithUnrecognizableFontSize) {
  std::vector<std::string> expected_js_call_history;
  preferred_content_size_category_ = @"This is a new Category";

  // Load web page.
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  EXPECT_EQ(expected_js_call_history,
            fake_main_frame_->GetJavaScriptCallHistory());

  // Change PreferredContentSizeCategory and send
  // UIContentSizeCategoryDidChangeNotification.
  preferred_content_size_category_ = UIContentSizeCategoryExtraExtraLarge;
  SendUIContentSizeCategoryDidChangeNotification();
  expected_js_call_history.push_back(
      "__gCrWeb.accessibility.adjustFontSize(124);");
  EXPECT_EQ(expected_js_call_history,
            fake_main_frame_->GetJavaScriptCallHistory());

  // Reload web page.
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  expected_js_call_history.push_back(
      "__gCrWeb.accessibility.adjustFontSize(124);");
  EXPECT_EQ(expected_js_call_history,
            fake_main_frame_->GetJavaScriptCallHistory());
}

// Tests that the font size is changed in all frames on the page.
TEST_F(FontSizeTabHelperTest, ZoomInAllFrames) {
  preferred_content_size_category_ = UIContentSizeCategoryExtraLarge;

  std::unique_ptr<web::FakeWebFrame> other_frame =
      std::make_unique<web::FakeChildWebFrame>(GURL("https://example.com"));
  web::FakeWebFrame* fake_other_frame = other_frame.get();
  AddWebFrame(std::move(other_frame));

  // Load web page.
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  std::string expected_js_call = "__gCrWeb.accessibility.adjustFontSize(112);";
  EXPECT_EQ(expected_js_call, fake_main_frame_->GetLastJavaScriptCall());
  EXPECT_EQ(expected_js_call, fake_other_frame->GetLastJavaScriptCall());
}

// Tests that the user can zoom in, and that after zooming in, the correct
// Javascript has been executed, and the zoom value is stored in the user pref
// under the correct key.
TEST_F(FontSizeTabHelperTest, ZoomIn) {
  GURL test_url("https://test.url/");
  web_state_.SetCurrentURL(test_url);

  FontSizeTabHelper* font_size_tab_helper =
      FontSizeTabHelper::FromWebState(&web_state_);
  font_size_tab_helper->UserZoom(ZOOM_IN);

  EXPECT_TRUE(font_size_tab_helper->CanUserZoomIn());

  std::string last_executed_js = fake_main_frame_->GetLastJavaScriptCall();
  EXPECT_EQ("__gCrWeb.accessibility.adjustFontSize(110);", last_executed_js);

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
  web_state_.SetCurrentURL(test_url);

  FontSizeTabHelper* font_size_tab_helper =
      FontSizeTabHelper::FromWebState(&web_state_);
  font_size_tab_helper->UserZoom(ZOOM_OUT);

  EXPECT_TRUE(font_size_tab_helper->CanUserZoomOut());

  std::string last_executed_js = fake_main_frame_->GetLastJavaScriptCall();
  EXPECT_EQ("__gCrWeb.accessibility.adjustFontSize(90);", last_executed_js);

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
  web_state_.SetCurrentURL(test_url);

  // First zoom in to setup the reset.
  FontSizeTabHelper* font_size_tab_helper =
      FontSizeTabHelper::FromWebState(&web_state_);
  font_size_tab_helper->UserZoom(ZOOM_IN);
  std::string pref_key =
      ZoomMultiplierPrefKey(preferred_content_size_category_, test_url);
  const base::Value* pref =
      chrome_browser_state_->GetPrefs()->Get(prefs::kIosUserZoomMultipliers);
  EXPECT_EQ(1.1, pref->FindDoublePath(pref_key));

  // Then reset. The pref key should be removed from the dictionary.
  font_size_tab_helper->UserZoom(ZOOM_RESET);
  std::string last_executed_js = fake_main_frame_->GetLastJavaScriptCall();
  EXPECT_EQ("__gCrWeb.accessibility.adjustFontSize(100);", last_executed_js);
  EXPECT_FALSE(pref->FindDoublePath(pref_key));
}

// Tests that when the user changes the accessibility content size category and
// zooms, the resulting zoom level is the multiplication of both parts.
TEST_F(FontSizeTabHelperTest, ZoomAndAccessibilityTextSize) {
  preferred_content_size_category_ = UIContentSizeCategoryExtraLarge;

  GURL test_url("https://test.url/");
  web_state_.SetCurrentURL(test_url);

  FontSizeTabHelper* font_size_tab_helper =
      FontSizeTabHelper::FromWebState(&web_state_);
  font_size_tab_helper->UserZoom(ZOOM_IN);
  std::string pref_key =
      ZoomMultiplierPrefKey(preferred_content_size_category_, test_url);
  std::string last_executed_js = fake_main_frame_->GetLastJavaScriptCall();
  // 1.12 from accessibility * 1.1 from zoom
  EXPECT_EQ("__gCrWeb.accessibility.adjustFontSize(123);", last_executed_js);
  // Only the user zoom portion is stored in the preferences.
  const base::Value* pref =
      chrome_browser_state_->GetPrefs()->Get(prefs::kIosUserZoomMultipliers);
  EXPECT_EQ(1.1, pref->FindDoublePath(pref_key));
}

// Tests that the user pref is cleared when requested.
TEST_F(FontSizeTabHelperTest, ClearUserZoomPrefs) {
  GURL test_url("https://test.url/");
  web_state_.SetCurrentURL(test_url);

  FontSizeTabHelper* font_size_tab_helper =
      FontSizeTabHelper::FromWebState(&web_state_);
  font_size_tab_helper->UserZoom(ZOOM_IN);

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

// Tests that zoom is only enabled if the page content is html.
TEST_F(FontSizeTabHelperTest, CanZoomContent) {
  FontSizeTabHelper* font_size_tab_helper =
      FontSizeTabHelper::FromWebState(&web_state_);

  web_state_.SetContentIsHTML(false);
  EXPECT_FALSE(font_size_tab_helper->CurrentPageSupportsTextZoom());

  web_state_.SetContentIsHTML(true);
  EXPECT_TRUE(font_size_tab_helper->CurrentPageSupportsTextZoom());
}

TEST_F(FontSizeTabHelperTest, GoogleCachedAMPPageHasSeparateKey) {
  // First, zoom in on a regular Google url.
  GURL google_url("https://www.google.com/");
  web_state_.SetCurrentURL(google_url);
  FontSizeTabHelper* font_size_tab_helper =
      FontSizeTabHelper::FromWebState(&web_state_);
  font_size_tab_helper->UserZoom(ZOOM_IN);
  std::string google_pref_key =
      ZoomMultiplierPrefKey(preferred_content_size_category_, google_url);

  // Next, zoom out on a Google AMP url and make sure both states are saved
  // separately.
  GURL google_amp_url("https://www.google.com/amp/s/www.france24.com");
  web_state_.SetCurrentURL(google_amp_url);
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
