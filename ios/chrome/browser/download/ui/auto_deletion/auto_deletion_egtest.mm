// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/functional/bind.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/download/ui/download_egtest_util.h"
#import "ios/chrome/browser/history/ui_bundled/history_ui_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/common/ui/confirmation_alert/constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/request_handler_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// The GREY matcher for the IPH's primary action button.
id<GREYMatcher> enableAutoDeletionIPHButton() {
  return grey_allOf(grey_accessibilityID(
                        kConfirmationAlertPrimaryActionAccessibilityIdentifier),
                    grey_accessibilityLabel(l10n_util::GetNSString(
                        IDS_IOS_AUTO_DELETION_IPH_PRIMARY_ACTION)),
                    grey_interactable(), nil);
}

// GREY matcher for the IPH's secondary action button.
id<GREYMatcher> dismissAutoDeletionIPHButton() {
  return grey_allOf(
      grey_accessibilityID(
          kConfirmationAlertSecondaryActionAccessibilityIdentifier),
      grey_accessibilityLabel(
          l10n_util::GetNSString(IDS_IOS_AUTO_DELETION_IPH_REJECTION)),
      grey_interactable(), nil);
}

// GREY matcher for the action-sheet's primary action button.
id<GREYMatcher> scheduleAutoDeletionActionSheetButton() {
  NSString* primaryActionTitle =
      l10n_util::GetNSString(IDS_IOS_AUTO_DELETION_ACTION_SHEET_PRIMARY_ACTION);
  NSString* accessibilityID =
      [NSString stringWithFormat:@"%@%@", primaryActionTitle, @"AlertAction"];
  return grey_allOf(grey_accessibilityID(accessibilityID),
                    grey_accessibilityLabel(primaryActionTitle),
                    grey_interactable(), nil);
}

// Waits for the Auto-deletion IPH to appear.
[[nodiscard]] bool WaitForIPH() {
  return base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^{
        NSError* error = nil;
        [[EarlGrey selectElementWithMatcher:enableAutoDeletionIPHButton()]
            assertWithMatcher:grey_interactable()
                        error:&error];
        return (error == nil);
      });
}

// Waits for the Auto-deletion action-sheet to appear.
[[nodiscard]] bool WaitForActionSheet() {
  return base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^{
        NSError* error = nil;
        [[EarlGrey
            selectElementWithMatcher:scheduleAutoDeletionActionSheetButton()]
            assertWithMatcher:grey_interactable()
                        error:&error];
        return (error == nil);
      });
}

}  // namespace

@interface AutoDeletionTestCase : ChromeTestCase
@end

@implementation AutoDeletionTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kDownloadAutoDeletionFeatureEnabled);
  config.additional_args.push_back(std::string("-") +
                                   test_switches::kEnableIPH +
                                   "=IPH_iOSDownloadAutoDeletion");
  return config;
}

- (void)setUp {
  [super setUp];
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&net::test_server::HandlePrefixedRequest, "/",
                          base::BindRepeating(&download::GetResponse)));
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
}

// Tests that the Auto-deletion IPH appears when a file is downloaded while in
// Incognito.
- (void)testIPHAppearsWhenDowloadingFileInIncognito {
  [ChromeEarlGrey openNewIncognitoTab];

  // Create a download task.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:"Download"];
  [ChromeEarlGrey tapWebStateElementWithID:@"download"];
  GREYAssert(download::WaitForDownloadButton(),
             @"Download button did not show up");

  [[EarlGrey selectElementWithMatcher:download::DownloadButton()]
      performAction:grey_tap()];

  GREYAssert(WaitForIPH(), @"Auto-deletion IPH did not appear.");
}

// Tests that the Auto-deletion IPH appears when Incognito has been used and a
// file is then downloaded in an non-incognito tab.
- (void)testIPHAppearsWhenDowloadingFileAfterUsingIncognito {
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey openNewTab];

  // Create a download task.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:"Download"];
  [ChromeEarlGrey tapWebStateElementWithID:@"download"];
  GREYAssert(download::WaitForDownloadButton(),
             @"Download button did not show up");

  [[EarlGrey selectElementWithMatcher:download::DownloadButton()]
      performAction:grey_tap()];

  GREYAssert(WaitForIPH(), @"Auto-deletion IPH did not appear.");
}

// Tests that the Auto-deletion action sheet does not appear after the user
// declines to enable Auto-deletion.
- (void)testActionSheetDoesNotAppearWhenDowloadingFile {
  [ChromeEarlGrey openNewIncognitoTab];

  // Create a download task.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:"Download"];
  [ChromeEarlGrey tapWebStateElementWithID:@"download"];
  GREYAssert(download::WaitForDownloadButton(),
             @"Download button did not show up");
  [[EarlGrey selectElementWithMatcher:download::DownloadButton()]
      performAction:grey_tap()];
  GREYAssert(WaitForIPH(), @"Auto-deletion IPH did not appear.");

  // Dismiss the Auto-deletion IPH and the Download Manager.
  [[EarlGrey selectElementWithMatcher:dismissAutoDeletionIPHButton()]
      performAction:grey_tap()];
  GREYAssert(download::WaitForOpenInButton(),
             @"Open in... button did not show up");
  [[EarlGrey selectElementWithMatcher:download::CloseButton()]
      performAction:grey_tap()];

  // Reload page with another download task.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:"Download"];
  [ChromeEarlGrey tapWebStateElementWithID:@"download"];
  GREYAssert(download::WaitForDownloadButton(),
             @"Download button did not show up");
  [[EarlGrey selectElementWithMatcher:download::DownloadButton()]
      performAction:grey_tap()];

  GREYAssertFalse(WaitForActionSheet(),
                  @"Auto-deletion action sheet did not appear.");
}

@end
