// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/web/model/suppress_input_views_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/element_selector.h"
#import "net/test/embedded_test_server/embedded_test_server.h"

namespace {

using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

// Relative URL of the test page containing a text input.
constexpr char kTestPageRelativeURL[] = "/multi_field_form.html";
// ID of the text input element in `kTestPageRelativeURL`.
constexpr char kTextInputID[] = "username";

}  // namespace

// Tests for the `CRWWebViewProxy.shouldSuppressInputViews` API.
@interface SuppressInputViewsTestCase : ChromeTestCase
@end

@implementation SuppressInputViewsTestCase

- (void)setUp {
  [super setUp];
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

// Tests that the software keyboard is displayed normally when
// `shouldSuppressInputViews` defaults to `NO`.
- (void)testInputViewsDisplayedByDefault {
  // TODO(crbug.com/556733164): Add iPad support.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"iPad is currently unsupported.");
  }
  [self loadPageAndFocusInput];
  [ChromeEarlGrey waitForKeyboardToAppear];
}

// Tests that the software keyboard is suppressed when
// `shouldSuppressInputViews` is set to `YES`.
- (void)testInputViewsSuppressedWhenSuppressionEnabled {
  // TODO(crbug.com/556733164): Add iPad support.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"iPad is currently unsupported.");
  }
  [SuppressInputViewsAppInterface setShouldSuppressInputViews:YES];
  [self loadPageAndFocusInput];
  // The keyboard appears some time after the input gains focus, so check that
  // it stays hidden for a while instead of checking once.
  const bool keyboardShown =
      WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
        return [EarlGrey isKeyboardShownWithError:nil];
      });
  GREYAssertFalse(
      keyboardShown,
      @"Keyboard should remain hidden when input views are suppressed.");
}

// Tests that toggling `shouldSuppressInputViews` dynamically updates keyboard
// visibility while preserving input focus.
- (void)testInputViewsToggledDynamically {
  // TODO(crbug.com/556733164): Add iPad support.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"iPad is currently unsupported.");
  }
  [self loadPageAndFocusInput];
  [ChromeEarlGrey waitForKeyboardToAppear];

  [SuppressInputViewsAppInterface setShouldSuppressInputViews:YES];
  [ChromeEarlGrey waitForKeyboardToDisappear];
  [ChromeEarlGrey
      waitForJavaScriptCondition:@"document.activeElement.id === 'username'"];

  [SuppressInputViewsAppInterface setShouldSuppressInputViews:NO];
  [ChromeEarlGrey waitForKeyboardToAppear];
}

#pragma mark - Private

// Loads `kTestPageRelativeURL`, taps the text input to focus it, and waits for
// it to become the active DOM element.
- (void)loadPageAndFocusInput {
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kTestPageRelativeURL)];
  [ChromeEarlGrey
      waitForWebStateContainingElement:[ElementSelector
                                           selectorWithElementID:kTextInputID]];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kTextInputID)];
  [ChromeEarlGrey
      waitForJavaScriptCondition:@"document.activeElement.id === 'username'"];
}

@end
