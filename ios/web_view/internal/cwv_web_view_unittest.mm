// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/cwv_web_view_internal.h"

#include <memory>

#import "ios/web/common/crw_input_view_provider.h"
#include "ios/web/public/test/scoped_testing_web_client.h"
#include "ios/web/public/test/web_task_environment.h"
#include "ios/web/public/web_client.h"
#import "ios/web_view/internal/cwv_web_view_configuration_internal.h"
#include "ios/web_view/internal/web_view_browser_state.h"
#include "ios/web_view/test/test_with_locale_and_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

class CWVWebViewTest : public TestWithLocaleAndResources {
 public:
  void SetUp() override {
    TestWithLocaleAndResources::SetUp();
    CWVWebView.customUserAgent = nil;
  }

  void TearDown() override {
    TestWithLocaleAndResources::TearDown();
    CWVWebView.customUserAgent = nil;
  }

 protected:
  CWVWebViewTest() : web_client_(std::make_unique<web::WebClient>()) {}

  web::WebTaskEnvironment task_environment_;
  web::ScopedTestingWebClient web_client_;
};

// Test +[CWVWebView customUserAgent].
TEST_F(CWVWebViewTest, CustomUserAgent) {
  EXPECT_FALSE(CWVWebView.customUserAgent);
  CWVWebView.customUserAgent = @"FooCustomUserAgent";
  EXPECT_NSEQ(@"FooCustomUserAgent", CWVWebView.customUserAgent);
}

// Test CWVWebView's inputAccessoryView controls whether or not the overriding
// behavior is enabled.
TEST_F(CWVWebViewTest, InputAccessoryView) {
  std::unique_ptr<WebViewBrowserState> browser_state =
      std::make_unique<WebViewBrowserState>(/*off_the_record=*/false);
  CWVWebViewConfiguration* configuration = [[CWVWebViewConfiguration alloc]
      initWithBrowserState:std::move(browser_state)];

  CGRect frame = CGRectMake(0, 0, 1, 1);
  CWVWebView* web_view = [[CWVWebView alloc] initWithFrame:frame
                                             configuration:configuration];
  EXPECT_FALSE(web_view.inputAccessoryView);
  EXPECT_FALSE([web_view webStateInputViewProvider:nil]);

  UIView* input_accessory_view = [[UIView alloc] initWithFrame:frame];
  web_view.inputAccessoryView = input_accessory_view;
  EXPECT_EQ(web_view, [web_view webStateInputViewProvider:nil]);

  // |browser_state| cannot outlive |task_environment_| or it will DCHECK.
  [configuration shutDown];
}

}  // namespace ios_web_view
