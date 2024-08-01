// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/ios/ios_util.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "components/enterprise/idle/idle_pref_names.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "components/policy/policy_constants.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/policy/model/policy_app_interface.h"
#import "ios/chrome/browser/policy/model/policy_earl_grey_utils.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/policy/ui_bundled/idle/constants.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/confirmation_alert/constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_matchers_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/base_eg_test_helper_impl.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

using chrome_test_util::ButtonWithAccessibilityLabelId;
using policy_test_utils::SetPolicy;

namespace {

// kSnackbarDisappearanceTimeout = MDCSnackbarMessageDurationMax + extra 4
// seconds for avoiding flakiness due to time lags.
constexpr base::TimeDelta kSnackbarDisappearanceTimeout = base::Seconds(10 + 4);

// Returns a matcher for the idle timeout dialog's "Continue using Chrome"
// button.
id<GREYMatcher> GetContinueButton() {
  NSString* buttonTitle =
      l10n_util::GetNSString(IDS_IOS_IDLE_TIMEOUT_CONTINUE_USING_CHROME);
  id<GREYMatcher> matcher =
      grey_allOf(grey_accessibilityLabel(buttonTitle),
                 grey_accessibilityTrait(UIAccessibilityTraitStaticText),
                 grey_sufficientlyVisible(), nil);

  return matcher;
}

// Returns a matcher for the idle timeout confirmation dialog.
id<GREYMatcher> GetIdleTimeoutDialogMatcher() {
  return grey_accessibilityID(kIdleTimeoutDialogAccessibilityIdentifier);
}

// Returns whether the idle timeout dialog is shown.
BOOL IsIdleTimeoutDialogShown() {
  NSError* error = nil;
  [[EarlGrey selectElementWithMatcher:GetIdleTimeoutDialogMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()
                  error:&error];
  return error == nil;
}

// Returns a condition for the dialog shown.
GREYCondition* DialogLoadedCondition() {
  return [GREYCondition
      conditionWithName:@"Wait for the idle timeout dialog to show"
                  block:^BOOL {
                    return IsIdleTimeoutDialogShown();
                  }];
}

// Checks that the idle timeout confirmation dialog is shown after the timeout
// set in the policy.
void VerifyIdleDialogShownOnTimeout() {
  // The modal will only show after 60 seconds (idle timeout policy value) then
  // display for 30 seconds, so the timeout is set until the dialog is
  // dismissed. The polling interval is 10 to give the test 3 chances to check
  // the dialog after it has been displayed to avoid flakiness.
  BOOL modalViewLoaded = [DialogLoadedCondition() waitWithTimeout:90.0
                                                     pollInterval:10.0];
  GREYAssertTrue(modalViewLoaded,
                 @"The confirmation dialog should be shown on idle timeout.");
}

// Returns YES if the idle timeout dialog is fully dismissed by making sure
// that there isn't any dialog displayed.
BOOL IsIdleTimeoutDialogDismissed() {
  NSError* error = nil;
  [[EarlGrey selectElementWithMatcher:GetIdleTimeoutDialogMatcher()]
      assertWithMatcher:grey_nil()
                  error:&error];
  return error == nil;
}

// Checks if the idle timeout dialog is dismissed.
void VerifyIdleTimeoutDialogDismissed() {
  GREYAssertTrue(IsIdleTimeoutDialogDismissed(),
                 @"The confirmation dialog should be dismissed.");
}

// Returns a condition for the modal dismissal.
GREYCondition* ModalViewDismissedCondition() {
  return [GREYCondition
      conditionWithName:@"Wait for the idle timeout dialog to dismiss"
                  block:^BOOL {
                    return IsIdleTimeoutDialogDismissed();
                  }];
}

// Checks that the idle timeout dialog is fully dismissed by making sure
// that there isn't any PolicyAppInterface displayed.
void VerifyIdleTimeoutDialogDismissedOnDialogExpiry() {
  // The dialog is dismissed after 30 seconds, so wait 30 seconds with an
  // additional buffer of 10 seconds to avoid flakiness. Polling interval of 5
  // seconds is chosen to check at t=30, t=35 and t=40.
  BOOL modalViewDismissed = [ModalViewDismissedCondition() waitWithTimeout:40.0
                                                              pollInterval:5.0];
  GREYAssertTrue(
      modalViewDismissed,
      @"The confirmation dialog should be dismissed after 30 seconds.");
}

// Clicks `Continue using Chromium` when the idle timeout dialog is shown.
void WaitForIdleTimeoutScreenAndClickContinue() {
  // Wait and verify that the dialog is shown.
  [ChromeEarlGrey waitForMatcher:GetIdleTimeoutDialogMatcher()];
  [[EarlGrey selectElementWithMatcher:GetContinueButton()]
      performAction:grey_tap()];
}

// Waits to confirm that the snackbar is shown after idle timeout actions run.
void VerifyActionsSnackbarShown(int actions_string_id) {
  id<GREYMatcher> snackbarMatcher = grey_allOf(
      grey_accessibilityID(@"MDCSnackbarMessageTitleAutomationIdentifier"),
      grey_text(l10n_util::GetNSString(actions_string_id)), nil);
  // Wait for the snackbar to appear.
  [ChromeEarlGrey testUIElementAppearanceWithMatcher:snackbarMatcher];
  // Wait for the snackbar to disappear to make sure it is not indefinitely in
  // the view.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:snackbarMatcher
                                     timeout:kSnackbarDisappearanceTimeout];
}

// Verifies that the snackbar does not appear within 5 seconds. The condition is
// expected to timeout and return false.
void VerifySnackbarDoesNotAppear() {
  id<GREYMatcher> snackbarMatcher =
      grey_accessibilityID(@"MDCSnackbarMessageTitleAutomationIdentifier");
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:snackbarMatcher]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    return error == nil;
  };

  bool matched =
      base::test::ios::WaitUntilConditionOrTimeout(base::Seconds(5), condition);
  GREYAssertFalse(matched,
                  @"The snackbar should not appear if actions do not run.");
}

// Verifies that the specified window has the blocking overlay window.
void VerifyActivityOverlayShownInWindowNumber(int window_number) {
  [[EarlGrey
      selectElementWithMatcher:
          chrome_test_util::MatchInBlockerWindowWithNumber(
              window_number,
              ButtonWithAccessibilityLabelId(
                  IDS_IOS_UI_BLOCKED_USE_OTHER_WINDOW_SWITCH_WINDOW_ACTION))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Verifies that the metric for the confirmation dialog appearance has been
// logged as expected.
void VerifyDialogShownMetricsLogged() {
  GREYAssertNil(
      [MetricsAppInterface
           expectCount:1
             forBucket:0
          forHistogram:
              @"Enterprise.IdleTimeoutPolicies.IdleTimeoutDialogEvent"],
      @"IdleTimeoutDialogEvent metrics count was incorrect for "
      @"kDialogShown.");
}

// Verifies that the dialog dismissal metrics have
// been logged correctly based on whether rhe dialog was closed by the user or
// due to expiry.
void VerifyDialogDismissalMetricsLogged(int dismissed_by_user_bucket_count,
                                        int expired_count) {
  GREYAssertNil(
      [MetricsAppInterface
           expectCount:dismissed_by_user_bucket_count
             forBucket:1
          forHistogram:
              @"Enterprise.IdleTimeoutPolicies.IdleTimeoutDialogEvent"],
      @"IdleTimeoutDialogEvent metrics count was incorrect for "
      @"kDialogDismissedByUser.");

  GREYAssertNil(
      [MetricsAppInterface
           expectCount:expired_count
             forBucket:2
          forHistogram:
              @"Enterprise.IdleTimeoutPolicies.IdleTimeoutDialogEvent"],
      @"IdleTimeoutDialogEvent metrics count was incorrect for "
      @"kDialogExpired.");
}

void SignIn() {
  // Sign into a fake identity.
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity1];
  // Verify that the user has been signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity1];
}

// Confirm that the actions have indeed run by checking that the user is
// signed out and all tabs are closed when the dialog times out.
void VerifyActionsRan() {
  [SigninEarlGrey verifySignedOut];
  // The main tab count is 1 if it was backgrounded then foregrounded because a
  // NTP is always opened on reforegrounding.
  GREYAssert([ChromeEarlGrey mainTabCount] <= 1,
             @"All tabs should be closed after idle timeout actions run.");
  GREYAssert([ChromeEarlGrey incognitoTabCount] == 0,
             @"All tabs should be closed after idle timeout actions run.");
}

// Confirm that no actions have run by checking that the user is stil signed
// in and main and incognito tabs are still open.
void VerifyNoActionsRan() {
  [SigninEarlGrey
      verifySignedInWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  GREYAssert([ChromeEarlGrey mainTabCount] > 1,
             @"Tabs should remain opened if no actions run.");
  GREYAssert([ChromeEarlGrey incognitoTabCount] == 1,
             @"Tabs should remain opened if no actions run.");
}

}  // namespace

// Test the idle timeout policy screens .
@interface IdleTimeoutTestCase : ChromeTestCase
@end

@implementation IdleTimeoutTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  // Configure the IdleTimeout and IdleTimeoutActions policies.
  config.additional_args.push_back(
      "-" + base::SysNSStringToUTF8(kPolicyLoaderIOSConfigurationKey));
  config.additional_args.push_back(
      "<dict><key>IdleTimeout</key><integer>1</"
      "integer><key>IdleTimeoutActions</"
      "key><array><string>clear_browsing_history</string><string>sign_out</"
      "string><string>close_tabs</string></array></dict>");
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  return config;
}

- (void)setUp {
  [super setUp];

  GREYAssertNil([MetricsAppInterface setupHistogramTester],
                @"Cannot setup histogram tester.");
  [MetricsAppInterface overrideMetricsAndCrashReportingForTesting];
}

- (void)tearDown {
  [PolicyAppInterface clearPolicies];
  [MetricsAppInterface stopOverridingMetricsAndCrashReportingForTesting];
  GREYAssertNil([MetricsAppInterface releaseHistogramTester],
                @"Cannot reset histogram tester.");

  [super tearDown];
}

#pragma mark - Tests

// Tests that the idle timeout dialog is dismissed if `Continue using Chrome`
// button is clicked. There should be no snackar
// since actions do not run.
- (void)testDialogShownOnIdleTimeoutAndDismissedOnContinue {
  SignIn();
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey openNewIncognitoTab];

  VerifyIdleDialogShownOnTimeout();
  VerifyDialogShownMetricsLogged();

  WaitForIdleTimeoutScreenAndClickContinue();
  VerifyIdleTimeoutDialogDismissed();
  VerifyDialogDismissalMetricsLogged(1, 0);

  VerifySnackbarDoesNotAppear();
  VerifyNoActionsRan();
}

// Tests that the idle timeout dialog is dismissed after 30 seconds if
// `Continue using Chrome` button is not clicked. The snackbar should also be
// shown on dismissal and actions run.
- (void)testDialogDismissedAndActionsRunOnCountdownCompletion {
  SignIn();
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey openNewIncognitoTab];

  VerifyIdleDialogShownOnTimeout();
  VerifyDialogShownMetricsLogged();

  VerifyIdleTimeoutDialogDismissedOnDialogExpiry();
  VerifyDialogDismissalMetricsLogged(0, 1);

  VerifyActionsSnackbarShown(IDS_IOS_IDLE_TIMEOUT_ALL_ACTIONS_SNACKBAR_MESSAGE);
  VerifyActionsRan();
}

// Tests that if the app times out then the app is backgrounded, the actions
// will run on reforeground and a snackbar will be shown.
- (void)testActionsRunAfterBackgroundThenReforeground {
  VerifyIdleDialogShownOnTimeout();
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];
  VerifyIdleTimeoutDialogDismissed();
  VerifyActionsSnackbarShown(IDS_IOS_IDLE_TIMEOUT_ALL_ACTIONS_SNACKBAR_MESSAGE);
  VerifyActionsRan();
}

// Tests that the idle timeout confirmation dialog is shown on the other window
// when the window presenting the dialog is closed.
- (void)testIdleTimeoutDialogWithMultiWindows {
  if (![ChromeEarlGrey areMultipleWindowsSupported]) {
    EARL_GREY_TEST_DISABLED(@"Multiple windows can't be opened.");
  }

  // Wait and verify that the idle timeout dialog is shown on timeout.
  VerifyIdleDialogShownOnTimeout();

  // Open a new window on which the UIBlocker will be shown.
  [ChromeEarlGrey openNewWindow];
  [ChromeEarlGrey waitUntilReadyWindowWithNumber:1];
  [ChromeEarlGrey waitForForegroundWindowCount:2];

  // The new window at index 1 should be blocked by an overlay.
  VerifyActivityOverlayShownInWindowNumber(1);

  // Close the window that is showing the dialog which
  // corresponds to the first window that was opened.
  [ChromeEarlGrey closeWindowWithNumber:0];
  [ChromeEarlGrey waitForForegroundWindowCount:1];

  // Verify that the dialog is still shown on the other window that remains
  // opened.
  GREYAssert(IsIdleTimeoutDialogShown(),
             @"The idle dialog was not shown on the other window after "
             @"displaying window was closed");
}

@end
