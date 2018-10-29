// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/font_size_tab_helper.h"

#import <UIKit/UIKit.h>

#include "base/strings/utf_string_conversions.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for FontSizeTabHelper class.
class FontSizeTabHelperTest : public PlatformTest {
 public:
  FontSizeTabHelperTest()
      : application_(OCMPartialMock([UIApplication sharedApplication])) {
    OCMStub([application_ preferredContentSizeCategory])
        .andDo(^(NSInvocation* invocation) {
          [invocation setReturnValue:&preferred_content_size_category_];
        });
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

 protected:
  web::TestWebState web_state_;
  UIContentSizeCategory preferred_content_size_category_ =
      UIContentSizeCategoryLarge;
  id application_ = nil;

  DISALLOW_COPY_AND_ASSIGN(FontSizeTabHelperTest);
};

// Tests that a web page's font size is set properly in a procedure started
// with default |UIApplication.sharedApplication.preferredContentSizeCategory|.
TEST_F(FontSizeTabHelperTest, PageLoadedWithDefaultFontSize) {
  std::string last_executed_js;

  // Load web page.
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  last_executed_js = base::UTF16ToUTF8(web_state_.GetLastExecutedJavascript());
  EXPECT_EQ("", last_executed_js);

  // Change PreferredContentSizeCategory and send
  // UIContentSizeCategoryDidChangeNotification.
  preferred_content_size_category_ = UIContentSizeCategoryExtraLarge;
  SendUIContentSizeCategoryDidChangeNotification();
  last_executed_js = base::UTF16ToUTF8(web_state_.GetLastExecutedJavascript());
  EXPECT_EQ("__gCrWeb.accessibility.adjustFontSize(112)", last_executed_js);
  web_state_.ClearLastExecutedJavascript();

  // Reload web page.
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  last_executed_js = base::UTF16ToUTF8(web_state_.GetLastExecutedJavascript());
  EXPECT_EQ("__gCrWeb.accessibility.adjustFontSize(112)", last_executed_js);
}

// Tests that a web page's font size is set properly in a procedure started
// with special |UIApplication.sharedApplication.preferredContentSizeCategory|.
TEST_F(FontSizeTabHelperTest, PageLoadedWithExtraLargeFontSize) {
  std::string last_executed_js;
  preferred_content_size_category_ = UIContentSizeCategoryExtraLarge;

  // Load web page.
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  last_executed_js = base::UTF16ToUTF8(web_state_.GetLastExecutedJavascript());
  EXPECT_EQ("__gCrWeb.accessibility.adjustFontSize(112)", last_executed_js);
  web_state_.ClearLastExecutedJavascript();

  // Change PreferredContentSizeCategory and send
  // UIContentSizeCategoryDidChangeNotification.
  preferred_content_size_category_ = UIContentSizeCategoryExtraExtraLarge;
  SendUIContentSizeCategoryDidChangeNotification();
  last_executed_js = base::UTF16ToUTF8(web_state_.GetLastExecutedJavascript());
  EXPECT_EQ("__gCrWeb.accessibility.adjustFontSize(124)", last_executed_js);
  web_state_.ClearLastExecutedJavascript();

  // Reload web page.
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  last_executed_js = base::UTF16ToUTF8(web_state_.GetLastExecutedJavascript());
  EXPECT_EQ("__gCrWeb.accessibility.adjustFontSize(124)", last_executed_js);
}

// Tests that UMA log is sent when
// |UIApplication.sharedApplication.preferredContentSizeCategory| returns an
// unrecognizable category.
TEST_F(FontSizeTabHelperTest, PageLoadedWithUnrecognizableFontSize) {
  std::string last_executed_js;
  preferred_content_size_category_ = @"This is a new Category";

  // Load web page.
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  last_executed_js = base::UTF16ToUTF8(web_state_.GetLastExecutedJavascript());
  EXPECT_EQ("", last_executed_js);

  // Change PreferredContentSizeCategory and send
  // UIContentSizeCategoryDidChangeNotification.
  preferred_content_size_category_ = UIContentSizeCategoryExtraExtraLarge;
  SendUIContentSizeCategoryDidChangeNotification();
  last_executed_js = base::UTF16ToUTF8(web_state_.GetLastExecutedJavascript());
  EXPECT_EQ("__gCrWeb.accessibility.adjustFontSize(124)", last_executed_js);
  web_state_.ClearLastExecutedJavascript();

  // Reload web page.
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  last_executed_js = base::UTF16ToUTF8(web_state_.GetLastExecutedJavascript());
  EXPECT_EQ("__gCrWeb.accessibility.adjustFontSize(124)", last_executed_js);
}
