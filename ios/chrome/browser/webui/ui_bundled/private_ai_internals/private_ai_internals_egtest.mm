// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "components/private_ai/features.h"
#import "components/webui/chrome_urls/pref_names.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

@interface PrivateAiInternalsTestCase : ChromeTestCase
@end

@implementation PrivateAiInternalsTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(private_ai::kPrivateAi);
  return config;
}

- (void)setUp {
  [super setUp];
  // Enables internal-only UIs.
  [ChromeEarlGrey setBoolValue:YES
             forLocalStatePref:chrome_urls::kInternalOnlyUisEnabled];
}

- (void)tearDownHelper {
  [ChromeEarlGrey
      resetDataForLocalStatePref:chrome_urls::kInternalOnlyUisEnabled];
  [super tearDownHelper];
}

// Tests that chrome://private-ai-internals loads and renders UI elements.
- (void)testChromePrivateAiInternalsSite {
  GURL url = GURL("chrome://private-ai-internals");
  [ChromeEarlGrey loadURL:url];

  // Validates that expected text on the page exists.
  [ChromeEarlGrey waitForWebStateContainingText:"Server URL:"];

  // Click create connection button.
  (void)[ChromeEarlGrey
      evaluateJavaScript:@"document.getElementById('create-connection-button')"
                         @".click()"];

  // Verify disconnect button becomes visible.
  [ChromeEarlGrey
      waitForJavaScriptCondition:@"!document.getElementById('disconnect-button'"
                                 @").classList.contains('hidden')"];
}

@end
