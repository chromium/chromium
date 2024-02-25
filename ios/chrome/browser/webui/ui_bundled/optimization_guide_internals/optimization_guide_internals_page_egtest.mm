// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <string>

#import "base/command_line.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/optimization_guide/core/optimization_guide_switches.h"
#import "components/optimization_guide/optimization_guide_internals/webui/url_constants.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_test_app_interface.h"
#import "ios/chrome/browser/webui/ui_bundled/web_ui_test_utils.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

@interface OptimizationGuideInternalsPageTestCase : ChromeTestCase
@end

@implementation OptimizationGuideInternalsPageTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.additional_args.push_back(
      std::string("--") + optimization_guide::switches::kDebugLoggingEnabled);
  return config;
}

// Tests that chrome://optimization-guide-internals loads when debug logs flag
// is enabled, and that logs get added to #log-message-container on the page.
- (void)testChromeOptimizationGuideInternalsSite {
  GURL url = WebUIPageUrlWithHost(
      optimization_guide_internals::kChromeUIOptimizationGuideInternalsHost);
  [ChromeEarlGrey loadURL:url];

  GREYAssert(WaitForOmniboxURLString(url.spec(), false),
             @"Omnibox did not contain URL.");

  // Validates that some of the expected text on the page exists.
  [ChromeEarlGrey waitForWebStateContainingText:"Optimization Guide Internals"];

  [OptimizationGuideTestAppInterface
      registerOptimizationType:optimization_guide::proto::OptimizationType::
                                   NOSCRIPT];
  [ChromeEarlGrey openNewTab];
  GURL fooURL = GURL("https://foo");
  [ChromeEarlGrey loadURL:fooURL];
  GREYAssert(WaitForOmniboxURLString(fooURL.spec(), false),
             @"Omnibox did not contain URL.");
  // Call `-canApplyOptimization:type:metadata:` for its side-effect of logging
  // to HintsManager. The event logged should then become visible in the WebUI.
  [OptimizationGuideTestAppInterface
      canApplyOptimization:@"https://foo"
                      type:optimization_guide::proto::OptimizationType::NOSCRIPT
                  metadata:nil];

  [ChromeEarlGrey selectTabAtIndex:0];
  // Expect Optimization Guide internals page to have more than two entries of
  // debug log messages.
  [ChromeEarlGrey waitForJavaScriptCondition:
                      @"document.getElementById('log-message-container')."
                      @"children[0].childElementCount > 2"];
}

@end
