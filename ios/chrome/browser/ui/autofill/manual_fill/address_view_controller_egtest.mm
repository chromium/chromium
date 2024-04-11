// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/autofill/autofill_app_interface.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_constants.h"
#import "ios/chrome/common/ui/elements/form_input_accessory_view.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/element_selector.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

using base::test::ios::kWaitForActionTimeout;
using chrome_test_util::ManualFallbackFormSuggestionViewMatcher;
using chrome_test_util::ManualFallbackKeyboardIconMatcher;
using chrome_test_util::ManualFallbackManageProfilesMatcher;
using chrome_test_util::ManualFallbackProfilesIconMatcher;
using chrome_test_util::ManualFallbackProfilesTableViewMatcher;
using chrome_test_util::ManualFallbackProfileTableViewWindowMatcher;
using chrome_test_util::NavigationBarCancelButton;
using chrome_test_util::SettingsProfileMatcher;

namespace {

constexpr char kFormElementName[] = "name";
constexpr char kFormElementCity[] = "city";

constexpr char kFormHTMLFile[] = "/profile_form.html";

// Waits for the keyboard to appear. Returns NO on timeout.
BOOL WaitForKeyboardToAppear() {
  GREYCondition* waitForKeyboard = [GREYCondition
      conditionWithName:@"Wait for keyboard"
                  block:^BOOL {
                    return [EarlGrey isKeyboardShownWithError:nil];
                  }];
  return [waitForKeyboard waitWithTimeout:kWaitForActionTimeout.InSecondsF()];
}

// Opens the address manual fill view and verifies that the address view
// controller is visible afterwards.
void OpenAddressManualFillView() {
  id<GREYMatcher> button_to_tap;
  if ([AutofillAppInterface isKeyboardAccessoryUpgradeEnabled]) {
    button_to_tap = grey_accessibilityLabel(
        l10n_util::GetNSString(IDS_IOS_AUTOFILL_ACCNAME_AUTOFILL_DATA));
  } else {
    [[EarlGrey
        selectElementWithMatcher:ManualFallbackFormSuggestionViewMatcher()]
        performAction:grey_scrollToContentEdge(kGREYContentEdgeRight)];
    button_to_tap = ManualFallbackProfilesIconMatcher();
  }

  // Tap the button that'll open the address manual fill view.
  [[EarlGrey selectElementWithMatcher:button_to_tap] performAction:grey_tap()];

  // Verify the address table view controller is visible.
  [[EarlGrey selectElementWithMatcher:ManualFallbackProfilesTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Matcher for the expanded address manual fill view button.
id<GREYMatcher> AddressManualFillViewButton() {
  return grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(
                        IDS_IOS_AUTOFILL_ADDRESS_AUTOFILL_DATA)),
                    grey_ancestor(grey_accessibilityID(
                        kFormInputAccessoryViewAccessibilityID)),
                    nil);
}

// Matcher for the address tab in the manual fill view.
id<GREYMatcher> AddressManualFillViewTab() {
  return grey_allOf(
      grey_accessibilityLabel(l10n_util::GetNSString(
          IDS_IOS_EXPANDED_MANUAL_FILL_ADDRESS_TAB_ACCESSIBILITY_LABEL)),
      grey_ancestor(
          grey_accessibilityID(manual_fill::kExpandedManualFillHeaderViewID)),
      nil);
}

// Opens the address manual fill view when there are no saved addresses and
// verifies that the address view controller is visible afterwards. Only useful
// when the `kIOSKeyboardAccessoryUpgrade` feature is enabled.
void OpenAddressManualFillViewWithNoSavedAddresses() {
  // Tap the button to open the expanded manual fill view.
  [[EarlGrey selectElementWithMatcher:AddressManualFillViewButton()]
      performAction:grey_tap()];

  // Tap the address tab from the segmented control.
  [[EarlGrey selectElementWithMatcher:AddressManualFillViewTab()]
      performAction:grey_tap()];

  // Verify the address table view controller is visible.
  [[EarlGrey selectElementWithMatcher:ManualFallbackProfilesTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

}  // namespace

// Integration Tests for Mannual Fallback Addresses View Controller.
@interface AddressViewControllerTestCase : ChromeTestCase
@end

@implementation AddressViewControllerTestCase

- (BOOL)shouldEnableKeyboardAccessoryUpgradeFeature {
  return YES;
}

- (void)setUp {
  [super setUp];
  [AutofillAppInterface clearProfilesStore];
  [AutofillAppInterface saveExampleProfile];

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL URL = self.testServer->GetURL(kFormHTMLFile);
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:"Profile form"];
}

- (void)tearDown {
  [AutofillAppInterface clearProfilesStore];
  [super tearDown];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;

  if ([self shouldEnableKeyboardAccessoryUpgradeFeature]) {
    config.features_enabled.push_back(kIOSKeyboardAccessoryUpgrade);
  } else {
    config.features_disabled.push_back(kIOSKeyboardAccessoryUpgrade);
  }

  return config;
}

// Tests that the addresses view controller appears on screen.
// TODO(crbug.com/1116043): Flaky on ios simulator.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_testAddressesViewControllerIsPresented \
  DISABLED_testAddressesViewControllerIsPresented
#else
#define MAYBE_testAddressesViewControllerIsPresented \
  testAddressesViewControllerIsPresented
#endif
- (void)MAYBE_testAddressesViewControllerIsPresented {
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementName)];

  // Open the address manual fill view and verify that the address table view
  // controller is visible.
  OpenAddressManualFillView();
}

// Tests that the "Manage Addresses..." action works.
- (void)testManageAddressesActionOpensAddressSettings {
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementName)];

  // Open the address manual fill view.
  OpenAddressManualFillView();

  // Tap the "Manage Addresses..." action.
  [[EarlGrey selectElementWithMatcher:ManualFallbackProfilesTableViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];
  [[EarlGrey selectElementWithMatcher:ManualFallbackManageProfilesMatcher()]
      performAction:grey_tap()];

  // Verify the address settings opened.
  [[EarlGrey selectElementWithMatcher:SettingsProfileMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that returning from "Manage Addresses..." leaves the icons and keyboard
// in the right state.
- (void)testAddressesStateAfterPresentingManageAddresses {
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementName)];

  // Open the address manual fill view.
  OpenAddressManualFillView();

  // Icons are not present when the Keyboard Accessory Upgrade feature is
  // enabled.
  if (![AutofillAppInterface isKeyboardAccessoryUpgradeEnabled]) {
    // Verify the status of the icon.
    [[EarlGrey selectElementWithMatcher:ManualFallbackProfilesIconMatcher()]
        assertWithMatcher:grey_not(grey_userInteractionEnabled())];
  }

  // Tap the "Manage Addresses..." action.
  [[EarlGrey selectElementWithMatcher:ManualFallbackProfilesTableViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];
  [[EarlGrey selectElementWithMatcher:ManualFallbackManageProfilesMatcher()]
      performAction:grey_tap()];

  // Verify the address settings opened.
  [[EarlGrey selectElementWithMatcher:SettingsProfileMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap Cancel Button.
  [[EarlGrey selectElementWithMatcher:NavigationBarCancelButton()]
      performAction:grey_tap()];

  // Verify the address settings closed.
  [[EarlGrey selectElementWithMatcher:SettingsProfileMatcher()]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];

  // TODO(crbug.com/332956674): Keyboard and keyboard accessory are not present
  // on iOS 17.4+, remove version check once fixed.
  if (@available(iOS 17.4, *)) {
    // Skip verifications.
  } else {
    // Icons are not present when the Keyboard Accessory Upgrade feature is
    // enabled.
    if (![AutofillAppInterface isKeyboardAccessoryUpgradeEnabled]) {
      // Verify the status of the icons.
      [[EarlGrey
          selectElementWithMatcher:ManualFallbackFormSuggestionViewMatcher()]
          performAction:grey_scrollToContentEdge(kGREYContentEdgeRight)];
      [[EarlGrey selectElementWithMatcher:ManualFallbackProfilesIconMatcher()]
          assertWithMatcher:grey_sufficientlyVisible()];
      [[EarlGrey selectElementWithMatcher:ManualFallbackProfilesIconMatcher()]
          assertWithMatcher:grey_userInteractionEnabled()];
      [[EarlGrey selectElementWithMatcher:ManualFallbackKeyboardIconMatcher()]
          assertWithMatcher:grey_not(grey_sufficientlyVisible())];
    }

    // Verify the keyboard is not covered by the profiles view.
    GREYAssertTrue([EarlGrey isKeyboardShownWithError:nil],
                   @"Keyboard should be shown");
  }

  [[EarlGrey selectElementWithMatcher:ManualFallbackProfilesTableViewMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the Address View Controller is dismissed when tapping the
// keyboard icon.
- (void)testKeyboardIconDismissAddressController {
  if ([ChromeEarlGrey isIPadIdiom] ||
      [AutofillAppInterface isKeyboardAccessoryUpgradeEnabled]) {
    EARL_GREY_TEST_SKIPPED(
        @"The keyboard icon is never present on iPads or when the Keyboard "
        @"Accessory Upgrade feature is enabled.");
  }

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementName)];

  // Open the address manual fill view.
  OpenAddressManualFillView();

  // Tap on the keyboard icon.
  [[EarlGrey selectElementWithMatcher:ManualFallbackKeyboardIconMatcher()]
      performAction:grey_tap()];

  // Verify the address controller table view and the address icon is NOT
  // visible.
  [[EarlGrey selectElementWithMatcher:ManualFallbackProfilesTableViewMatcher()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:ManualFallbackKeyboardIconMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the Address View Controller is dismissed when tapping the outside
// the popover on iPad.
- (void)testIPadTappingOutsidePopOverDismissAddressController {
  if (![ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Test is not applicable for iPhone");
  }
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementName)];

  // Open the address manual fill view.
  OpenAddressManualFillView();

  // Tap on a point outside of the popover.
  // The way EarlGrey taps doesn't go through the window hierarchy. Because of
  // this, the tap needs to be done in the same window as the popover.
  [[EarlGrey
      selectElementWithMatcher:ManualFallbackProfileTableViewWindowMatcher()]
      performAction:grey_tapAtPoint(CGPointMake(0, 0))];

  // Verify the address controller table view and the address icon is NOT
  // visible.
  [[EarlGrey selectElementWithMatcher:ManualFallbackProfilesTableViewMatcher()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:ManualFallbackKeyboardIconMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the address icon is hidden when no addresses are available.
- (void)testAddressIconIsNotVisibleWhenAddressStoreEmpty {
  if ([AutofillAppInterface isKeyboardAccessoryUpgradeEnabled]) {
    EARL_GREY_TEST_SKIPPED(@"This test is not relevant when the Keyboard "
                           @"Accessory Upgrade feature is enabled.");
  }

  // Delete the profile that is added on `-setUp`.
  [AutofillAppInterface clearProfilesStore];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementName)];

  // Wait for the keyboard to appear.
  WaitForKeyboardToAppear();

  // Assert the address icon is not visible.
  [[EarlGrey selectElementWithMatcher:ManualFallbackProfilesIconMatcher()]
      assertWithMatcher:grey_notVisible()];

  // Store one address.
  [AutofillAppInterface saveExampleProfile];

  // Tap another field to trigger form activity.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementCity)];

  // Assert the address icon is visible now.
  [[EarlGrey selectElementWithMatcher:ManualFallbackFormSuggestionViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeRight)];
  // Verify the status of the icons.
  [[EarlGrey selectElementWithMatcher:ManualFallbackProfilesIconMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:ManualFallbackProfilesIconMatcher()]
      assertWithMatcher:grey_userInteractionEnabled()];
  [[EarlGrey selectElementWithMatcher:ManualFallbackKeyboardIconMatcher()]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];
}

// Tests that the the "no addresses found" message is visible when no address
// suggestions are available.
- (void)testNoAddressesFoundMessageIsVisibleWhenNoAddressSuggestions {
  if (![AutofillAppInterface isKeyboardAccessoryUpgradeEnabled]) {
    EARL_GREY_TEST_SKIPPED(@"This test is not relevant when the Keyboard "
                           @"Accessory Upgrade feature is disabled.");
  }

  [AutofillAppInterface clearProfilesStore];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementName)];

  // Open the address manual fill view.
  OpenAddressManualFillViewWithNoSavedAddresses();

  // Assert that the "no addresses found" message is visible.
  id<GREYMatcher> noAddressesFoundMessage = grey_accessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_MANUAL_FALLBACK_NO_ADDRESSES));
  [[EarlGrey selectElementWithMatcher:noAddressesFoundMessage]
      assertWithMatcher:grey_sufficientlyVisible()];
}

@end

// Rerun all the tests in this file but with kIOSKeyboardAccessoryUpgrade
// disabled. This will be removed once that feature launches fully, but ensures
// regressions aren't introduced in the meantime.
@interface AddressViewControllerKeyboardAccessoryUpgradeDisabledTestCase
    : AddressViewControllerTestCase

@end

@implementation AddressViewControllerKeyboardAccessoryUpgradeDisabledTestCase

- (BOOL)shouldEnableKeyboardAccessoryUpgradeFeature {
  return NO;
}

// This causes the test case to actually be detected as a test case. The actual
// tests are all inherited from the parent class.
- (void)testEmpty {
}

@end
