// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <ChromeWebView/ChromeWebView.h>
#import <Foundation/Foundation.h>

#import "base/test/ios/wait_util.h"
#import "ios/web_view/test/web_view_inttest_base.h"
#import "ios/web_view/test/web_view_test_util.h"
#import "net/base/mac/url_conversions.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest_mac.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CWVFakeScriptCommandHandler : NSObject<CWVScriptCommandHandler>

@property(nonatomic) CWVScriptCommand* lastReceivedCommand;

- (BOOL)webView:(CWVWebView*)webView
    handleScriptCommand:(CWVScriptCommand*)command
          fromMainFrame:(BOOL)fromMainFrame;

@end

@implementation CWVFakeScriptCommandHandler

@synthesize lastReceivedCommand = _lastReceivedCommand;

- (BOOL)webView:(CWVWebView*)webView
    handleScriptCommand:(CWVScriptCommand*)command
          fromMainFrame:(BOOL)fromMainFrame {
  self.lastReceivedCommand = command;
  return YES;
}

@end

namespace ios_web_view {

// Tests the script command feature in CWVWebView.
using WebViewScriptCommandTest = WebViewInttestBase;

// Tests that a handler added by -[CWVWebView
// addScriptCommandHandler:commandPrefix] is invoked by JavaScript.
TEST_F(WebViewScriptCommandTest, TestScriptCommand) {
  ASSERT_TRUE(test_server_->Start());
  CWVFakeScriptCommandHandler* handler =
      [[CWVFakeScriptCommandHandler alloc] init];
  [web_view_ addScriptCommandHandler:handler commandPrefix:@"test"];

  // Uses GetUrlForPageWithHtmlBody() instead of simply using about:blank
  // because it looks __gCrWeb may not be available on about:blank.
  // TODO(crbug.com/836114): Analyze why.
  NSURL* url = net::NSURLWithGURL(GetUrlForPageWithHtmlBody(""));
  ASSERT_TRUE(test::LoadUrl(web_view_, url));
  ASSERT_TRUE(test::WaitForWebViewLoadCompletionOrTimeout(web_view_));

  NSString* script =
      @"__gCrWeb.message.invokeOnHost("
      @"{'command': 'test.command1', 'key1': 'value1', 'key2': 42});";
  NSError* script_error = nil;
  test::EvaluateJavaScript(web_view_, script, &script_error);
  ASSERT_NSEQ(nil, script_error);

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, ^{
        return handler.lastReceivedCommand != nil;
      }));

  EXPECT_NSEQ(@"test.command1",
              handler.lastReceivedCommand.content[@"command"]);
  EXPECT_NSEQ(@"value1", handler.lastReceivedCommand.content[@"key1"]);
  EXPECT_NSEQ(@42, handler.lastReceivedCommand.content[@"key2"]);
  EXPECT_NSEQ(url, handler.lastReceivedCommand.mainDocumentURL);
  EXPECT_FALSE(handler.lastReceivedCommand.userInteracting);

  [web_view_ removeScriptCommandHandlerForCommandPrefix:@"test"];
}

// Tests that added script commands are still valid after state restoration.
// Tests the same thing as TestScriptCommand() after state restoration.
TEST_F(WebViewScriptCommandTest, TestScriptCommandAfterStateRestoration) {
  ASSERT_TRUE(test_server_->Start());
  CWVFakeScriptCommandHandler* handler =
      [[CWVFakeScriptCommandHandler alloc] init];
  [web_view_ addScriptCommandHandler:handler commandPrefix:@"test"];

  CWVWebView* source_web_view = test::CreateWebView();
  test::CopyWebViewState(source_web_view, web_view_);

  // Uses GetUrlForPageWithHtmlBody() instead of simply using about:blank
  // because it looks __gCrWeb may not be available on about:blank.
  // TODO(crbug.com/836114): Analyze why.
  NSURL* url = net::NSURLWithGURL(GetUrlForPageWithHtmlBody(""));
  ASSERT_TRUE(test::LoadUrl(web_view_, url));
  ASSERT_TRUE(test::WaitForWebViewLoadCompletionOrTimeout(web_view_));

  NSString* script =
      @"__gCrWeb.message.invokeOnHost("
      @"{'command': 'test.command1', 'key1': 'value1', 'key2': 42});";
  NSError* script_error = nil;
  test::EvaluateJavaScript(web_view_, script, &script_error);
  ASSERT_NSEQ(nil, script_error);

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, ^{
        return handler.lastReceivedCommand != nil;
      }));

  EXPECT_NSEQ(@"test.command1",
              handler.lastReceivedCommand.content[@"command"]);
  EXPECT_NSEQ(@"value1", handler.lastReceivedCommand.content[@"key1"]);
  EXPECT_NSEQ(@42, handler.lastReceivedCommand.content[@"key2"]);
  EXPECT_NSEQ(url, handler.lastReceivedCommand.mainDocumentURL);
  EXPECT_FALSE(handler.lastReceivedCommand.userInteracting);

  [web_view_ removeScriptCommandHandlerForCommandPrefix:@"test"];
}

}  // namespace ios_web_view
