// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/static_content/static_html_native_content.h"

#include <memory>

#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/ui/static_content/static_html_view_controller.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface StaticHtmlNativeContentTestGenerator : NSObject<HtmlGenerator>
@end

@implementation StaticHtmlNativeContentTestGenerator
- (void)generateHtml:(HtmlCallback)callback {
  callback(@"<html><body><p>Hello, World!</p></body></html>");
}
@end

namespace {

class StaticHtmlNativeContentTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();
  }
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
};

TEST_F(StaticHtmlNativeContentTest, BasicResourceTest) {
  GURL url("chrome://foo");
  StaticHtmlNativeContent* content = [[StaticHtmlNativeContent alloc]
      initWithResourcePathResource:@"about_credits.html"
                      browserState:chrome_browser_state_.get()
                               url:url];

  ASSERT_EQ(url, [content url]);
}

TEST_F(StaticHtmlNativeContentTest, BasicInitTest) {
  GURL url("chrome://foo");
  StaticHtmlNativeContentTestGenerator* generator =
      [[StaticHtmlNativeContentTestGenerator alloc] init];

  StaticHtmlViewController* viewController = [[StaticHtmlViewController alloc]
      initWithGenerator:generator
           browserState:chrome_browser_state_.get()];
  StaticHtmlNativeContent* content = [[StaticHtmlNativeContent alloc]
      initWithStaticHTMLViewController:viewController
                                   URL:url];
  ASSERT_EQ(url, [content url]);
}

}  // namespace
