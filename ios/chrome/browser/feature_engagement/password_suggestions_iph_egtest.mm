// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/notreached.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/feature_engagement/feature_engagement_app_interface.h"
#import "ios/chrome/browser/passwords/password_manager_app_interface.h"
#import "ios/chrome/browser/ui/bubble/bubble_features.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/base/mac/url_conversions.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

// URL path for a page with password field form.
constexpr char kPasswordForm[] = "/username_password_field_form.html";

// Element ID for the username field in the password form.
constexpr char kPasswordFormUsername[] = "username";

// Matcher for the PasswordSuggestions tip text.
id<GREYMatcher> TipText() {
  return grey_accessibilityID(@"BubbleViewLabelIdentifier");
}

// Matcher for the PasswordSuggestions tip title.
id<GREYMatcher> TipTitle() {
  return grey_accessibilityID(@"BubbleViewTitleLabelIdentifier");
}

// Matcher for the PasswordSuggestions tip close button.
id<GREYMatcher> TipCloseButton() {
  return grey_accessibilityID(@"BubbleViewCloseButtonIdentifier");
}

// Matcher for the PasswordSuggestions tip snooze button.
id<GREYMatcher> TipSnoozeButton() {
  return grey_accessibilityID(@"kBubbleViewSnoozeButtonIdentifier");
}

}  // namespace

// Tests related to the triggering of Autofill Rich IPHs.
// TODO(crbug.com/1338585): Remove the ZZZ prefix once the bug is fixed, where
// the parent class needs to be called last.
@interface ZZZPasswordSuggestionsIPHTestCase : ChromeTestCase {
  // The variant of the feature to use. This is consumed in
  // -appConfigurationForTestCase, as part of -setUp. Subclasses should set this
  // before calling the parent class -setUp.
  std::string _variant;
}

// Verifies that the tip has the required UI affordances. This can be overriden
// by variant subclasses to check for additional affordances.
- (void)verifyTip;

// Dismisses the tip. By default, this is done by dismissing the keyboard.
// This can be overriden by variant subclasses to test different methods of
// dismissal.
- (void)dismissTip;

@end

@implementation ZZZPasswordSuggestionsIPHTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];

  config.additional_args.push_back(
      "--enable-features=" + std::string(kBubbleRichIPH.name) + "<" +
      std::string(kBubbleRichIPH.name));

  config.additional_args.push_back(
      "--force-fieldtrials=" + std::string(kBubbleRichIPH.name) + "/Test");

  config.additional_args.push_back(
      "--force-fieldtrial-params=" + std::string(kBubbleRichIPH.name) +
      ".Test:" + std::string(kBubbleRichIPHParameterName) + "/" + _variant);

  return config;
}

- (void)tearDown {
  [FeatureEngagementAppInterface reset];
  [PasswordManagerAppInterface clearCredentials];
  [super tearDown];
}

#pragma mark - Public

- (void)verifyTip {
  // No-op. Subclasses should override this.
}

- (void)dismissTip {
  // No-op. Subclasses should override this.
}

#pragma mark - Helpers

// Steps to add username/password for the test form.
- (void)addCredentialsToAutofill {
  GREYAssert(
      [FeatureEngagementAppInterface enablePasswordSuggestionsTipTriggering],
      @"Feature Engagement tracker did not load");
  self.testServer->AddDefaultHandlers();
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start");

  // Save the password.
  NSURL* URL = net::NSURLWithGURL(self.testServer->GetURL(kPasswordForm));
  [PasswordManagerAppInterface storeCredentialWithUsername:@"EgUsername"
                                                  password:@"EgPassword"
                                                       URL:URL];
  int credentialsCount = [PasswordManagerAppInterface storedCredentialsCount];
  GREYAssertEqual(1, credentialsCount, @"Wrong number of stored credentials.");
}

// Steps to open the form page where the username password fields are present
// and Autofill has matching entries.
- (void)openForm {
  [ChromeEarlGreyUI openNewTab];
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPasswordForm)];
}

// Steps to select the username field. As Autofill has matching entries, this
// executes the IPH triggering code path. (It does not mean that the IPH will
// appear, as the Feature Engagement tracker will decide to show or not.)
- (void)selectUsernameField {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(
                        kPasswordFormUsername)];
}

// Verifies that the tip is not visible.
- (void)verifyTipNotVisible {
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:TipText()]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    return error == nil;
  };
  GREYAssert(!WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, condition),
             @"The password suggestion tip shouldn't appear");
}

#pragma mark - Tests

// Verifies that the password suggestion tip is displayed only the first time
// password suggestions are shown.
- (void)testPasswordSuggestionsTipAppearsOnceIfClosed {
  if (_variant.length() == 0) {
    EARL_GREY_TEST_SKIPPED(@"Only test children classes with a variant.");
  }

  [self addCredentialsToAutofill];
  [self openForm];
  [self selectUsernameField];

  // Verify that the tip is matching the expectations of the variant.
  [self verifyTip];

  // Dismiss the tip. This can be different per variant.
  [self dismissTip];
  [self verifyTipNotVisible];

  // Dismiss the keyboard.
  NSError* error = nil;
  GREYAssert([EarlGrey dismissKeyboardWithError:&error] && error == nil,
             @"Cannot dismiss the keyboard");

  // Second time, the tip should no longer trigger.
  [self openForm];
  [self selectUsernameField];

  [self verifyTipNotVisible];
}

@end

// Tests related to the triggering of Autofill Rich IPHs for the Target
// Highlight variant.
@interface TargetHighlightTestCase : ZZZPasswordSuggestionsIPHTestCase
@end

@implementation TargetHighlightTestCase

- (void)setUp {
  _variant = std::string(kBubbleRichIPHParameterTargetHighlight);
  [super setUp];
}

- (void)verifyTip {
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:TipText()];
}

- (void)dismissTip {
  // Check that tapping on the tip dismisses it.
  [[EarlGrey selectElementWithMatcher:TipText()] performAction:grey_tap()];
}

// This is currently needed to prevent this test case from being ignored.
- (void)testEmpty {
}

@end

// Tests related to the triggering of Autofill Rich IPHs for the Explicit
// Dismissal variant.
@interface ExplicitDismissalTestCase : ZZZPasswordSuggestionsIPHTestCase
@end

@implementation ExplicitDismissalTestCase

- (void)setUp {
  _variant = std::string(kBubbleRichIPHParameterExplicitDismissal);
  [super setUp];
}

- (void)verifyTip {
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:TipText()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:TipCloseButton()];
}

- (void)dismissTip {
  // Check that tapping anywhere outside the tip dismisses it.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(
                        kPasswordFormUsername)];
}

// This is currently needed to prevent this test case from being ignored.
- (void)testEmpty {
}

@end

// Tests related to the triggering of Autofill Rich IPHs for the Rich IPH
// variant.
@interface RichIPHTestCase : ZZZPasswordSuggestionsIPHTestCase
@end

@implementation RichIPHTestCase

- (void)setUp {
  _variant = std::string(kBubbleRichIPHParameterRich);
  [super setUp];
}

- (void)verifyTip {
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:TipTitle()];
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:TipText()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:TipCloseButton()];
}

- (void)dismissTip {
  // Check that tapping the Close button diesmisses the tip.
  [[EarlGrey selectElementWithMatcher:TipCloseButton()]
      performAction:grey_tap()];
}

// This is currently needed to prevent this test case from being ignored.
- (void)testEmpty {
}

@end

// Tests related to the triggering of Autofill Rich IPHs for the Rich IPH with
// Snooze variant.
@interface RichIPHWithSnoozeTestCase : ZZZPasswordSuggestionsIPHTestCase
@end

@implementation RichIPHWithSnoozeTestCase

- (void)setUp {
  _variant = std::string(kBubbleRichIPHParameterRichWithSnooze);
  [super setUp];
}

- (void)verifyTip {
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:TipTitle()];
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:TipText()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:TipCloseButton()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:TipSnoozeButton()];
}

- (void)dismissTip {
  [[EarlGrey selectElementWithMatcher:TipSnoozeButton()]
      performAction:grey_tap()];
}

// This is currently needed to prevent this test case from being ignored.
- (void)testEmpty {
}

@end
