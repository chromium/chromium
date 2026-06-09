// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/functional/bind.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/send_tab_to_self/features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/element_selector.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

NSString* const kTargetDeviceName = @"My other device";
NSString* const kSendTabToSelfModalCancelButtonId =
    @"kSendTabToSelfModalCancelButton";

// Helpers for web element selectors.
ElementSelector* TargetElement() {
  return [ElementSelector selectorWithElementID:"target"];
}

ElementSelector* UsernameElement() {
  return [ElementSelector selectorWithElementID:"username"];
}

}  // namespace

@interface SendTabToSelfCoordinatorTestCase : ChromeTestCase
@end

@implementation SendTabToSelfCoordinatorTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.features_enabled.push_back(
      send_tab_to_self::kSendTabToSelfPropagateScrollPosition);
  config.features_enabled.push_back(
      send_tab_to_self::kSendTabToSelfPropagateFormFields);
  config.features_enabled.push_back(
      send_tab_to_self::kSendTabToSelfExtraEntryPoints);
  if ([self
          isRunningTest:@selector(testSendTabToSelfAndVerifySuccessSnackbar)]) {
    config.features_enabled.push_back(
        send_tab_to_self::kSendTabToSelfPostSendToast);
  }
  return config;
}

- (void)setUp {
  [super setUp];

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

// Tests that the entry point button is shown to a signed out user, even if
// there are no device-level accounts.
- (void)testShowButtonIfSignedOutAndNoDeviceAccount {
  [ChromeEarlGrey
      loadURL:self.testServer->GetURL(
                  "/send_tab_to_self/send_tab_to_self_active_page.html")];
  [ChromeEarlGrey waitForWebStateContainingElement:TargetElement()];

  [ChromeEarlGreyUI shareCurrentPage];

  NSString* sendTabToSelf =
      l10n_util::GetNSString(IDS_IOS_SHARE_MENU_SEND_TAB_TO_SELF_ACTION);
  [ChromeEarlGrey verifyTextVisibleInActivitySheetWithID:sendTabToSelf];
}

- (void)testShowPromoIfSignedOutAndHasDeviceAccount {
  [ChromeEarlGrey addFakeSyncServerDeviceInfo:kTargetDeviceName
                         lastUpdatedTimestamp:base::Time::Now()];
  [SigninEarlGrey addFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [ChromeEarlGrey
      loadURL:self.testServer->GetURL(
                  "/send_tab_to_self/send_tab_to_self_active_page.html")];
  [ChromeEarlGrey waitForWebStateContainingElement:TargetElement()];

  [ChromeEarlGreyUI shareCurrentPage];

  NSString* sendTabToSelf =
      l10n_util::GetNSString(IDS_IOS_SHARE_MENU_SEND_TAB_TO_SELF_ACTION);
  [ChromeEarlGrey tapButtonInActivitySheetWithID:sendTabToSelf];

  [SigninEarlGreyUI verifyWebSigninIsVisible:YES];

  // Confirm the promo.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kConsistencySigninPrimaryButtonAccessibilityIdentifier)]
      performAction:grey_tap()];

  // The device list should be shown.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:grey_accessibilityLabel(
                                                       kTargetDeviceName)];

  // Clean up the promo sheet.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSendTabToSelfModalCancelButtonId)]
      performAction:grey_tap()];
}

- (void)testShowMessageIfSignedInAndNoTargetDevice {
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [ChromeEarlGrey
      loadURL:self.testServer->GetURL(
                  "/send_tab_to_self/send_tab_to_self_active_page.html")];
  [ChromeEarlGrey waitForWebStateContainingElement:TargetElement()];

  [ChromeEarlGreyUI shareCurrentPage];
  NSString* sendTabToSelf =
      l10n_util::GetNSString(IDS_IOS_SHARE_MENU_SEND_TAB_TO_SELF_ACTION);
  [ChromeEarlGrey tapButtonInActivitySheetWithID:sendTabToSelf];

  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(
                         IDS_SEND_TAB_TO_SELF_NO_TARGET_DEVICE_LABEL)),
                     grey_userInteractionEnabled(), nil)];

  // Clean up.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSendTabToSelfModalCancelButtonId)]
      performAction:grey_tap()];
}

- (void)testShowDevicePickerIfSignedInAndHasTargetDevice {
  // Setting a recent timestamp here is necessary, otherwise the device will be
  // considered expired and won't be displayed.
  [ChromeEarlGrey addFakeSyncServerDeviceInfo:kTargetDeviceName
                         lastUpdatedTimestamp:base::Time::Now()];
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [ChromeEarlGrey
      loadURL:self.testServer->GetURL(
                  "/send_tab_to_self/send_tab_to_self_active_page.html")];
  [ChromeEarlGrey waitForWebStateContainingElement:TargetElement()];

  [ChromeEarlGreyUI shareCurrentPage];
  NSString* sendTabToSelf =
      l10n_util::GetNSString(IDS_IOS_SHARE_MENU_SEND_TAB_TO_SELF_ACTION);
  [ChromeEarlGrey tapButtonInActivitySheetWithID:sendTabToSelf];

  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:grey_accessibilityLabel(
                                                       kTargetDeviceName)];

  // Clean up.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSendTabToSelfModalCancelButtonId)]
      performAction:grey_tap()];
}

- (void)testSendTabToSelfAndVerifySnackbar {
  const char kPageText[] =
      "This is a long and unique text that should be easy to generate a text "
      "fragment for without any ambiguity.";

  [ChromeEarlGrey addFakeSyncServerDeviceInfo:kTargetDeviceName
                         lastUpdatedTimestamp:base::Time::Now()];
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [ChromeEarlGrey
      loadURL:self.testServer->GetURL(
                  "/send_tab_to_self/send_tab_to_self_active_page.html")];
  [ChromeEarlGrey waitForWebStateContainingElement:TargetElement()];

  [ChromeEarlGreyUI shareCurrentPage];
  NSString* sendTabToSelf =
      l10n_util::GetNSString(IDS_IOS_SHARE_MENU_SEND_TAB_TO_SELF_ACTION);
  [ChromeEarlGrey tapButtonInActivitySheetWithID:sendTabToSelf];

  // Tap the device in the device picker.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:grey_accessibilityLabel(
                                                       kTargetDeviceName)];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(kTargetDeviceName)]
      performAction:grey_tap()];

  // Tap "Send".
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          @"kSendTabToSelfModalSendButton")]
      performAction:grey_tap()];

  // Wait for and verify the snackbar message.
  NSString* snackbarMessage =
      l10n_util::GetNSStringF(IDS_IOS_SEND_TAB_TO_SELF_SNACKBAR_MESSAGE,
                              base::SysNSStringToUTF16(kTargetDeviceName));
  id<GREYMatcher> snackbarMatcher = grey_allOf(
      chrome_test_util::SnackbarViewMatcher(),
      grey_descendant(grey_accessibilityLabel(snackbarMessage)), nil);
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:snackbarMatcher];

  // Verify that the text fragment was successfully captured and attached to the
  // STTS entry in the model.
  NSString* urlString = base::SysUTF8ToNSString(
      self.testServer
          ->GetURL("/send_tab_to_self/send_tab_to_self_active_page.html")
          .spec());
  NSString* textFragment =
      [ChromeEarlGrey textFragmentForSendTabToSelfEntryWithURL:urlString];
  GREYAssertTrue(
      [textFragment caseInsensitiveCompare:base::SysUTF8ToNSString(
                                               kPageText)] == NSOrderedSame,
      @"Text fragment should be captured. Expected '%s' (case-insensitive) but "
      @"got %@",
      kPageText, textFragment);
}

- (void)testSendTabToSelfAndVerifySuccessSnackbar {
  [ChromeEarlGrey addFakeSyncServerDeviceInfo:kTargetDeviceName
                         lastUpdatedTimestamp:base::Time::Now()];
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [ChromeEarlGrey
      loadURL:self.testServer->GetURL(
                  "/send_tab_to_self/send_tab_to_self_active_page.html")];
  [ChromeEarlGrey waitForWebStateContainingElement:TargetElement()];

  [ChromeEarlGreyUI shareCurrentPage];
  NSString* sendTabToSelf =
      l10n_util::GetNSString(IDS_IOS_SHARE_MENU_SEND_TAB_TO_SELF_ACTION);
  [ChromeEarlGrey tapButtonInActivitySheetWithID:sendTabToSelf];

  // Tap the device in the device picker.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:grey_accessibilityLabel(
                                                       kTargetDeviceName)];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(kTargetDeviceName)]
      performAction:grey_tap()];

  // Tap "Send".
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          @"kSendTabToSelfModalSendButton")]
      performAction:grey_tap()];

  // Wait for and verify the success snackbar message.
  NSString* successMessage =
      l10n_util::GetNSStringF(IDS_SEND_TAB_TO_SELF_POST_SEND_SUCCESS_TOAST,
                              base::SysNSStringToUTF16(kTargetDeviceName));
  id<GREYMatcher> snackbarMatcher =
      grey_allOf(chrome_test_util::SnackbarViewMatcher(),
                 grey_descendant(grey_accessibilityLabel(successMessage)), nil);
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:snackbarMatcher];
}

// Tests that a text fragment is correctly consumed and scrolls the page
// when passed internally during an OpenNewTabCommand, without highlighting.
- (void)testRestoreScrollPosition {
  NSString* urlString = base::SysUTF8ToNSString(
      self.testServer
          ->GetURL("/send_tab_to_self/send_tab_to_self_scroll_restoration.html")
          .spec());

  // Use the known text fragment for the page content.
  NSString* textFragment = @"This%20is%20a%20long,without%20any%20ambiguity.";

  // Sign in first. This ensures the keystore encryption keys (Nigori) are
  // generated and the local device cache GUID is registered.
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];

  // Inject a fake DeviceInfo for the sender device.
  [ChromeEarlGrey addFakeSyncServerDeviceInfo:kTargetDeviceName
                         lastUpdatedTimestamp:base::Time::Now()];

  // Add fake SendTabToSelf entry.
  NSString* guid =
      [ChromeEarlGrey addFakeSendTabToSelfEntryWithURL:urlString
                                                 title:@"Scroll Page"
                                          textFragment:textFragment];

  [ChromeEarlGrey triggerSyncCycleForType:syncer::SEND_TAB_TO_SELF];
  [ChromeEarlGrey waitForSendTabToSelfEntryWithGUID:guid];

  // Open the new tab marking it as from Send Tab To Self.
  [ChromeEarlGrey openSendTabToSelfNewTabWithURL:urlString
                                    textFragment:textFragment
                                       entryGUID:guid];
  [ChromeEarlGrey
      waitForWebStateVisibleURL:GURL(base::SysNSStringToUTF8(urlString))];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Wait for the new tab to load and the fragment to be applied.
  [ChromeEarlGrey waitForWebStateContainingElement:TargetElement()];

  // Verify that the page has scrolled down to the fragment.
  NSString* checkScrollJS = @"window.scrollY > 0;";
  [ChromeEarlGrey waitForJavaScriptCondition:checkScrollJS];

  // Verify that the polyfill did not leave any highlight marks.
  NSString* checkHighlightJS =
      @"document.getElementsByTagName('mark').length === 0;";
  BOOL noHighlight =
      [ChromeEarlGrey evaluateJavaScript:checkHighlightJS].GetBool();
  GREYAssertTrue(noHighlight, @"Text fragment should not be highlighted.");

  // Clean up the opened tab.
  [ChromeEarlGrey closeCurrentTab];
}

// Tests that an invalid text fragment is safely ignored and doesn't crash or
// highlight.
- (void)testRestoreScrollPositionInvalidFragment {
  NSString* urlString = base::SysUTF8ToNSString(
      self.testServer
          ->GetURL("/send_tab_to_self/send_tab_to_self_scroll_restoration.html")
          .spec());

  // Use an invalid text fragment.
  NSString* textFragment = @"InvalidFragmentThatDoesNotMatchAnything";

  // Sign in first. This ensures the keystore encryption keys (Nigori) are
  // generated and the local device cache GUID is registered.
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];

  // Inject a fake DeviceInfo for the sender device.
  [ChromeEarlGrey addFakeSyncServerDeviceInfo:kTargetDeviceName
                         lastUpdatedTimestamp:base::Time::Now()];

  // Add fake SendTabToSelf entry.
  NSString* guid =
      [ChromeEarlGrey addFakeSendTabToSelfEntryWithURL:urlString
                                                 title:@"Scroll Page"
                                          textFragment:textFragment];

  [ChromeEarlGrey triggerSyncCycleForType:syncer::SEND_TAB_TO_SELF];
  [ChromeEarlGrey waitForSendTabToSelfEntryWithGUID:guid];

  // Open the new tab marking it as from Send Tab To Self.
  [ChromeEarlGrey openSendTabToSelfNewTabWithURL:urlString
                                    textFragment:textFragment
                                       entryGUID:guid];
  [ChromeEarlGrey
      waitForWebStateVisibleURL:GURL(base::SysNSStringToUTF8(urlString))];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Wait for the new tab to load.
  [ChromeEarlGrey waitForWebStateContainingElement:TargetElement()];

  // Verify that the page has NOT scrolled down. We wait for a short duration
  // to ensure any pending async scrolls do not occur.
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(1));
  NSString* checkScrollJS = @"window.scrollY === 0;";
  BOOL hasNotScrolled =
      [ChromeEarlGrey evaluateJavaScript:checkScrollJS].GetBool();
  GREYAssertTrue(hasNotScrolled, @"Page should not have scrolled.");

  // Verify that the polyfill did not leave any highlight marks.
  NSString* checkHighlightJS =
      @"document.getElementsByTagName('mark').length === 0;";
  BOOL noHighlight =
      [ChromeEarlGrey evaluateJavaScript:checkHighlightJS].GetBool();
  GREYAssertTrue(noHighlight, @"Text fragment should not be highlighted.");

  // Clean up the opened tab.
  [ChromeEarlGrey closeCurrentTab];
}

// Tests that an empty text fragment is safely ignored.
- (void)testRestoreScrollPositionEmptyFragment {
  NSString* urlString = base::SysUTF8ToNSString(
      self.testServer
          ->GetURL("/send_tab_to_self/send_tab_to_self_scroll_restoration.html")
          .spec());

  // Use an empty text fragment.
  NSString* textFragment = @"";

  // Sign in first. This ensures the keystore encryption keys (Nigori) are
  // generated and the local device cache GUID is registered.
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];

  // Inject a fake DeviceInfo for the sender device.
  [ChromeEarlGrey addFakeSyncServerDeviceInfo:kTargetDeviceName
                         lastUpdatedTimestamp:base::Time::Now()];

  // Add fake SendTabToSelf entry.
  NSString* guid =
      [ChromeEarlGrey addFakeSendTabToSelfEntryWithURL:urlString
                                                 title:@"Scroll Page"
                                          textFragment:textFragment];

  [ChromeEarlGrey triggerSyncCycleForType:syncer::SEND_TAB_TO_SELF];
  [ChromeEarlGrey waitForSendTabToSelfEntryWithGUID:guid];

  // Open the new tab marking it as from Send Tab To Self.
  [ChromeEarlGrey openSendTabToSelfNewTabWithURL:urlString
                                    textFragment:textFragment
                                       entryGUID:guid];
  [ChromeEarlGrey
      waitForWebStateVisibleURL:GURL(base::SysNSStringToUTF8(urlString))];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Wait for the new tab to load.
  [ChromeEarlGrey waitForWebStateContainingElement:TargetElement()];

  // Verify that the page has NOT scrolled down. We wait for a short duration
  // to ensure any pending async scrolls do not occur.
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(1));
  NSString* checkScrollJS = @"window.scrollY === 0;";
  BOOL hasNotScrolled =
      [ChromeEarlGrey evaluateJavaScript:checkScrollJS].GetBool();
  GREYAssertTrue(hasNotScrolled,
                 @"Page should not have scrolled for empty fragment.");

  // Clean up the opened tab.
  [ChromeEarlGrey closeCurrentTab];
}

// Tests that form fields are successfully restored when a page is opened
// via Send Tab To Self with form field propagation enabled.
- (void)testRestoreFormFields {
  NSString* urlString = base::SysUTF8ToNSString(
      self.testServer
          ->GetURL("/send_tab_to_self/send_tab_to_self_form_propagation.html")
          .spec());

  // 1. Sign in first. This ensures the keystore encryption keys (Nigori) are
  // generated and the local device cache GUID is registered.
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];

  // Inject a fake DeviceInfo for the sender device.
  [ChromeEarlGrey addFakeSyncServerDeviceInfo:kTargetDeviceName
                         lastUpdatedTimestamp:base::Time::Now()];

  NSDictionary<NSString*, NSString*>* formData = @{@"username" : @"testuser"};
  NSString* guid = [ChromeEarlGrey addFakeSendTabToSelfEntryWithURL:urlString
                                                              title:@"Form Page"
                                                      formFieldData:formData];

  // TODO(crbug.com/519101926): Investigate why manually triggering a sync cycle
  // is necessary. It might be because we are not waiting for the invalidations
  // system on the client to be started up. If so, we should find a global fix.
  [ChromeEarlGrey triggerSyncCycleForType:syncer::SEND_TAB_TO_SELF];
  [ChromeEarlGrey waitForSendTabToSelfEntryWithGUID:guid];

  // 2. Open the tab via Send Tab To Self.
  [ChromeEarlGrey openSendTabToSelfNewTabWithURL:urlString
                                    textFragment:nil
                                       entryGUID:guid];
  [ChromeEarlGrey
      waitForWebStateVisibleURL:GURL(base::SysNSStringToUTF8(urlString))];
  [ChromeEarlGrey waitForPageToFinishLoading];
  [ChromeEarlGrey waitForWebStateContainingElement:UsernameElement()];

  // Verify that the input field was populated with the expected value.
  NSString* checkFilledJS = @"(function() {"
                            @"  var el = document.getElementById('username');"
                            @"  return el ? el.value === 'testuser' : false;"
                            @"})();";
  [ChromeEarlGrey waitForJavaScriptCondition:checkFilledJS];
  [ChromeEarlGrey closeCurrentTab];

  // 3. Open the tab normally again (with the entry still active in the
  // database).
  [ChromeEarlGrey openNewTabWithURL:urlString textFragment:nil];
  [ChromeEarlGrey
      waitForWebStateVisibleURL:GURL(base::SysNSStringToUTF8(urlString))];
  [ChromeEarlGrey waitForPageToFinishLoading];
  [ChromeEarlGrey waitForWebStateContainingElement:UsernameElement()];

  // Verify that the input field remains empty for normal navigations.
  NSString* checkEmptyJS = @"(function() {"
                           @"  var el = document.getElementById('username');"
                           @"  return el ? el.value === '' : false;"
                           @"})();";
  [ChromeEarlGrey waitForJavaScriptCondition:checkEmptyJS];
  [ChromeEarlGrey closeCurrentTab];
}

// Tests that long-pressing a tab cell in the tab switcher shows "Send to Your
// Devices" and tapping it displays the device picker modal.
- (void)testLongPressTabSwitcherTabToShowSendToYourDevices {
  [ChromeEarlGrey addFakeSyncServerDeviceInfo:kTargetDeviceName
                         lastUpdatedTimestamp:base::Time::Now()];
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [ChromeEarlGrey
      loadURL:self.testServer->GetURL(
                  "/send_tab_to_self/send_tab_to_self_active_page.html")];
  [ChromeEarlGrey waitForWebStateContainingElement:TargetElement()];

  // Open tab switcher.
  [ChromeEarlGrey showTabSwitcher];

  // Long press the active tab cell (index 0).
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      performAction:grey_longPress()];

  // Verify the "Send to Your Devices" menu item shows up.
  id<GREYMatcher> sendToDevicesMenuItem =
      chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
          IDS_IOS_SHARE_MENU_SEND_TAB_TO_SELF_ACTION);
  [[EarlGrey selectElementWithMatcher:sendToDevicesMenuItem]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap the context menu item.
  [[EarlGrey selectElementWithMatcher:sendToDevicesMenuItem]
      performAction:grey_tap()];

  // Verify that the device picker shows up.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:grey_accessibilityLabel(
                                                       kTargetDeviceName)];

  // Clean up.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSendTabToSelfModalCancelButtonId)]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:
                      grey_accessibilityID(kSendTabToSelfModalCancelButtonId)];
}

// Tests that long-pressing a tab cell in the tab switcher shows "Send to Your
// Devices" and tapping it displays the sign-in promo if the user is signed out.
- (void)testLongPressTabSwitcherTabToShowSigninPromo {
  [ChromeEarlGrey addFakeSyncServerDeviceInfo:kTargetDeviceName
                         lastUpdatedTimestamp:base::Time::Now()];
  [SigninEarlGrey addFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [ChromeEarlGrey
      loadURL:self.testServer->GetURL(
                  "/send_tab_to_self/send_tab_to_self_active_page.html")];
  [ChromeEarlGrey waitForWebStateContainingElement:TargetElement()];

  // Open tab switcher.
  [ChromeEarlGrey showTabSwitcher];

  // Long press the active tab cell (index 0).
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      performAction:grey_longPress()];

  // Verify the "Send to Your Devices" menu item shows up.
  id<GREYMatcher> sendToDevicesMenuItem =
      chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
          IDS_IOS_SHARE_MENU_SEND_TAB_TO_SELF_ACTION);
  [[EarlGrey selectElementWithMatcher:sendToDevicesMenuItem]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap the context menu item.
  [[EarlGrey selectElementWithMatcher:sendToDevicesMenuItem]
      performAction:grey_tap()];

  // Verify that the sign-in promo is visible.
  [SigninEarlGreyUI verifyWebSigninIsVisible:YES];

  // Confirm the promo.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kConsistencySigninPrimaryButtonAccessibilityIdentifier)]
      performAction:grey_tap()];

  // The device list should be shown.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:grey_accessibilityLabel(
                                                       kTargetDeviceName)];

  // Clean up.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSendTabToSelfModalCancelButtonId)]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:
                      grey_accessibilityID(kSendTabToSelfModalCancelButtonId)];
}

// Tests that long-pressing the defocused location view shows "Send to Your
// Devices" and tapping it displays the device picker modal.
- (void)testLongPressOmniboxToShowSendToYourDevices {
  [ChromeEarlGrey addFakeSyncServerDeviceInfo:kTargetDeviceName
                         lastUpdatedTimestamp:base::Time::Now()];
  // Disable EarlGrey's synchronization during sign-in because the concurrent
  // sync/sign-in initialization triggers micro-animations and layouts on the
  // Location Bar steady view, which makes EarlGrey's synchronization hang
  // indefinitely on heavily-loaded bots when trying to subsequently interact
  // with the defocused location view.
  {
    ScopedSynchronizationDisabler disabler;
    [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  }
  [ChromeEarlGrey
      loadURL:self.testServer->GetURL(
                  "/send_tab_to_self/send_tab_to_self_active_page.html")];
  [ChromeEarlGrey waitForWebStateContainingElement:TargetElement()];

  // Long press the DefocusedLocationView.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::DefocusedLocationView()]
      performAction:grey_longPress()];

  // Verify the "Send to Your Devices" menu item shows up.
  id<GREYMatcher> sendToDevicesMenuItem =
      chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
          IDS_IOS_SHARE_MENU_SEND_TAB_TO_SELF_ACTION);
  [[EarlGrey selectElementWithMatcher:sendToDevicesMenuItem]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap the context menu item.
  [[EarlGrey selectElementWithMatcher:sendToDevicesMenuItem]
      performAction:grey_tap()];

  // Verify that the device picker shows up.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:grey_accessibilityLabel(
                                                       kTargetDeviceName)];

  // Clean up.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSendTabToSelfModalCancelButtonId)]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:
                      grey_accessibilityID(kSendTabToSelfModalCancelButtonId)];
}

@end
