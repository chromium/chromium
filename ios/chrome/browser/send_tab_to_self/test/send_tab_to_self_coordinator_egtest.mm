// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/functional/bind.h"
#import "base/strings/strcat.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/send_tab_to_self/features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/infobars/ui_bundled/banners/infobar_banner_constants.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/common/ui/button_stack/button_stack_constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
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
NSString* const kSendTabToSelfModalMenuButtonId =
    @"kSendTabToSelfModalMenuButton";
NSString* const kExampleURL = @"https://www.example.com/";

// Helpers for web element selectors.
ElementSelector* TargetElement() {
  return [ElementSelector selectorWithElementID:"target"];
}

ElementSelector* UsernameElement() {
  return [ElementSelector selectorWithElementID:"username"];
}

void DismissSnackbar() {
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      chrome_test_util::SnackbarViewMatcher()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SnackbarViewMatcher()]
      performAction:grey_tap()];
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
  config.features_enabled.push_back(
      send_tab_to_self::kSendTabToSelfEnhancedBottomsheet);
  if ([self
          isRunningTest:@selector(testSendTabToSelfAndVerifySuccessSnackbar)] ||
      [self isRunningTest:@selector(testSendTabToSelfAndVerifyErrorSnackbar)]) {
    config.features_enabled.push_back(
        send_tab_to_self::kSendTabToSelfPostSendToast);
  } else if ([self
                 isRunningTest:@selector(testSendTabToSelfAndVerifySnackbar)]) {
    config.features_disabled.push_back(
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
      l10n_util::GetNSString(IDS_IOS_SEND_TAB_TO_SELF_TARGET_DEVICE_ACTION);
  [ChromeEarlGrey verifyTextVisibleInActivitySheetWithID:sendTabToSelf];

  // Clean up the activity sheet.
  [ChromeEarlGrey closeActivitySheet];
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
      l10n_util::GetNSString(IDS_IOS_SEND_TAB_TO_SELF_TARGET_DEVICE_ACTION);
  [ChromeEarlGrey tapButtonInActivitySheetWithID:sendTabToSelf];

  [SigninEarlGreyUI verifyWebSigninIsVisible:YES];

  // Confirm the promo.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kConsistencySigninPrimaryButtonAccessibilityIdentifier)]
      performAction:grey_tap()];

  // Dismiss the signin snackbar, which would otherwise overlap and obscure the
  // target device picker bottom sheet.
  DismissSnackbar();

  // The device list should be shown.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:grey_accessibilityLabel(
                                                       kTargetDeviceName)];

  // Clean up the promo sheet.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSendTabToSelfModalCancelButtonId)]
      performAction:grey_tap()];
}

- (void)testTapManageDevicesOpensMyAccountDevicesPage {
  [ChromeEarlGrey addFakeSyncServerDeviceInfo:kTargetDeviceName
                         lastUpdatedTimestamp:base::Time::Now()];
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [ChromeEarlGrey
      loadURL:self.testServer->GetURL(
                  "/send_tab_to_self/send_tab_to_self_active_page.html")];
  [ChromeEarlGrey waitForWebStateContainingElement:TargetElement()];

  [ChromeEarlGreyUI shareCurrentPage];
  NSString* sendTabToSelf =
      l10n_util::GetNSString(IDS_IOS_SEND_TAB_TO_SELF_TARGET_DEVICE_ACTION);
  [ChromeEarlGrey tapButtonInActivitySheetWithID:sendTabToSelf];

  // Tap the menu button on the top left.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSendTabToSelfModalMenuButtonId)]
      performAction:grey_tap()];

  // The menu should pop up and show the "Manage your devices" action. Tap it.
  NSString* manageYourDevicesText =
      l10n_util::GetNSString(IDS_IOS_SEND_TAB_TO_SELF_MANAGE_DEVICES);
  [[EarlGrey selectElementWithMatcher:grey_text(manageYourDevicesText)]
      performAction:grey_tap()];

  // Tapping "Manage your devices" should open the My Devices page in a new tab.
  [ChromeEarlGrey
      waitForWebStateVisibleURL:GURL(kGoogleMyAccountDeviceActivityURL)];

  // Clean up: Close the opened tab.
  [ChromeEarlGrey closeCurrentTab];
}

- (void)testShowMessageIfSignedInAndNoTargetDevice {
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [ChromeEarlGrey
      loadURL:self.testServer->GetURL(
                  "/send_tab_to_self/send_tab_to_self_active_page.html")];
  [ChromeEarlGrey waitForWebStateContainingElement:TargetElement()];

  [ChromeEarlGreyUI shareCurrentPage];
  NSString* sendTabToSelf =
      l10n_util::GetNSString(IDS_IOS_SEND_TAB_TO_SELF_TARGET_DEVICE_ACTION);
  [ChromeEarlGrey tapButtonInActivitySheetWithID:sendTabToSelf];

  // Verify the "No devices found" title is shown.
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      grey_accessibilityLabel(l10n_util::GetNSString(
                          IDS_IOS_SEND_TAB_TO_SELF_NO_DEVICES_FOUND_TITLE))];

  // Verify the subtitle is shown.
  NSString* expectedSubtitle = l10n_util::GetNSStringF(
      IDS_IOS_SEND_TAB_TO_SELF_NO_TARGET_DEVICE_LABEL_WITH_EMAIL,
      base::SysNSStringToUTF16([FakeSystemIdentity fakeIdentity1].userEmail));
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      grey_allOf(grey_accessibilityLabel(expectedSubtitle),
                                 grey_userInteractionEnabled(), nil)];

  // Clean up: tap the "Close" primary action button.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kButtonStackPrimaryActionAccessibilityIdentifier)]
      performAction:grey_tap()];

  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:
                      grey_accessibilityID(
                          kButtonStackPrimaryActionAccessibilityIdentifier)];
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
      l10n_util::GetNSString(IDS_IOS_SEND_TAB_TO_SELF_TARGET_DEVICE_ACTION);
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
      l10n_util::GetNSString(IDS_IOS_SEND_TAB_TO_SELF_TARGET_DEVICE_ACTION);
  [ChromeEarlGrey tapButtonInActivitySheetWithID:sendTabToSelf];

  // Verify the device is shown in the device picker.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:grey_accessibilityLabel(
                                                       kTargetDeviceName)];

  // Tap "Send".
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          @"kSendTabToSelfModalSendButton")]
      performAction:grey_tap()];

  // Verify that the bottom sheet is dismissed.
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:
                      grey_accessibilityID(@"kSendTabToSelfModalSendButton")];

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
      l10n_util::GetNSString(IDS_IOS_SEND_TAB_TO_SELF_TARGET_DEVICE_ACTION);
  [ChromeEarlGrey tapButtonInActivitySheetWithID:sendTabToSelf];

  // Verify the device is shown in the device picker.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:grey_accessibilityLabel(
                                                       kTargetDeviceName)];

  // Tap "Send".
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          @"kSendTabToSelfModalSendButton")]
      performAction:grey_tap()];

  // Verify that the bottom sheet is dismissed.
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:
                      grey_accessibilityID(@"kSendTabToSelfModalSendButton")];

  // Wait for and verify the success snackbar message.
  NSString* snackbarMessage =
      l10n_util::GetNSStringF(IDS_SEND_TAB_TO_SELF_POST_SEND_SUCCESS_TOAST,
                              base::SysNSStringToUTF16(kTargetDeviceName));
  id<GREYMatcher> snackbarMatcher = grey_allOf(
      chrome_test_util::SnackbarViewMatcher(),
      grey_descendant(grey_accessibilityLabel(snackbarMessage)), nil);
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:snackbarMatcher];
}

- (void)testSendTabToSelfAndVerifyErrorSnackbar {
  [ChromeEarlGrey addFakeSyncServerDeviceInfo:kTargetDeviceName
                         lastUpdatedTimestamp:base::Time::Now()];
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [ChromeEarlGrey
      loadURL:self.testServer->GetURL(
                  "/send_tab_to_self/send_tab_to_self_active_page.html")];
  [ChromeEarlGrey waitForWebStateContainingElement:TargetElement()];

  [ChromeEarlGreyUI shareCurrentPage];
  NSString* sendTabToSelf =
      l10n_util::GetNSString(IDS_IOS_SEND_TAB_TO_SELF_TARGET_DEVICE_ACTION);
  [ChromeEarlGrey tapButtonInActivitySheetWithID:sendTabToSelf];

  // Verify the device is shown in the device picker.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:grey_accessibilityLabel(
                                                       kTargetDeviceName)];

  // Simulate network disconnection for the fake sync server.
  [ChromeEarlGrey disconnectFakeSyncServerNetwork];
  [self addTeardownBlock:^{
    [ChromeEarlGrey connectFakeSyncServerNetwork];
  }];

  // Tap "Send".
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          @"kSendTabToSelfModalSendButton")]
      performAction:grey_tap()];

  // Verify that the bottom sheet is dismissed after the failure.
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:
                      grey_accessibilityID(@"kSendTabToSelfModalSendButton")];

  // Wait for and verify the error snackbar message ("Something went wrong.
  // Check your internet connection and try again.").
  NSString* errorSnackbarMessage =
      l10n_util::GetNSString(IDS_SEND_TAB_TO_SELF_POST_SEND_NO_INTERNET_TOAST);
  id<GREYMatcher> snackbarMatcher = grey_allOf(
      chrome_test_util::SnackbarViewMatcher(),
      grey_descendant(grey_accessibilityLabel(errorSnackbarMessage)), nil);
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

  // Verify that the page has NOT scrolled down. Wait for a short duration to
  // ensure any pending async scrolls do not occur.
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

  // Verify that the page has NOT scrolled down. Wait for a short duration to
  // ensure any pending async scrolls do not occur.
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
          IDS_IOS_SEND_TAB_TO_SELF_TARGET_DEVICE_ACTION);
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
          IDS_IOS_SEND_TAB_TO_SELF_TARGET_DEVICE_ACTION);
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

  // Dismiss the signin snackbar, which would otherwise overlap and obscure the
  // target device picker bottom sheet.
  DismissSnackbar();

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
          IDS_IOS_SEND_TAB_TO_SELF_TARGET_DEVICE_ACTION);
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

@interface SendTabToSelfCoordinatorAutoOpenTestCase : ChromeTestCase
@end

@implementation SendTabToSelfCoordinatorAutoOpenTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.features_enabled.push_back(
      send_tab_to_self::kSendTabToSelfPropagateScrollPosition);
  config.features_enabled.push_back(
      send_tab_to_self::kSendTabToSelfPropagateFormFields);
  config.features_enabled.push_back(
      send_tab_to_self::kSendTabToSelfExtraEntryPoints);
  config.features_enabled.push_back(send_tab_to_self::kSendTabToSelfAutoOpen);
  return config;
}

// Tests that when kSendTabToSelfAutoOpen is enabled, receiving a shared tab
// while active in the foreground automatically opens it as a background tab
// and presents a snackbar banner.
- (void)testSendTabToSelfAutoOpenWhenReceivedInForeground {
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [ChromeEarlGrey addFakeSyncServerDeviceInfo:kTargetDeviceName
                         lastUpdatedTimestamp:base::Time::Now()];

  // Load a starting page so there is an active, visible WebState.
  [ChromeEarlGrey loadURL:GURL("about:blank")];

  NSUInteger initialTabCount = [ChromeEarlGrey mainTabCount];

  [ChromeEarlGrey addFakeSyncServerSendTabToSelfEntryWithURL:kExampleURL
                                                       title:@"AutoOpen Page"
                                                  deviceName:@"remote_device"
                                            targetDeviceGUID:@""];

  [ChromeEarlGrey triggerSyncCycleForType:syncer::SEND_TAB_TO_SELF];

  // Verify that a background tab was opened automatically (tab count increased
  // by 1).
  [ChromeEarlGrey waitForMainTabCount:initialTabCount + 1];

  // Verify that the InfoBar message banner is displayed with correct title and
  // subtitle.
  NSString* title =
      l10n_util::GetNSString(IDS_SEND_TAB_TO_SELF_INFOBAR_AUTO_OPEN_TITLE);
  NSString* subtitle = l10n_util::GetNSStringF(
      IDS_SEND_TAB_TO_SELF_INFOBAR_AUTO_OPEN_SUBTITLE, u"remote_device");
  NSString* combinedLabel =
      [NSString stringWithFormat:@"%@,%@", title, subtitle];
  id<GREYMatcher> labelsStackMatcher =
      grey_allOf(grey_accessibilityID(kInfobarBannerLabelsStackViewIdentifier),
                 grey_accessibilityLabel(combinedLabel), nil);
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:labelsStackMatcher];

  // Tap "Open" on the banner and verify that the received tab is opened
  // directly in the foreground.
  NSString* buttonText =
      l10n_util::GetNSString(IDS_SEND_TAB_TO_SELF_INFOBAR_MESSAGE_URL);
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(buttonText),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  [ChromeEarlGrey
      waitForWebStateVisibleURL:GURL(base::SysNSStringToUTF8(kExampleURL))];
}

// Tests that when kSendTabToSelfAutoOpen is enabled and a shared tab is
// received while the active WebState is hidden (e.g. in the Tab Grid), it is
// not opened immediately but is automatically opened as a background tab when
// an active WebState is brought back to the foreground.
- (void)testSendTabToSelfAutoOpenWhenBroughtToForeground {
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [ChromeEarlGrey addFakeSyncServerDeviceInfo:kTargetDeviceName
                         lastUpdatedTimestamp:base::Time::Now()];

  // Load a starting page.
  [ChromeEarlGrey loadURL:GURL("about:blank")];

  NSUInteger initialTabCount = [ChromeEarlGrey mainTabCount];

  // Open the Tab Grid so the active WebState is no longer visible.
  [ChromeEarlGreyUI openTabGrid];

  [ChromeEarlGrey addFakeSyncServerSendTabToSelfEntryWithURL:kExampleURL
                                                       title:@"AutoOpen Page"
                                                  deviceName:@"remote_device"
                                            targetDeviceGUID:@""];

  [ChromeEarlGrey triggerSyncCycleForType:syncer::SEND_TAB_TO_SELF];

  // While in the Tab Grid, the tab should not be opened automatically yet.
  GREYAssertEqual(initialTabCount, [ChromeEarlGrey mainTabCount],
                  @"Tab count should not change while in Tab Grid.");

  // Leave the Tab Grid to bring the active WebState back to the foreground.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      performAction:grey_tap()];

  // Verify that the pending entry was now opened automatically in the
  // background.
  [ChromeEarlGrey waitForMainTabCount:initialTabCount + 1];

  // Verify that the InfoBar message banner is displayed with correct title and
  // subtitle.
  NSString* title =
      l10n_util::GetNSString(IDS_SEND_TAB_TO_SELF_INFOBAR_AUTO_OPEN_TITLE);
  NSString* subtitle = l10n_util::GetNSStringF(
      IDS_SEND_TAB_TO_SELF_INFOBAR_AUTO_OPEN_SUBTITLE, u"remote_device");
  NSString* combinedLabel =
      [NSString stringWithFormat:@"%@,%@", title, subtitle];
  id<GREYMatcher> labelsStackMatcher =
      grey_allOf(grey_accessibilityID(kInfobarBannerLabelsStackViewIdentifier),
                 grey_accessibilityLabel(combinedLabel), nil);
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:labelsStackMatcher];
}

// Tests that when a shared tab is auto-opened, its tab card in the Tab Grid
// displays the "From remote_device" activity label, and that the label
// disappears once the tab is viewed.
- (void)testTabCardLabelDisplayedInTabGrid {
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [ChromeEarlGrey addFakeSyncServerDeviceInfo:kTargetDeviceName
                         lastUpdatedTimestamp:base::Time::Now()];

  // Load a starting page so there is an active, visible WebState.
  [ChromeEarlGrey loadURL:GURL("about:blank")];

  NSUInteger initialTabCount = [ChromeEarlGrey mainTabCount];

  // Receive a shared tab.
  [ChromeEarlGrey addFakeSyncServerSendTabToSelfEntryWithURL:kExampleURL
                                                       title:@"AutoOpen Page"
                                                  deviceName:@"remote_device"
                                            targetDeviceGUID:@""];
  [ChromeEarlGrey triggerSyncCycleForType:syncer::SEND_TAB_TO_SELF];

  // Wait for the background tab to open.
  [ChromeEarlGrey waitForMainTabCount:initialTabCount + 1];

  // Enter the Tab Grid.
  [ChromeEarlGreyUI openTabGrid];

  // Verify that the activity label "From remote_device" is visible.
  NSString* labelText = l10n_util::GetNSStringF(
      IDS_SEND_TAB_TO_SELF_INFOBAR_AUTO_OPEN_SUBTITLE, u"remote_device");
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(labelText),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];

  // Tap the newly opened tab (index 1) to view it.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(1)]
      performAction:grey_tap()];

  // Enter the Tab Grid again.
  [ChromeEarlGreyUI openTabGrid];

  // Verify that the label is now gone.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(labelText),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];
}

// Tests that the tab card activity label is correctly persisted and restored
// across app relaunch, and is dismissed once the tab is viewed.
- (void)testTabCardLabelPersistsAcrossRelaunch {
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [ChromeEarlGrey addFakeSyncServerDeviceInfo:kTargetDeviceName
                         lastUpdatedTimestamp:base::Time::Now()];

  // Load a starting page.
  [ChromeEarlGrey loadURL:GURL("about:blank")];

  NSUInteger initialTabCount = [ChromeEarlGrey mainTabCount];

  // Receive a shared tab.
  [ChromeEarlGrey addFakeSyncServerSendTabToSelfEntryWithURL:kExampleURL
                                                       title:@"AutoOpen Page"
                                                  deviceName:@"remote_device"
                                            targetDeviceGUID:@""];
  [ChromeEarlGrey triggerSyncCycleForType:syncer::SEND_TAB_TO_SELF];

  // Wait for the background tab to open.
  [ChromeEarlGrey waitForMainTabCount:initialTabCount + 1];

  // Enter the Tab Grid.
  [ChromeEarlGreyUI openTabGrid];

  // Verify that the activity label "From remote_device" is visible.
  NSString* labelText = l10n_util::GetNSStringF(
      IDS_SEND_TAB_TO_SELF_INFOBAR_AUTO_OPEN_SUBTITLE, u"remote_device");
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(labelText),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];

  // Relaunch the app with the fake identity.
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
  config.additional_args.push_back(base::StrCat({
    "-", test_switches::kAddFakeIdentitiesAtStartup, "=",
        [FakeSystemIdentity encodeIdentitiesToBase64:@[ identity ]]
  }));
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  // Enter the Tab Grid.
  [ChromeEarlGreyUI openTabGrid];

  // Verify that the label is still visible after restart.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(labelText),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];

  // Tap the tab (index 1) to view it.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(1)]
      performAction:grey_tap()];

  // Enter the Tab Grid again.
  [ChromeEarlGreyUI openTabGrid];

  // Verify that the label is now gone.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(labelText),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];
}

// Tests that when a shared tab is auto-opened, the activation tracking survives
// an app relaunch, and viewing the restored tab from the Tab Grid correctly
// logs the activation metrics (both the entry point and the time from opened to
// activated).
- (void)testTabCardActivationLogsMetrics {
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [ChromeEarlGrey addFakeSyncServerDeviceInfo:kTargetDeviceName
                         lastUpdatedTimestamp:base::Time::Now()];

  // Load a starting page so there is an active, visible WebState.
  [ChromeEarlGrey loadURL:GURL("about:blank")];

  NSUInteger initialTabCount = [ChromeEarlGrey mainTabCount];

  // Receive a shared tab.
  [ChromeEarlGrey addFakeSyncServerSendTabToSelfEntryWithURL:kExampleURL
                                                       title:@"AutoOpen Page"
                                                  deviceName:@"remote_device"
                                            targetDeviceGUID:@""];
  [ChromeEarlGrey triggerSyncCycleForType:syncer::SEND_TAB_TO_SELF];

  // Wait for the background tab to open.
  [ChromeEarlGrey waitForMainTabCount:initialTabCount + 1];

  // Relaunch the app with the fake identity.
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
  config.additional_args.push_back(base::StrCat({
    "-", test_switches::kAddFakeIdentitiesAtStartup, "=",
        [FakeSystemIdentity encodeIdentitiesToBase64:@[ identity ]]
  }));
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  // Setup the histogram tester AFTER relaunch, since relaunching wipes the
  // previous app-side histogram tester.
  GREYAssertNil([MetricsAppInterface setupHistogramTester],
                @"Cannot setup histogram tester.");
  [MetricsAppInterface overrideMetricsAndCrashReportingForTesting];
  [self addTeardownBlock:^{
    [MetricsAppInterface stopOverridingMetricsAndCrashReportingForTesting];
    GREYAssertNil([MetricsAppInterface releaseHistogramTester],
                  @"Cannot reset histogram tester.");
  }];

  // Enter the Tab Grid (this triggers lazy recreation of the card label and
  // re-attaches the tracker).
  [ChromeEarlGreyUI openTabGrid];

  // Verify that the activation metrics histograms have NOT been logged yet.
  GREYAssertNil(
      [MetricsAppInterface
          expectTotalCount:0
              forHistogram:@"Sharing.SendTabToSelf.ActivatedEntryPoint"],
      @"Sharing.SendTabToSelf.ActivatedEntryPoint logged prematurely.");
  GREYAssertNil(
      [MetricsAppInterface
          expectTotalCount:0
              forHistogram:@"Sharing.SendTabToSelf.TimeOpenedToActivated"],
      @"Sharing.SendTabToSelf.TimeOpenedToActivated logged prematurely.");

  // Tap the restored background tab (index 1) to view/activate it.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(1)]
      performAction:grey_tap()];

  // Verify that the activation metrics histograms were logged successfully.
  // 1. Sharing.SendTabToSelf.ActivatedEntryPoint should have 1 sample in bucket
  // 4 (kTabStrip).
  GREYAssertNil(
      [MetricsAppInterface
          expectUniqueSampleWithCount:1
                            forBucket:4  // ShareActivatedEntryPoint::kTabStrip
                                         // is 4
                         forHistogram:
                             @"Sharing.SendTabToSelf.ActivatedEntryPoint"],
      @"Sharing.SendTabToSelf.ActivatedEntryPoint histogram not logged.");

  // 2. Sharing.SendTabToSelf.TimeOpenedToActivated should have exactly 1
  // sample.
  GREYAssertNil(
      [MetricsAppInterface
          expectTotalCount:1
              forHistogram:@"Sharing.SendTabToSelf.TimeOpenedToActivated"],
      @"Sharing.SendTabToSelf.TimeOpenedToActivated histogram not logged.");
}

@end
