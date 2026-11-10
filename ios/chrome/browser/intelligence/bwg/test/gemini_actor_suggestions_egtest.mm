// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "base/test/ios/wait_util.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/bwg/test/gemini_actuation_worklog_app_interface.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/common/ui/confirmation_alert/constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_coordinator_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

namespace {

using base::test::ios::kWaitForUIElementTimeout;

NSString* const kUser1 = @"user1@example.com";
NSString* const kUser2 = @"user2@example.com";

// Returns the matcher for the actuation worklog accessory view displaying
// `username`.
id<GREYMatcher> WorklogAccessoryViewWithUsername(NSString* username) {
  return grey_allOf(grey_kindOfClassName(@"ActuationWorklogAccessoryView"),
                    grey_accessibilityLabel(
                        [NSString stringWithFormat:@"%@, ••••••••", username]),
                    grey_sufficientlyVisible(), nil);
}

// Returns the matcher for a suggestion cell in the bottom sheet.
id<GREYMatcher> SuggestionCell(NSString* username) {
  return grey_allOf(
      grey_kindOfClass([UITableViewCell class]), grey_accessibilityID(username),
      grey_ancestor(grey_accessibilityID(
          kConfirmationAlertUnderTitleViewAccessibilityIdentifier)),
      nil);
}

// Returns the matcher for a selected suggestion cell in the bottom sheet.
id<GREYMatcher> SelectedSuggestionCell(NSString* username) {
  return grey_allOf(SuggestionCell(username),
                    grey_accessibilityTrait(UIAccessibilityTraitSelected), nil);
}

// Returns the matcher for the bottom sheet's "Use Password" primary button.
id<GREYMatcher> UsePasswordButton() {
  return grey_allOf(chrome_test_util::ButtonWithAccessibilityLabelId(
                        IDS_IOS_CREDENTIAL_BOTTOM_SHEET_USE_PASSWORD),
                    grey_sufficientlyVisible(), nil);
}

// Returns the matcher for the "Allow" intervention button.
id<GREYMatcher> AllowButton() {
  return chrome_test_util::ButtonWithAccessibilityLabelId(
      IDS_IOS_ACTOR_WORKLOG_SUGGESTIONS_ALLOW_BUTTON);
}

// Returns the matcher for the "Enter password manually" intervention button.
id<GREYMatcher> EnterPasswordManuallyButton() {
  return chrome_test_util::ButtonWithAccessibilityLabelId(
      IDS_IOS_ACTOR_WORKLOG_SUGGESTIONS_CREDENTIAL_MANUAL_FILL_BUTTON);
}

}  // namespace

// Tests for Actor suggestion selection and intervention confirmation.
@interface GeminiActorSuggestionsTestCase : ChromeTestCase
@end

@implementation GeminiActorSuggestionsTestCase

- (BOOL)loadMinimalAppUI {
  return YES;
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.features_enabled.push_back(kActorTools);
  return config;
}

- (void)setUp {
  [super setUp];
  [ChromeCoordinatorAppInterface startBrowser];
  [GeminiActuationWorklogAppInterface
      startActuationWorklogCoordinatorOnMinimalUI];
}

- (void)tearDownHelper {
  [GeminiActuationWorklogAppInterface
      stopActuationWorklogCoordinatorOnMinimalUI];
  [ChromeCoordinatorAppInterface reset];
  [super tearDownHelper];
}

// Tests that dismissing the bottom sheet without selecting a new credential
// retains the initial selection.
- (void)testDismissBottomSheetKeepsCurrentSelection {
  __block NSString* selectedUsername = nil;
  __block BOOL interventionCompleted = NO;
  [GeminiActuationWorklogAppInterface
      startWorklogTaskWithCredentialSuggestions:@[ kUser1, kUser2 ]
                              completionHandler:^(NSString* username) {
                                selectedUsername = username;
                                interventionCompleted = YES;
                              }
                                     nextAction:nil];

  // Tap the worklog accessory view to open the suggestions picker.
  [[EarlGrey selectElementWithMatcher:WorklogAccessoryViewWithUsername(kUser1)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SelectedSuggestionCell(kUser1)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap on user 2, and then cancel / dismiss the bottom sheet and verify
  // selection remains user1 in the worklog.
  [[EarlGrey selectElementWithMatcher:SuggestionCell(kUser2)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::CancelButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:WorklogAccessoryViewWithUsername(kUser1)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap "Allow" to confirm the intervention. Verify that bottom buttons are
  // dismissed.
  [[EarlGrey selectElementWithMatcher:AllowButton()] performAction:grey_tap()];
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:AllowButton()];
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:EnterPasswordManuallyButton()];

  // Verify that user 1 is selected.
  BOOL completed = [[GREYCondition
      conditionWithName:@"Wait for intervention completion"
                  block:^BOOL {
                    return interventionCompleted;
                  }] waitWithTimeout:kWaitForUIElementTimeout.InSecondsF()];
  GREYAssertTrue(completed, @"Intervention timed out.");
  GREYAssertEqualObjects(selectedUsername, kUser1,
                         @"Expected user1 to remain selected.");
}

// Tests that selecting a different credential from the bottom sheet updates
// the worklog in place and completes the intervention when "Allow" is tapped.
- (void)testSelectDifferentSuggestionAndAllow {
  __block NSString* selectedUsername = nil;
  __block BOOL interventionCompleted = NO;
  [GeminiActuationWorklogAppInterface
      startWorklogTaskWithCredentialSuggestions:@[ kUser1, kUser2 ]
                              completionHandler:^(NSString* username) {
                                selectedUsername = username;
                                interventionCompleted = YES;
                              }
                                     nextAction:nil];

  // Verify initial selection in the worklog is user1, tap the accessory view
  // to open the suggestions picker, and select user2.
  [[EarlGrey selectElementWithMatcher:WorklogAccessoryViewWithUsername(kUser1)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SelectedSuggestionCell(kUser1)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:SuggestionCell(kUser2)]
      performAction:grey_tap()];

  // Confirm selection by tapping "Use Password". Verify the selected suggestion
  // is updated in the worklog and remains selected when reopening the picker.
  [[EarlGrey selectElementWithMatcher:UsePasswordButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:WorklogAccessoryViewWithUsername(kUser2)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SelectedSuggestionCell(kUser2)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::CancelButton()]
      performAction:grey_tap()];

  // Tap "Allow" to confirm the intervention and verify intervention buttons are
  // dismissed.
  [[EarlGrey selectElementWithMatcher:AllowButton()] performAction:grey_tap()];
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:AllowButton()];
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:EnterPasswordManuallyButton()];

  // Verify that user 2 is selected.
  BOOL completed = [[GREYCondition
      conditionWithName:@"Wait for intervention completion"
                  block:^BOOL {
                    return interventionCompleted;
                  }] waitWithTimeout:kWaitForUIElementTimeout.InSecondsF()];
  GREYAssertTrue(completed, @"Intervention timed out.");
  GREYAssertEqualObjects(selectedUsername, kUser2,
                         @"Expected user2 to be selected.");
}

// Tests that tapping "Enter password manually" completes the intervention
// without selecting credentials.
- (void)testEnterPasswordManually {
  __block NSString* selectedUsername = nil;
  __block BOOL interventionCompleted = NO;
  [GeminiActuationWorklogAppInterface
      startWorklogTaskWithCredentialSuggestions:@[ kUser1, kUser2 ]
                              completionHandler:^(NSString* username) {
                                selectedUsername = username;
                                interventionCompleted = YES;
                              }
                                     nextAction:nil];

  // Tap the manual entry button and verify intervention buttons are dismissed.
  [[EarlGrey selectElementWithMatcher:EnterPasswordManuallyButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:AllowButton()];
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:EnterPasswordManuallyButton()];

  // Verify that no credential has been selected.
  BOOL completed = [[GREYCondition
      conditionWithName:@"Wait for intervention completion"
                  block:^BOOL {
                    return interventionCompleted;
                  }] waitWithTimeout:kWaitForUIElementTimeout.InSecondsF()];
  GREYAssertTrue(completed, @"Intervention timed out.");
  GREYAssertNil(selectedUsername,
                @"Expected nil username for manual password entry.");
}

// Tests that stopping the task while the bottom sheet is open dismisses the
// bottom sheet and worklog UI without completing the intervention, and that
// starting a new task resets selection to the first credential.
- (void)testStopTaskWithBottomSheetOpenAndStartNewTask {
  __block NSString* selectedUsername = nil;
  __block BOOL interventionCompleted = NO;
  [GeminiActuationWorklogAppInterface
      startWorklogTaskWithCredentialSuggestions:@[ kUser1, kUser2 ]
                              completionHandler:^(NSString* username) {
                                selectedUsername = username;
                                interventionCompleted = YES;
                              }
                                     nextAction:nil];

  // Select user2, then reopen the bottom sheet so it remains presented when the
  // task stops.
  [[EarlGrey selectElementWithMatcher:WorklogAccessoryViewWithUsername(kUser1)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SelectedSuggestionCell(kUser1)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:SuggestionCell(kUser2)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:UsePasswordButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:WorklogAccessoryViewWithUsername(kUser2)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SelectedSuggestionCell(kUser2)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Stop the task while the bottom sheet is open and verify no task remains
  // active.
  [GeminiActuationWorklogAppInterface stopActiveTask];
  BOOL taskStopped = [[GREYCondition
      conditionWithName:@"Wait for active task to stop"
                  block:^BOOL {
                    return ![GeminiActuationWorklogAppInterface hasActiveTask];
                  }] waitWithTimeout:kWaitForUIElementTimeout.InSecondsF()];
  GREYAssertTrue(taskStopped, @"Expected no active task after stopping.");
  [GeminiActuationWorklogAppInterface stopObservingWorklogTask];

  // Verify the bottom sheet, worklog accessory, and intervention buttons are
  // dismissed without completing the intervention.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:SuggestionCell(kUser2)];
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:WorklogAccessoryViewWithUsername(
                                                 kUser2)];
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:AllowButton()];
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:EnterPasswordManuallyButton()];
  GREYAssertFalse(interventionCompleted,
                  @"Intervention should not complete when the task stops.");
  GREYAssertNil(selectedUsername, @"Expected nil username.");

  // Start a new task and verify selection defaults back to user1.
  [GeminiActuationWorklogAppInterface
      startWorklogTaskWithCredentialSuggestions:@[ kUser1, kUser2 ]
                              completionHandler:^(NSString* username) {
                                selectedUsername = username;
                                interventionCompleted = YES;
                              }
                                     nextAction:nil];

  [[EarlGrey selectElementWithMatcher:WorklogAccessoryViewWithUsername(kUser1)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:AllowButton()] performAction:grey_tap()];

  BOOL completed = [[GREYCondition
      conditionWithName:@"Wait for new task intervention completion"
                  block:^BOOL {
                    return interventionCompleted;
                  }] waitWithTimeout:kWaitForUIElementTimeout.InSecondsF()];
  GREYAssertTrue(completed, @"New task intervention timed out.");
  GREYAssertEqualObjects(selectedUsername, kUser1,
                         @"Expected new task to select user1.");
}

// Tests that in expanded mode, a resolved suggestion accessory is no longer
// tappable after the intervention completes and the task moves on to the next
// step.
- (void)testResolvedSuggestionAccessoryNotTappableInExpandedMode {
  [GeminiActuationWorklogAppInterface setWorklogDisplayModeExpanded];

  // Make sure a new action appears in the worklog after the user has allowed
  // credential selection.
  optimization_guide::proto::Action waitAction;
  waitAction.mutable_wait()->set_wait_time_ms(0);
  std::string serializedAction = waitAction.SerializeAsString();
  NSData* actionData = [NSData dataWithBytes:serializedAction.data()
                                      length:serializedAction.length()];

  __block NSString* selectedUsername = nil;
  __block BOOL interventionCompleted = NO;
  [GeminiActuationWorklogAppInterface
      startWorklogTaskWithCredentialSuggestions:@[ kUser1, kUser2 ]
                              completionHandler:^(NSString* username) {
                                selectedUsername = username;
                                interventionCompleted = YES;
                              }
                                     nextAction:actionData];

  // Resolve the suggestion intervention by tapping "Allow".
  [[EarlGrey selectElementWithMatcher:WorklogAccessoryViewWithUsername(kUser1)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:AllowButton()] performAction:grey_tap()];

  BOOL completed = [[GREYCondition
      conditionWithName:@"Wait for intervention completion"
                  block:^BOOL {
                    return interventionCompleted;
                  }] waitWithTimeout:kWaitForUIElementTimeout.InSecondsF()];
  GREYAssertTrue(completed, @"Intervention timed out.");
  GREYAssertEqualObjects(selectedUsername, kUser1,
                         @"Expected user1 to be selected.");

  // Verify the next step is displayed and tapping the earlier resolved
  // accessory in expanded mode does not open the bottom sheet. Note that there
  // are two instances of "Next step" chips (one invisible) and we only want to
  // select the visible one.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_text(@"Next step"),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:WorklogAccessoryViewWithUsername(kUser1)]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:SuggestionCell(kUser1)];
}

// TODO(crbug.com/567040651): Test task pausing.

@end
