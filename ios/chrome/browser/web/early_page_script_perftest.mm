// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

#include "base/timer/elapsed_timer.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/test/base/perf_test_ios.h"
#import "ios/web/common/web_view_creation_util.h"
#import "ios/web/public/test/js_test_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Class for testing early page script injection into WKWebView.
// TODO(crbug.com/583218): improve this test to use WKUserScript injections.
class EarlyPageScriptPerfTest : public PerfTest {
 protected:
  EarlyPageScriptPerfTest() : PerfTest("Early Page Script for WKWebView") {
    std::unique_ptr<ios::ChromeBrowserState> browser_state =
        TestChromeBrowserState::Builder().Build();
    // |web_view| already has the script injected. |web_view_| is a bare
    // WKWebView, which will be used for script execution testing performance.
    web_view_ = [[WKWebView alloc] init];
    WKWebView* web_view = web::BuildWKWebView(CGRectZero, browser_state.get());
    NSArray* scripts = web_view.configuration.userContentController.userScripts;
    EXPECT_EQ(2U, scripts.count);
    script_ = [scripts.firstObject source];
  }

  // Injects early script into WKWebView.
  void InjectEarlyScript() { web::test::ExecuteJavaScript(web_view_, script_); }

  // WKWebView to test scripts injections.
  WKWebView* web_view_;
  NSString* script_;
};

// Tests injection time into a bare web view.
// TODO(crbug.com/796149): Reenable it.
TEST_F(EarlyPageScriptPerfTest, FLAKY_BareWebViewInjection) {
  RepeatTimedRuns("Bare web view injection",
                  ^base::TimeDelta(int) {
                    base::ElapsedTimer timer;
                    InjectEarlyScript();
                    return timer.Elapsed();
                  },
                  nil);
}

}  // namespace
