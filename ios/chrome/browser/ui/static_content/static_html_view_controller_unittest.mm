// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/static_content/static_html_view_controller.h"

#include <memory>

#import "base/mac/foundation_util.h"
#import "base/test/ios/wait_util.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/url_loading/test_url_loading_service.h"
#include "ios/chrome/browser/url_loading/url_loading_params.h"
#include "ios/chrome/browser/url_loading/url_loading_service_factory.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/testing/ocmock_complex_type_helper.h"
#import "ios/web/public/deprecated/crw_native_content.h"
#include "ios/web/public/navigation/referrer.h"
#import "ios/web/public/test/fakes/test_web_client.h"
#import "ios/web/public/test/js_test_util.h"
#include "ios/web/public/test/scoped_testing_web_client.h"
#include "ios/web/public/test/web_task_environment.h"
#import "net/base/mac/url_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Generator that returns the value of a localized identifier.
@interface L10nHtmlGenerator : NSObject<HtmlGenerator> {
 @private
  int messageId_;
}
- (id)initWithMessageId:(int)messageId;
@end

@implementation L10nHtmlGenerator
- (id)initWithMessageId:(int)messageId {
  if ((self = [super init])) {
    messageId_ = messageId;
  }
  return self;
}
- (void)generateHtml:(HtmlCallback)callback {
  callback(l10n_util::GetNSString(messageId_));
}
@end

namespace {

bool isRunLoopDry = true;

// Wait until either all event in the run loop at the time of execution are
// executed, or 5.0 seconds, whichever happens first. If |publishSentinel| is
// false, the caller is responsible to set |isRunLoopDry| to true when it
// considers the run loop dry.
void DryRunLoop(bool publishSentinel) {
  isRunLoopDry = false;
  if (publishSentinel) {
    dispatch_async(dispatch_get_main_queue(), ^{
      isRunLoopDry = true;
    });
  }
  NSDate* startDate = [NSDate date];
  NSDate* endDate = [NSDate dateWithTimeIntervalSinceNow:5.0];

  while (!isRunLoopDry && (-[startDate timeIntervalSinceNow]) < 5.0) {
    [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
                             beforeDate:endDate];
  }
}

class StaticHtmlViewControllerTest : public PlatformTest {
 public:
  StaticHtmlViewControllerTest()
      : web_client_(std::make_unique<web::TestWebClient>()) {}

 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        UrlLoadingServiceFactory::GetInstance(),
        UrlLoadingServiceFactory::GetDefaultFactory());
    chrome_browser_state_ = test_cbs_builder.Build();
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  web::ScopedTestingWebClient web_client_;
};

// Tests the creation of a StaticHtmlViewController displaying a resource file.
TEST_F(StaticHtmlViewControllerTest, LoadResourceTest) {
  id<CRWNativeContentDelegate> delegate =
      [OCMockObject mockForProtocol:@protocol(CRWNativeContentDelegate)];
  GURL referrer_url("chrome://foo");
  web::Referrer referrer(referrer_url, web::ReferrerPolicyDefault);
  StaticHtmlViewController* content = [[StaticHtmlViewController alloc]
      initWithResource:@"terms_en.html"
          browserState:chrome_browser_state_.get()];
  [content setDelegate:delegate];
  [[(OCMockObject*)delegate expect]
       nativeContent:content
      titleDidChange:[OCMArg checkWithBlock:^BOOL(id value) {
        isRunLoopDry = true;
        return [@"Google Chrome Terms of Service"
            isEqualToString:(NSString*)value];
      }]];
  [content triggerPendingLoad];
  DryRunLoop(false);

  TestUrlLoadingService* url_loader =
      (TestUrlLoadingService*)UrlLoadingServiceFactory::GetForBrowserState(
          chrome_browser_state_.get());
  EXPECT_EQ(0, url_loader->load_current_tab_call_count);
  EXPECT_EQ(0, url_loader->load_new_tab_call_count);

  DryRunLoop(true);

  EXPECT_EQ(0, url_loader->load_current_tab_call_count);
  EXPECT_EQ(0, url_loader->load_new_tab_call_count);
  ASSERT_OCMOCK_VERIFY((OCMockObject*)delegate);
}

// Tests the creation of a StaticHtmlViewController displaying a local file.
TEST_F(StaticHtmlViewControllerTest, LoadFileURLTest) {
  id<CRWNativeContentDelegate> delegate =
      [OCMockObject mockForProtocol:@protocol(CRWNativeContentDelegate)];
  GURL referrer_url("chrome://foo");
  web::Referrer referrer(referrer_url, web::ReferrerPolicyDefault);
  NSURL* fileURL = [NSURL
      fileURLWithPath:[[NSBundle mainBundle] pathForResource:@"terms_en.html"
                                                      ofType:nil
                                                 inDirectory:nil]];
  StaticHtmlViewController* content = [[StaticHtmlViewController alloc]
              initWithFileURL:net::GURLWithNSURL(fileURL)
      allowingReadAccessToURL:net::GURLWithNSURL(
                                  [fileURL URLByDeletingLastPathComponent])
                 browserState:chrome_browser_state_.get()];
  [content setDelegate:delegate];
  [[(OCMockObject*)delegate expect]
       nativeContent:content
      titleDidChange:[OCMArg checkWithBlock:^BOOL(id value) {
        isRunLoopDry = true;
        return [@"Google Chrome Terms of Service"
            isEqualToString:(NSString*)value];
      }]];
  [content triggerPendingLoad];
  DryRunLoop(false);

  TestUrlLoadingService* url_loader =
      (TestUrlLoadingService*)UrlLoadingServiceFactory::GetForBrowserState(
          chrome_browser_state_.get());
  EXPECT_EQ(0, url_loader->load_current_tab_call_count);
  EXPECT_EQ(0, url_loader->load_new_tab_call_count);

  DryRunLoop(true);

  EXPECT_EQ(0, url_loader->load_current_tab_call_count);
  EXPECT_EQ(0, url_loader->load_new_tab_call_count);
  ASSERT_OCMOCK_VERIFY((OCMockObject*)delegate);
}

// Tests that -[StaticHtmlViewController webView] returns a non-nil view.
TEST_F(StaticHtmlViewControllerTest, WebViewNonNil) {
  L10nHtmlGenerator* generator =
      [[L10nHtmlGenerator alloc] initWithMessageId:IDS_IOS_TOOLS_MENU];
  StaticHtmlViewController* staticHtmlViewController =
      [[StaticHtmlViewController alloc]
          initWithGenerator:generator
               browserState:chrome_browser_state_.get()];
  EXPECT_TRUE([staticHtmlViewController webView]);
}

// Tests the generated HTML is localized.
TEST_F(StaticHtmlViewControllerTest, L10NTest) {
  L10nHtmlGenerator* generator =
      [[L10nHtmlGenerator alloc] initWithMessageId:IDS_IOS_TOOLS_MENU];
  StaticHtmlViewController* content = [[StaticHtmlViewController alloc]
      initWithGenerator:generator
           browserState:chrome_browser_state_.get()];
  [content triggerPendingLoad];
  __block id string_in_page = nil;
  base::test::ios::WaitUntilCondition(^bool {
    string_in_page = web::test::ExecuteJavaScript([content webView],
                                                  @"document.body.innerHTML");
    return ![string_in_page isEqual:@""];
  });
  EXPECT_TRUE([string_in_page isKindOfClass:[NSString class]]);
  NSString* to_find = l10n_util::GetNSString(IDS_IOS_TOOLS_MENU);
  EXPECT_TRUE([string_in_page rangeOfString:to_find].length);
}

}  // namespace
