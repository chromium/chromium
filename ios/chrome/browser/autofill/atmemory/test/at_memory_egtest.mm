// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/common/autofill_features.h"
#import "ios/chrome/browser/autofill/atmemory/test/at_memory_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/default_handlers.h"

namespace {
constexpr char kMultiFormPageURL[] = "/multi_form_page.html";
const char kNameFieldID[] = "name";

// Loads a page with forms for different data types.
void LoadMultiFormPage(net::test_server::EmbeddedTestServer* test_server) {
  [ChromeEarlGrey loadURL:test_server->GetURL(kMultiFormPageURL)];
  [ChromeEarlGrey waitForWebStateContainingText:"hello!"];
}
}  // namespace

// Test case for the AtMemory screen.
@interface AtMemoryTestCase : ChromeTestCase
@end

@implementation AtMemoryTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.features_enabled.push_back(autofill::features::kAutofillAtMemory);
  return config;
}

- (void)setUp {
  [super setUp];
  RegisterDefaultHandlers(self.testServer);
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
}

// Tests that tapping the magnifying glass spark icon in the keyboard accessory
// shows the AtMemory bottom sheet.
- (void)testShowsAtMemoryBottomSheet {
  // Load a page with form fields.
  LoadMultiFormPage(self.testServer);

  // Focus a text field.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kNameFieldID)];

  // TODO(crbug.com/532090671): add verification after the cleanup.
  // TODO(crbug.com/522340351): Add tests for selection-based filling.
}

@end
