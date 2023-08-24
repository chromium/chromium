// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/feature_list.h"
#import "base/strings/sys_string_conversions.h"
#import "components/supervised_user/core/common/features.h"
#import "ios/chrome/browser/policy/policy_earl_grey_matchers.h"
#import "ios/chrome/browser/signin/capabilities_types.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_constants.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Message shown in the disabled incognito tab page for supervised users.
NSString* const kTestSupervisedIncognitoMessage =
    @"Your account is managed by your parent.";

// Label used to find the 'Learn More' link.
NSString* const kTestLearnMoreLabel = @"Learn More";

}  // namespace

// Tests that supervised users have incognito mode disabled.
@interface SupervisedUserIncognitoModeTestCase : ChromeTestCase
@end

@implementation SupervisedUserIncognitoModeTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(
      supervised_user::kFilterWebsitesForSupervisedUsersOnDesktopAndIOS);
  return config;
}

- (void)setUp {
  [super setUp];
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [SigninEarlGrey setIsSubjectToParentalControls:YES forIdentity:fakeIdentity];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableSync:NO];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
}

- (void)tearDown {
  [super tearDown];
}

// Test that the tools menu item "New Incognito Tab" is disabled.
- (void)testToolsMenuIncognitoDisabled {
  [ChromeEarlGreyUI openToolsMenu];

  policy::AssertOverflowMenuElementEnabled(kToolsMenuNewTabId);
  policy::AssertOverflowMenuElementDisabled(kToolsMenuNewIncognitoTabId);
}

// Test that the tools menu item "New Incognito Tab" is available after signout.
- (void)testToolsMenuIncognitoEnabledOnSignout {
  [SigninEarlGreyUI
      signOutWithConfirmationChoice:SignOutConfirmationChoiceNotSyncing];
  [SigninEarlGrey verifySignedOut];

  [ChromeEarlGreyUI openToolsMenu];

  policy::AssertOverflowMenuElementEnabled(kToolsMenuNewTabId);
  policy::AssertOverflowMenuElementEnabled(kToolsMenuNewIncognitoTabId);
}

// Test that the "New Incognito Tab" item is disabled in the popup menu
// triggered by long-pressing the tab grid button.
- (void)testTabGridButtonLongPressMenuIncognitoDisabled {
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_TOOLBAR_SHOW_TABS)]
      performAction:grey_longPress()];

  policy::AssertButtonInCollectionEnabled(IDS_IOS_TOOLS_MENU_NEW_TAB);
  policy::AssertButtonInCollectionDisabled(
      IDS_IOS_TOOLS_MENU_NEW_INCOGNITO_TAB);
}

// Test that the "New Incognito Tab" item is available in the popup menu
// triggered by long-pressing the tab grid button after signout.
- (void)testTabGridButtonLongPressMenuIncognitoEnabledOnSignout {
  [SigninEarlGreyUI
      signOutWithConfirmationChoice:SignOutConfirmationChoiceNotSyncing];
  [SigninEarlGrey verifySignedOut];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_TOOLBAR_SHOW_TABS)]
      performAction:grey_longPress()];

  policy::AssertButtonInCollectionEnabled(IDS_IOS_TOOLS_MENU_NEW_TAB);
  policy::AssertButtonInCollectionEnabled(IDS_IOS_TOOLS_MENU_NEW_INCOGNITO_TAB);
}

// Test that the disabled incognito tab grid shows a link to Family Link.
- (void)testTabGridIncognitoDisabled {
  [ChromeEarlGrey showTabSwitcher];

  // Open incognito tab grid.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridIncognitoTabsPanelButton()]
      performAction:grey_tap()];

  // New Incognito Tab button `(+)` should be disabled.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridNewIncognitoTabButton()]
      assertWithMatcher:grey_accessibilityTrait(
                            UIAccessibilityTraitNotEnabled)];

  // The disabled incognito tab grid should display a message for supervised
  // users.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ContainsPartialText(
                                          kTestSupervisedIncognitoMessage)]
      assertWithMatcher:grey_notNil()];

  // Check that the "Learn More" link works.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityLabel(kTestLearnMoreLabel),
                                   grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  // Wait for the Family Link page to finish loading.
  [ChromeEarlGrey waitForPageToFinishLoading];

  // For testing, there will be a redirect to the main Family Link website and
  // thus we only compare the hostnames.
  std::string expectedHostname =
      GURL(supervised_user::kManagedByParentUiMoreInfoUrl.Get()).host();
  GREYAssertEqual([ChromeEarlGrey webStateLastCommittedURL].host(),
                  expectedHostname,
                  @"Did not open the correct Learn more URL with hostname %s",
                  expectedHostname.c_str());
}

// Test that the incognito tab grid is available after signout.
- (void)testTabGridIncognitoEnabledOnSignout {
  [SigninEarlGreyUI
      signOutWithConfirmationChoice:SignOutConfirmationChoiceNotSyncing];
  [SigninEarlGrey verifySignedOut];

  [ChromeEarlGrey showTabSwitcher];

  // Open incognito tab grid.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridIncognitoTabsPanelButton()]
      performAction:grey_tap()];

  // New Incognito Tab button `(+)` should be enabled.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridNewIncognitoTabButton()]
      assertWithMatcher:grey_not(grey_accessibilityTrait(
                            UIAccessibilityTraitNotEnabled))];

  // The disabled incognito tab should not display any messages from the
  // disabled incognito tab grid.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ContainsPartialText(
                                          kTestSupervisedIncognitoMessage)]
      assertWithMatcher:grey_nil()];
}

@end
