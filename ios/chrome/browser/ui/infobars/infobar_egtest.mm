// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_constants.h"
#import "ios/chrome/browser/ui/infobars/infobar_constants.h"
#import "ios/chrome/browser/ui/infobars/infobar_manager_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "url/gurl.h"
#import "url/url_constants.h"

using base::test::ios::WaitUntilConditionOrTimeout;

namespace {

// Verifies that a single TestInfoBar with `message` is either present or absent
// on the current tab.
void VerifyTestInfoBarVisibleForCurrentTab(bool visible, NSString* message) {
  NSString* condition_name =
      visible ? @"Waiting for infobar to show" : @"Waiting for infobar to hide";
  id<GREYMatcher> expected_visibility = visible ? grey_notNil() : grey_nil();

  // After `kInfobarBannerDefaultPresentationDurationInSeconds` seconds the
  // banner should disappear. Includes `kWaitForUIElementTimeout` for EG
  // synchronization.
  constexpr base::TimeDelta kDelay = kInfobarBannerDefaultPresentationDuration +
                                     base::test::ios::kWaitForUIElementTimeout;
  const BOOL banner_shown =
      WaitUntilConditionOrTimeout(kDelay, ^{
        NSError* error = nil;
        [[EarlGrey selectElementWithMatcher:
                       grey_allOf(grey_accessibilityID(
                                      kInfobarBannerLabelsStackViewIdentifier),
                                  grey_accessibilityLabel(message), nil)]
            assertWithMatcher:expected_visibility
                        error:&error];
        return error == nil;
      });

  GREYAssertTrue(banner_shown, condition_name);
}

}  // namespace

// Tests functionality related to infobars.
@interface InfobarTestCase : ChromeTestCase
@end

@implementation InfobarTestCase

- (void)setUp {
  [super setUp];
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
}

// Tests that page infobars don't persist on navigation.
- (void)testInfobarsDismissOnNavigate {
  // Open a new tab and navigate to the test page.
  const GURL testURL = self.testServer->GetURL("/pony.html");
  [ChromeEarlGrey loadURL:testURL];
  [ChromeEarlGrey waitForMainTabCount:1];

  // Infobar Message
  NSString* infoBarMessage = @"TestInfoBar";

  // Add a test infobar to the current tab. Verify that the infobar is present
  // in the model and that the infobar view is visible on screen.
  GREYAssertTrue([InfobarManagerAppInterface
                     addTestInfoBarToCurrentTabWithMessage:infoBarMessage],
                 @"Failed to add infobar to test tab.");
  VerifyTestInfoBarVisibleForCurrentTab(true, infoBarMessage);
  GREYAssertTrue([InfobarManagerAppInterface verifyInfobarCount:1],
                 @"Incorrect number of infobars.");

  // Navigate to a different page.  Verify that the infobar is dismissed and no
  // longer visible on screen.
  [ChromeEarlGrey loadURL:GURL(url::kAboutBlankURL)];
  VerifyTestInfoBarVisibleForCurrentTab(false, infoBarMessage);
  GREYAssertTrue([InfobarManagerAppInterface verifyInfobarCount:0],
                 @"Incorrect number of infobars.");
}

// Tests that page infobars persist only on the tabs they are opened on, and
// that navigation in other tabs doesn't affect them.
- (void)testInfobarTabSwitch {
  const GURL destinationURL = self.testServer->GetURL("/destination.html");
  const GURL ponyURL = self.testServer->GetURL("/pony.html");

  // Create the first tab and navigate to the test page.
  [ChromeEarlGrey loadURL:destinationURL];
  [ChromeEarlGrey waitForMainTabCount:1];

  // Infobar Message
  NSString* infoBarMessage = @"TestInfoBar";

  // Create the second tab, navigate to the test page, and add the test infobar.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:ponyURL];
  [ChromeEarlGrey waitForMainTabCount:2];
  VerifyTestInfoBarVisibleForCurrentTab(false, infoBarMessage);
  GREYAssertTrue([InfobarManagerAppInterface verifyInfobarCount:0],
                 @"Incorrect number of infobars.");
  GREYAssertTrue([InfobarManagerAppInterface
                     addTestInfoBarToCurrentTabWithMessage:infoBarMessage],
                 @"Failed to add infobar to second tab.");
  VerifyTestInfoBarVisibleForCurrentTab(true, infoBarMessage);
  GREYAssertTrue([InfobarManagerAppInterface verifyInfobarCount:1],
                 @"Incorrect number of infobars.");

  // Switch back to the first tab and make sure no infobar is visible.
  [ChromeEarlGrey selectTabAtIndex:0U];
  VerifyTestInfoBarVisibleForCurrentTab(false, infoBarMessage);
  GREYAssertTrue([InfobarManagerAppInterface verifyInfobarCount:0],
                 @"Incorrect number of infobars.");

  // Navigate to a different URL in the first tab, to verify that this
  // navigation does not hide the infobar in the second tab.
  [ChromeEarlGrey loadURL:ponyURL];

  // Close the first tab.  Verify that there is only one tab remaining.
  [ChromeEarlGrey closeCurrentTab];
  [ChromeEarlGrey waitForMainTabCount:1];
}

// Tests that the Infobar dissapears once the "OK" button is tapped.
- (void)testInfobarButtonDismissal {
  // Open a new tab and navigate to the test page.
  const GURL testURL = self.testServer->GetURL("/pony.html");
  [ChromeEarlGrey loadURL:testURL];
  [ChromeEarlGrey waitForMainTabCount:1];

  // Infobar Message
  NSString* infoBarMessage = @"TestInfoBar";

  // Add a test infobar to the current tab. Verify that the infobar is present
  // in the model and that the infobar view is visible on screen.
  GREYAssertTrue([InfobarManagerAppInterface
                     addTestInfoBarToCurrentTabWithMessage:infoBarMessage],
                 @"Failed to add infobar to test tab.");
  VerifyTestInfoBarVisibleForCurrentTab(true, infoBarMessage);
  GREYAssertTrue([InfobarManagerAppInterface verifyInfobarCount:1],
                 @"Incorrect number of infobars.");

  // Tap on "OK" which should dismiss the Infobar.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_buttonTitle(@"OK"),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  VerifyTestInfoBarVisibleForCurrentTab(false, infoBarMessage);
  GREYAssertTrue([InfobarManagerAppInterface verifyInfobarCount:0],
                 @"Incorrect number of infobars.");
}

// Tests adding an Infobar on top of an existing one.
- (void)testInfobarTopMostVisible {
  // Open a new tab and navigate to the test page.
  const GURL testURL = self.testServer->GetURL("/pony.html");
  [ChromeEarlGrey loadURL:testURL];
  [ChromeEarlGrey waitForMainTabCount:1];

  // First Infobar Message
  NSString* firstInfoBarMessage = @"TestFirstInfoBar";

  // Add a test infobar to the current tab. Verify that the infobar is present
  // in the model and that the infobar view is visible on screen.
  GREYAssertTrue([InfobarManagerAppInterface
                     addTestInfoBarToCurrentTabWithMessage:firstInfoBarMessage],
                 @"Failed to add infobar to test tab.");
  VerifyTestInfoBarVisibleForCurrentTab(true, firstInfoBarMessage);
  GREYAssertTrue([InfobarManagerAppInterface verifyInfobarCount:1],
                 @"Incorrect number of infobars.");

  // Second Infobar Message
  NSString* secondInfoBarMessage = @"TestSecondInfoBar";

  // Add a second test infobar to the current tab. Verify that the infobar is
  // present in the model, and that only this second infobar is now visible on
  // screen.
  GREYAssertTrue(
      [InfobarManagerAppInterface
          addTestInfoBarToCurrentTabWithMessage:secondInfoBarMessage],
      @"Failed to add infobar to test tab.");
  GREYAssertTrue([InfobarManagerAppInterface verifyInfobarCount:2],
                 @"Incorrect number of infobars.");
  VerifyTestInfoBarVisibleForCurrentTab(false, firstInfoBarMessage);
  VerifyTestInfoBarVisibleForCurrentTab(true, secondInfoBarMessage);
  // Confirm infobars are destroyed after their banners are dismissed.
  GREYAssertTrue([InfobarManagerAppInterface verifyInfobarCount:1],
                 @"Incorrect number of infobars.");
}

// Tests that a taller Infobar layout is correct and the OK button is tappable.
- (void)testInfobarTallerLayout {
  // Open a new tab and navigate to the test page.
  const GURL testURL = self.testServer->GetURL("/pony.html");
  [ChromeEarlGrey loadURL:testURL];
  [ChromeEarlGrey waitForMainTabCount:1];

  // Infobar Message
  NSString* firstInfoBarMessage =
      @"This is a really long message that will cause this infobar height to "
      @"increase since Confirm Infobar heights changes depending on its "
      @"message.";

  // Add a test infobar to the current tab. Verify that the infobar is present
  // in the model and that the infobar view is visible on screen.
  GREYAssertTrue([InfobarManagerAppInterface
                     addTestInfoBarToCurrentTabWithMessage:firstInfoBarMessage],
                 @"Failed to add infobar to test tab.");
  VerifyTestInfoBarVisibleForCurrentTab(true, firstInfoBarMessage);
  GREYAssertTrue([InfobarManagerAppInterface verifyInfobarCount:1],
                 @"Incorrect number of infobars.");

  // Dismiss the Infobar.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_buttonTitle(@"OK"),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  VerifyTestInfoBarVisibleForCurrentTab(false, firstInfoBarMessage);
  GREYAssertTrue([InfobarManagerAppInterface verifyInfobarCount:0],
                 @"Incorrect number of infobars.");
}

// Tests that the Infobar doesn't dismiss the keyboard when it is triggered
// while the omnibox is focused.
// https://crbug.com/369054108: Flaky.
- (void)DISABLED_testInfobarWithOmniboxFocused {
  // Open a new tab and navigate to the test page.
  const GURL testURL = self.testServer->GetURL("/pony.html");
  [ChromeEarlGrey loadURL:testURL];
  [ChromeEarlGrey waitForMainTabCount:1];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::DefocusedLocationView()]
      performAction:grey_tap()];

  XCUIApplication* app = [[XCUIApplication alloc] init];
  GREYAssert(app.keyboards.count > 0, @"The keyboard is not shown");

  // Infobar Message
  NSString* infoBarMessage = @"TestInfoBar";

  // Add a test infobar to the current tab. Verify that the infobar is present
  // in the model and that the infobar view is visible on screen.
  GREYAssertTrue([InfobarManagerAppInterface
                     addTestInfoBarToCurrentTabWithMessage:infoBarMessage],
                 @"Failed to add infobar to test tab.");

  GREYAssert(app.keyboards.count > 0, @"The keyboard was dismissed");

  VerifyTestInfoBarVisibleForCurrentTab(false, infoBarMessage);
  GREYAssertTrue([InfobarManagerAppInterface verifyInfobarCount:1],
                 @"Incorrect number of infobars.");

  // Cancel the omnibox focus. It should dismiss the keyboard and show the
  // infobar.
  if ([ChromeEarlGrey isCompactWidth]) {
    [[EarlGrey
        selectElementWithMatcher:grey_allOf(chrome_test_util::CancelButton(),
                                            grey_sufficientlyVisible(), nil)]
        performAction:grey_tap()];
  } else {
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Typing Shield")]
        performAction:grey_tap()];
  }
  GREYAssert(app.keyboards.count == 0, @"The keyboard is still visible");
  VerifyTestInfoBarVisibleForCurrentTab(true, infoBarMessage);
}

@end
