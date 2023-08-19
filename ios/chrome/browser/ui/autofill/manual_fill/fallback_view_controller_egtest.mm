// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/autofill_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/element_selector.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "url/gurl.h"

using chrome_test_util::TapWebElementWithId;
using chrome_test_util::ManualFallbackProfilesIconMatcher;

namespace {

constexpr char kFormElementNormal[] = "normal_field";
constexpr char kFormElementReadonly[] = "readonly_field";

constexpr char kFormHTMLFile[] = "/readonly_form.html";

}  // namespace

// Integration Tests for fallback coordinator.
@interface FallbackViewControllerTestCase : ChromeTestCase
@end

@implementation FallbackViewControllerTestCase

- (void)setUp {
  [super setUp];
  [AutofillAppInterface clearProfilesStore];
  [AutofillAppInterface saveExampleProfile];

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL URL = self.testServer->GetURL(kFormHTMLFile);
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:"Hello"];
}

- (void)tearDown {
  [AutofillAppInterface clearProfilesStore];
  [super tearDown];
}

// Tests that readonly fields don't have Manual Fallback icons.
- (void)testReadOnlyFieldDoesNotShowManualFallbackIcons {
  // Tap the readonly field.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementReadonly)];

  // Verify the profiles icon is not visible.
  [[EarlGrey selectElementWithMatcher:ManualFallbackProfilesIconMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that readonly fields don't have Manual Fallback icons after tapping a
// regular field.
- (void)testReadOnlyFieldDoesNotShowManualFallbackIconsAfterNormalField {
  // Tap the regular field.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementNormal)];

  // Verify the profiles icon is visible.
  [[EarlGrey selectElementWithMatcher:ManualFallbackProfilesIconMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap the readonly field.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementReadonly)];

  // Verify the profiles icon is not visible.
  [[EarlGrey selectElementWithMatcher:ManualFallbackProfilesIconMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that normal fields have Manual Fallback icons after tapping a readonly
// field.
- (void)testNormalFieldHasManualFallbackIconsAfterReadonlyField {
  // Tap the readonly field.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementReadonly)];

  // Verify the profiles icon is not visible.
  [[EarlGrey selectElementWithMatcher:ManualFallbackProfilesIconMatcher()]
      assertWithMatcher:grey_notVisible()];

  // Tap the regular field.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementNormal)];

  // Verify the profiles icon is visible.
  [[EarlGrey selectElementWithMatcher:ManualFallbackProfilesIconMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

@end
