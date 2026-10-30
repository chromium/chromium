// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/testing/earl_grey/system_alert_handler.h"

#import "base/logging.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/system_alert_handler.h"

@implementation SystemAlertHandlerImpl

- (void)handleSystemAlertIfVisible {
  // Asks the app under test whether SpringBoard is showing an alert. Going
  // through an EarlGrey matcher also synchronizes with the app's idle state,
  // which callers in test `setUp` and `loadURL:` rely on; see b/579116520.
  // This is not guaranteed to report alerts raised by a previous app process;
  // see `-handleLeftoverSystemAlertIfVisible`.
  NSError* systemAlertFoundError = nil;
  [[EarlGrey selectElementWithMatcher:grey_systemAlertViewShown()]
      assertWithMatcher:grey_nil()
                  error:&systemAlertFoundError];
  if (!systemAlertFoundError) {
    return;
  }
  [self handleVisibleSystemAlert];
}

- (void)handleLeftoverSystemAlertIfVisible {
  // Query SpringBoard directly rather than the app under test: a prompt raised
  // by a previous app process (e.g. a test that failed mid-way) may not be
  // reported by `-[UIApplication _isSpringBoardShowingAnAlert]` in the new
  // process, whereas this query sees the same hierarchy the dismissal code
  // operates on. Snapshotting SpringBoard is noticeably more expensive than
  // the in-process check, so this is only meant to be called once per app
  // launch.
  XCUIApplication* springboardApp = [[XCUIApplication alloc]
      initWithBundleIdentifier:@"com.apple.springboard"];
  if (!springboardApp.alerts.firstMatch.exists) {
    return;
  }
  [self handleVisibleSystemAlert];
}

#pragma mark - Private

// Accepts or dismisses the currently visible system alert, failing the test if
// it is unknown or cannot be dismissed.
- (void)handleVisibleSystemAlert {
  NSError* alertGetTextError = nil;
  NSString* alertText = [EarlGrey SystemAlertTextWithError:&alertGetTextError];
  GREYAssertNil(alertGetTextError, @"Error getting alert text.\n%@",
                alertGetTextError);

  @try {
    // Calling this method has for side-effect to throw an
    // NSInternalInconsistencyException if the system alert is unknown to the
    // EG2 framework.
    // If it does throw, handle the system alert in @catch.
    // If it doesn’t throw, accept it here.
    // TODO(crbug.com/40127610): Style guide does not allow throwing
    // exceptions.
    [EarlGrey SystemAlertType];

    DLOG(WARNING) << "Accepting iOS system alert: "
                  << base::SysNSStringToUTF8(alertText);

    NSError* acceptAlertError = nil;
    [EarlGrey AcceptSystemDialogWithError:&acceptAlertError];
    GREYAssertNil(acceptAlertError, @"Error accepting system alert.\n%@",
                  acceptAlertError);

  } @catch (NSException* exception) {
    GREYAssert((exception.name == NSInternalInconsistencyException &&
                [exception.reason containsString:@"Invalid System Alert"]),
               @"Unknown error caught when handling unknown system alert: %@",
               exception.reason);

    // Manually handle EG2-unsupported alerts for some known cases, otherwise
    // fail the test.
    [self handleSystemAlertUnsupportedByEG2:alertText];
  }

  // Ensures no visible alert after handling.
  [EarlGrey WaitForAlertVisibility:NO
                       withTimeout:kSystemAlertVisibilityTimeout];
}

// Tries to dismiss a system alert if it matches a list of known system alerts.
// alertText (NSString*) is the text of the system alert currently being shown.
// If the button cannot be tapped (system alert cannot be dismissed), or the
// text does not match a known text/button pair, the test will fail.
- (void)handleSystemAlertUnsupportedByEG2:(NSString*)alertText {
  NSDictionary<NSString*, NSArray<NSString*>*>* textToButtons =
      TextToButtonsOfKnownSystemAlerts();
  for (NSString* text in textToButtons) {
    if ([alertText containsString:text]) {
      DLOG(WARNING) << "Dismissing iOS system alert with label: " << text;
      NSError* error;

      // Try every wanted button label possibility for that text before checking
      // for an error. Some button labels share an alert text.
      for (NSString* button in textToButtons[text]) {
        error = nil;
        [self tapAlertButtonWithText:button error:&error];

        // Break out of button labels loop if the current button worked.
        if (error == nil) {
          break;
        }
      }

      GREYAssertNil(error, @"Error dismissing iOS alert with label: %@\n%@",
                    text, error);

      // For the case where a second alert will appear in succession, wait for
      // it, and then handle it.
      if ([alertText isEqualToString:@"Software Update"]) {
        [EarlGrey WaitForAlertVisibility:YES
                             withTimeout:kSystemAlertVisibilityTimeout];
        [self handleSystemAlertUnsupportedByEG2:alertText];
      }

      return;
    }
  }

  XCTFail("An unsupported system alert is present on device. Failing the test. "
          "Alert label: %@",
          alertText);
}

// Taps button with `text` in the system alert on screen. If an alert or the
// button doesn't exist, note it in `error` accordingly.
- (void)tapAlertButtonWithText:(NSString*)text error:(NSError**)error {
  XCUIApplication* springboardApp = [[XCUIApplication alloc]
      initWithBundleIdentifier:@"com.apple.springboard"];
  XCUIElement* alert = springboardApp.alerts.firstMatch;
  if (![alert waitForExistenceWithTimeout:kSystemAlertVisibilityTimeout]) {
    *error = [NSError errorWithDomain:kGREYSystemAlertDismissalErrorDomain
                                 code:GREYSystemAlertNotPresent
                             userInfo:nil];
    return;
  }
  XCUIElement* button = alert.buttons[text].firstMatch;
  if (![button exists]) {
    *error = [NSError errorWithDomain:kGREYSystemAlertDismissalErrorDomain
                                 code:GREYSystemAlertCustomButtonNotFound
                             userInfo:nil];
    return;
  }

  [button tap];
}

@end
