// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/enterprise/data_controls/test/data_controls_app_interface.h"
#import "ios/chrome/browser/reader_mode/model/features.h"
#import "ios/chrome/browser/reader_mode/ui/constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/components/enterprise/data_controls/features.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/element_selector.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "ui/base/l10n/l10n_util.h"

using chrome_test_util::ContextMenuItemWithAccessibilityLabelId;

namespace {

NSString* const kCopyConditionName = @"Link copied condition";
NSString* const kLoadReaderModeFailedMessage =
    @"Reader mode content could not be loaded";
NSString* const kCopyLinkFailedMessage = @"Copying link failed";
NSString* const kBlockCopyingLinkFailedMessage =
    @"Link should not have been copied";

// Path to a page compatible with reader mode.
const char kArticlePath[] = "/article.html";
// URL to a page with a static message.
const char kDestinationPageUrl[] = "/destination";
// Path to a page containing the chromium logo and the text `kLogoPageText`.
const char kLogoPagePath[] = "/chromium_logo_page.html";
// The DOM element ID of the chromium image on the logo page.
const char kLogoPageChromiumImageId[] = "chromium_image";
// The text of the message on the logo page.
const char kLogoPageText[] = "Page with some text and the chromium logo image.";

// Returns an ElementSelector for long pressing the first link in the page.
ElementSelector* ElementSelectorToLongPressLink() {
  return [ElementSelector selectorWithCSSSelector:"a"];
}

// Returns an ElementSelector for the chromium image on the logo page.
ElementSelector* LogoPageChromiumImageIdSelector() {
  return [ElementSelector selectorWithElementID:kLogoPageChromiumImageId];
}

// Matcher for the copy link button in the context menu.
id<GREYMatcher> CopyLinkButton() {
  return ContextMenuItemWithAccessibilityLabelId(
      IDS_IOS_COPY_LINK_ACTION_TITLE);
}

// Matcher for the open link in group button in the context menu.
id<GREYMatcher> CopyImageButton() {
  return ContextMenuItemWithAccessibilityLabelId(
      IDS_IOS_CONTENT_CONTEXT_COPYIMAGE);
}

// Taps on `context_menu_item_button` context menu item.
void TapOnContextMenuButton(id<GREYMatcher> context_menu_item_button) {
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:context_menu_item_button];
  [[EarlGrey selectElementWithMatcher:context_menu_item_button]
      performAction:grey_tap()];
}

}  // namespace

@interface DataControlsTestCase : ChromeTestCase
@end

@implementation DataControlsTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;

  if ([self isRunningTest:@selector(testCopyBlockedOnReaderMode)] ||
      [self isRunningTest:@selector(testCopyLinkWarnProceedOnReaderMode)] ||
      [self isRunningTest:@selector(testCopyLinkWarnCancelOnReaderMode)]) {
    config.features_enabled.push_back(kEnableReaderMode);
    config.features_enabled.push_back(kEnableReaderModeInUS);
  }
  config.features_enabled.push_back(
      data_controls::kEnableClipboardDataControlsIOS);

  return config;
}

- (void)setUp {
  [super setUp];
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
}

- (void)tearDownHelper {
  [super tearDownHelper];
}

#pragma mark - Tests

// Tests that copying an image via context menu is blocked when a "BLOCK" is set
// in DataControls policy.
- (void)testCopyBlocked {
  [DataControlsAppInterface setBlockCopyRule];

  [ChromeEarlGrey clearPasteboard];
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kLogoPagePath)];
  [ChromeEarlGrey waitForWebStateContainingText:kLogoPageText];

  [ChromeEarlGreyUI
      longPressElementOnWebView:LogoPageChromiumImageIdSelector()];
  TapOnContextMenuButton(CopyImageButton());

  // Check that the snackbar is shown.
  id<GREYMatcher> snackbarMessage = grey_text(
      l10n_util::GetNSString(IDS_POLICY_ACTION_BLOCKED_BY_ORGANIZATION));
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:snackbarMessage];

  // Check that the image was not copied.
  GREYAssertFalse([ChromeEarlGrey pasteboardHasImages],
                  @"Image should not have been copied");
  [DataControlsAppInterface clearDataControlRules];
}

// Tests that copying an image via context menu is allowed after the user
// proceeds through the warning triggered by DataControlRules policy.
- (void)testCopyWarnProceed {
  [DataControlsAppInterface setWarnCopyRule];

  [ChromeEarlGrey clearPasteboard];
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kLogoPagePath)];
  [ChromeEarlGrey waitForWebStateContainingText:kLogoPageText];

  [ChromeEarlGreyUI
      longPressElementOnWebView:LogoPageChromiumImageIdSelector()];
  TapOnContextMenuButton(CopyImageButton());

  // Tap the "Copy anyways" button on the warning dialog.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::AlertItemWithAccessibilityLabelId(
                     IDS_DATA_CONTROLS_COPY_WARN_CONTINUE_BUTTON)]
      performAction:grey_tap()];

  // Check that the image was copied.
  GREYCondition* copyCondition =
      [GREYCondition conditionWithName:@"Image copied condition"
                                 block:^BOOL {
                                   return [ChromeEarlGrey pasteboardHasImages];
                                 }];
  GREYAssertTrue([copyCondition waitWithTimeout:5], @"Copying image failed");
  [ChromeEarlGrey clearPasteboard];
  [DataControlsAppInterface clearDataControlRules];
}

// Tests that copying an image via context menu is cancelled when the user
// cancels on the warning triggered by DataControlRules policy.
- (void)testCopyWarnCancel {
  [DataControlsAppInterface setWarnCopyRule];

  [ChromeEarlGrey clearPasteboard];
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kLogoPagePath)];
  [ChromeEarlGrey waitForWebStateContainingText:kLogoPageText];

  [ChromeEarlGreyUI
      longPressElementOnWebView:LogoPageChromiumImageIdSelector()];
  TapOnContextMenuButton(CopyImageButton());

  // Tap the "cancel" button on the warning dialog.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::AlertItemWithAccessibilityLabelId(
                     IDS_DATA_CONTROLS_COPY_WARN_CANCEL_BUTTON)]
      performAction:grey_tap()];
  // Check that the image was not copied.
  GREYAssertFalse([ChromeEarlGrey pasteboardHasImages],
                  @"Image should not have been copied");
  [DataControlsAppInterface clearDataControlRules];
}

// Tests that copying a link is blocked in reader mode when the DataControlsRule
// policy is set to do so.
- (void)testCopyBlockedOnReaderMode {
  [DataControlsAppInterface setBlockCopyRule];

  [ChromeEarlGrey clearPasteboard];
  const GURL initialURL = self.testServer->GetURL(kArticlePath);
  [ChromeEarlGrey loadURL:initialURL];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Open Reader Mode UI.
  GREYAssertTrue(
      [ChromeEarlGrey showReaderModeAndWaitUntilReaderModeWebStateIsReady],
      kLoadReaderModeFailedMessage);

  // Wait for Reader Mode UI to appear on-screen.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kReaderModeViewAccessibilityIdentifier)];
  [ChromeEarlGrey
      waitForWebStateContainingElement:ElementSelectorToLongPressLink()];

  // Open the context menu and tap on an action.
  [ChromeEarlGreyUI longPressElementOnWebView:ElementSelectorToLongPressLink()];
  TapOnContextMenuButton(CopyLinkButton());

  // Check that the snackbar is shown.
  id<GREYMatcher> snackbarMessage = grey_text(
      l10n_util::GetNSString(IDS_POLICY_ACTION_BLOCKED_BY_ORGANIZATION));
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:snackbarMessage];

  // Check that the link was not copied.
  GREYAssertTrue([ChromeEarlGrey pasteboardURL].is_empty(),
                 kBlockCopyingLinkFailedMessage);
  [DataControlsAppInterface clearDataControlRules];
}

// Tests that copying a link is allowed in reader mode after the user proceeds
// through the warning triggered by DataControlRules policy.
- (void)testCopyLinkWarnProceedOnReaderMode {
  [DataControlsAppInterface setWarnCopyRule];

  [ChromeEarlGrey clearPasteboard];
  const GURL initialURL = self.testServer->GetURL(kArticlePath);
  [ChromeEarlGrey loadURL:initialURL];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Open Reader Mode UI.
  GREYAssertTrue(
      [ChromeEarlGrey showReaderModeAndWaitUntilReaderModeWebStateIsReady],
      kLoadReaderModeFailedMessage);

  // Wait for Reader Mode UI to appear on-screen.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kReaderModeViewAccessibilityIdentifier)];
  [ChromeEarlGrey
      waitForWebStateContainingElement:ElementSelectorToLongPressLink()];

  // Open the context menu and tap on an action.
  [ChromeEarlGreyUI longPressElementOnWebView:ElementSelectorToLongPressLink()];
  TapOnContextMenuButton(CopyLinkButton());

  // Tap the "Copy anyways" button on the warning dialog.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::AlertItemWithAccessibilityLabelId(
                     IDS_DATA_CONTROLS_COPY_WARN_CONTINUE_BUTTON)]
      performAction:grey_tap()];

  // Check that the link was copied.
  const GURL destinationURL = self.testServer->GetURL(kDestinationPageUrl);
  GREYCondition* copyCondition = [GREYCondition
      conditionWithName:kCopyConditionName
                  block:^BOOL {
                    return [ChromeEarlGrey pasteboardURL] == destinationURL;
                  }];
  GREYAssertTrue([copyCondition waitWithTimeout:5], kCopyLinkFailedMessage);
  [ChromeEarlGrey clearPasteboard];
  [DataControlsAppInterface clearDataControlRules];
}

// Tests that copying a link is cancelled in reader mode when the user cancels
// on the warning triggered by DataControlRules policy.
- (void)testCopyLinkWarnCancelOnReaderMode {
  [DataControlsAppInterface setWarnCopyRule];

  [ChromeEarlGrey clearPasteboard];
  const GURL initialURL = self.testServer->GetURL(kArticlePath);
  [ChromeEarlGrey loadURL:initialURL];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Open Reader Mode UI.
  GREYAssertTrue(
      [ChromeEarlGrey showReaderModeAndWaitUntilReaderModeWebStateIsReady],
      kLoadReaderModeFailedMessage);

  // Wait for Reader Mode UI to appear on-screen.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kReaderModeViewAccessibilityIdentifier)];
  [ChromeEarlGrey
      waitForWebStateContainingElement:ElementSelectorToLongPressLink()];

  // Open the context menu and tap on an action.
  [ChromeEarlGreyUI longPressElementOnWebView:ElementSelectorToLongPressLink()];
  TapOnContextMenuButton(CopyLinkButton());

  // Tap the "cancel" button on the warning dialog.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::AlertItemWithAccessibilityLabelId(
                     IDS_DATA_CONTROLS_COPY_WARN_CANCEL_BUTTON)]
      performAction:grey_tap()];
  // Check that the link was not copied.
  GREYAssertTrue([ChromeEarlGrey pasteboardURL].is_empty(),
                 kBlockCopyingLinkFailedMessage);
  [DataControlsAppInterface clearDataControlRules];
}

// Tests that by DataControlsRules policy, copying a link is blocked when using
// the JavaScript Clipboard API to directly write text to clipboard.
- (void)testCopyBlockedOnJSClipboardAPI {
  [DataControlsAppInterface setBlockCopyRule];

  [ChromeEarlGrey clearPasteboard];
  const GURL initialURL = self.testServer->GetURL(kArticlePath);
  [ChromeEarlGrey loadURL:initialURL];
  [ChromeEarlGrey waitForPageToFinishLoading];

  const GURL destinationURL = self.testServer->GetURL(kDestinationPageUrl);
  NSString* script =
      [NSString stringWithFormat:
                    @"navigator.clipboard.writeText('%@').then(() => { return "
                    @"'SUCCESS'; }).catch(() => { return 'FAILURE'; });",
                    base::SysUTF8ToNSString(destinationURL.spec())];

  [ChromeEarlGrey evaluateJavaScriptWithPotentialError:script];

  // Check that the snackbar is shown.
  id<GREYMatcher> snackbarMessage = grey_text(
      l10n_util::GetNSString(IDS_POLICY_ACTION_BLOCKED_BY_ORGANIZATION));
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:snackbarMessage];
  // Check that the link was not copied.
  GREYAssertTrue([ChromeEarlGrey pasteboardURL].is_empty(),
                 kBlockCopyingLinkFailedMessage);
  [DataControlsAppInterface clearDataControlRules];
}

// Tests that by DataControlsRules policy, copying a link is warned and ignored
// by the user when using the JavaScript Clipboard API to directly write text to
// clipboard, and link is indeed copied to clipboard.
- (void)testCopyWarndOnJSClipboardAPIProceed {
  [DataControlsAppInterface setWarnCopyRule];

  [ChromeEarlGrey clearPasteboard];
  const GURL initialURL = self.testServer->GetURL(kArticlePath);
  [ChromeEarlGrey loadURL:initialURL];
  [ChromeEarlGrey waitForPageToFinishLoading];

  const GURL destinationURL = self.testServer->GetURL(kDestinationPageUrl);
  NSString* script =
      [NSString stringWithFormat:
                    @"navigator.clipboard.writeText('%@').then(() => { return "
                    @"'SUCCESS'; }).catch(() => { return 'FAILURE'; });",
                    base::SysUTF8ToNSString(destinationURL.spec())];

  [ChromeEarlGrey evaluateJavaScriptWithPotentialError:script];

  // Tap the "Copy anyways" button on the warning dialog.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::AlertItemWithAccessibilityLabelId(
                     IDS_DATA_CONTROLS_COPY_WARN_CONTINUE_BUTTON)]
      performAction:grey_tap()];

  // Check that the link was copied.
  GREYCondition* copyCondition = [GREYCondition
      conditionWithName:kCopyConditionName
                  block:^BOOL {
                    return [ChromeEarlGrey pasteboardURL] == destinationURL;
                  }];
  GREYAssertTrue([copyCondition waitWithTimeout:5], kCopyLinkFailedMessage);
  [ChromeEarlGrey clearPasteboard];
  [DataControlsAppInterface clearDataControlRules];
}

// Tests that by DataControlsRules policy, copying a link is warned and
// cancelled by the user when using the JavaScript Clipboard API to directly
// write text to clipboard, and clipboard remains empty.
- (void)testCopyWarndOnJSClipboardAPICancel {
  [DataControlsAppInterface setWarnCopyRule];

  [ChromeEarlGrey clearPasteboard];
  const GURL initialURL = self.testServer->GetURL(kArticlePath);
  [ChromeEarlGrey loadURL:initialURL];
  [ChromeEarlGrey waitForPageToFinishLoading];

  const GURL destinationURL = self.testServer->GetURL(kDestinationPageUrl);
  NSString* script =
      [NSString stringWithFormat:
                    @"navigator.clipboard.writeText('%@').then(() => { return "
                    @"'SUCCESS'; }).catch(() => { return 'FAILURE'; });",
                    base::SysUTF8ToNSString(destinationURL.spec())];

  [ChromeEarlGrey evaluateJavaScriptWithPotentialError:script];

  // Tap the "cancel" button on the warning dialog.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::AlertItemWithAccessibilityLabelId(
                     IDS_DATA_CONTROLS_COPY_WARN_CANCEL_BUTTON)]
      performAction:grey_tap()];

  // Check that the link was not copied.
  GREYAssertTrue([ChromeEarlGrey pasteboardURL].is_empty(),
                 kBlockCopyingLinkFailedMessage);
  [ChromeEarlGrey clearPasteboard];
  [DataControlsAppInterface clearDataControlRules];
}

@end
