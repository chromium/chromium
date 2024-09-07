// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/feature_list.h"
#import "components/signin/internal/identity_manager/account_capabilities_constants.h"
#import "components/supervised_user/core/common/features.h"
#import "components/supervised_user/core/common/supervised_user_constants.h"
#import "ios/chrome/browser/policy/model/policy_earl_grey_matchers.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
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
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

using chrome_test_util::ContainsPartialText;
using chrome_test_util::ShowTabsButton;
using chrome_test_util::TabGridIncognitoTabsPanelButton;
using chrome_test_util::TabGridNewIncognitoTabButton;

namespace {

// Message shown in the disabled incognito tab page for supervised users.
NSString* const kTestSupervisedIncognitoMessage =
    @"Your account is managed by your parent.";

// Label used to find the 'Learn more' link.
NSString* const kTestLearnMoreLabel = @"Learn more";

}  // namespace

// Tests that supervised users have incognito mode disabled.
@interface SupervisedUserIncognitoModeTestCase : ChromeTestCase
@end

@implementation SupervisedUserIncognitoModeTestCase

// Signs in with a supervised account.
- (void)signInWithSupervisedAccount {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity
                 withCapabilities:@{
                   @(kIsSubjectToParentalControlsCapabilityName) : @YES,
                 }];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
}

// Tests that the tools menu item "New Incognito Tab" is disabled on signin and
// re-enabled on signout.
- (void)testToolsMenuIncognito {
  [self signInWithSupervisedAccount];

  [ChromeEarlGreyUI openToolsMenu];
  policy::AssertOverflowMenuElementEnabled(kToolsMenuNewTabId);
  policy::AssertOverflowMenuElementDisabled(kToolsMenuNewIncognitoTabId);
  [ChromeEarlGreyUI closeToolsMenu];

  [SigninEarlGrey signOut];

  [ChromeEarlGreyUI openToolsMenu];
  policy::AssertOverflowMenuElementEnabled(kToolsMenuNewTabId);
  policy::AssertOverflowMenuElementEnabled(kToolsMenuNewIncognitoTabId);
}

// Tests that the "New Incognito Tab" item in the popup menu (long-pressing the
// tab grid button) is disabled on signin and re-enabled on signout.
- (void)testTabGridButtonLongPressMenuIncognito {
  [self signInWithSupervisedAccount];

  [[EarlGrey selectElementWithMatcher:ShowTabsButton()]
      performAction:grey_longPress()];
  policy::AssertButtonInCollectionEnabled(IDS_IOS_TOOLS_MENU_NEW_TAB);
  policy::AssertButtonInCollectionDisabled(
      IDS_IOS_TOOLS_MENU_NEW_INCOGNITO_TAB);

  // Dismiss the popup menu by tapping anywhere.
  [[EarlGrey selectElementWithMatcher:ShowTabsButton()]
      performAction:grey_tap()];

  [SigninEarlGrey signOut];

  [[EarlGrey selectElementWithMatcher:ShowTabsButton()]
      performAction:grey_longPress()];
  policy::AssertButtonInCollectionEnabled(IDS_IOS_TOOLS_MENU_NEW_TAB);
  policy::AssertButtonInCollectionEnabled(IDS_IOS_TOOLS_MENU_NEW_INCOGNITO_TAB);
}

// Tests that the disabled incognito tab grid shows a link to Family Link.
- (void)testTabGridIncognitoDisabled {
  [self signInWithSupervisedAccount];

  // Open incognito tab grid.
  [ChromeEarlGrey showTabSwitcher];
  [[EarlGrey selectElementWithMatcher:TabGridIncognitoTabsPanelButton()]
      performAction:grey_tap()];

  // New Incognito Tab button `(+)` should be disabled.
  [[EarlGrey selectElementWithMatcher:TabGridNewIncognitoTabButton()]
      assertWithMatcher:grey_accessibilityTrait(
                            UIAccessibilityTraitNotEnabled)];

  // The disabled incognito tab grid should display a message for supervised
  // users.
  [[EarlGrey selectElementWithMatcher:ContainsPartialText(
                                          kTestSupervisedIncognitoMessage)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Check that the "Learn more" link works.
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
      GURL(supervised_user::kManagedByParentUiMoreInfoUrl).host();
  GREYAssertEqual([ChromeEarlGrey webStateLastCommittedURL].host(),
                  expectedHostname,
                  @"Did not open the correct Learn more URL with hostname %s",
                  expectedHostname.c_str());
}

// Tests that the incognito tab grid is available after signout.
- (void)testTabGridIncognitoEnabledOnSignout {
  [self signInWithSupervisedAccount];
  [SigninEarlGrey signOut];

  // Open the incognito tab grid.
  [ChromeEarlGrey showTabSwitcher];
  [[EarlGrey selectElementWithMatcher:TabGridIncognitoTabsPanelButton()]
      performAction:grey_tap()];

  // New Incognito Tab button `(+)` should be re-enabled.
  [[EarlGrey selectElementWithMatcher:TabGridNewIncognitoTabButton()]
      assertWithMatcher:grey_not(grey_accessibilityTrait(
                            UIAccessibilityTraitNotEnabled))];

  // The disabled incognito tab should not display any messages from the
  // disabled incognito tab grid.
  [[EarlGrey selectElementWithMatcher:ContainsPartialText(
                                          kTestSupervisedIncognitoMessage)]
      assertWithMatcher:grey_nil()];
}

// Tests that the incognito tab grid is available after signout with restart.
// Regression test for b/360787816.
- (void)testTabGridIncognitoEnabledOnSignoutWithRestart {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity
                 withCapabilities:@{
                   @(kIsSubjectToParentalControlsCapabilityName) : @YES,
                 }];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];

  // Create the config to relaunch Chrome. Because fake identities are not
  // added on startup, manually configure fake identity cache to include the
  // supervised account.
  AppLaunchConfiguration config;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.additional_args.push_back(
      base::StrCat({"--", test_switches::kSignInAtStartup}));
  config.additional_args.push_back(base::StrCat({
    "-", test_switches::kAddFakeIdentitiesAtStartup, "=",
        [FakeSystemIdentity encodeIdentitiesToBase64:@[ fakeIdentity ]]
  }));
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  [SigninEarlGrey signOut];

  // Open the incognito tab grid.
  [ChromeEarlGrey showTabSwitcher];
  [[EarlGrey selectElementWithMatcher:TabGridIncognitoTabsPanelButton()]
      performAction:grey_tap()];

  // New Incognito Tab button `(+)` should be re-enabled.
  [[EarlGrey selectElementWithMatcher:TabGridNewIncognitoTabButton()]
      assertWithMatcher:grey_not(grey_accessibilityTrait(
                            UIAccessibilityTraitNotEnabled))];

  // The disabled incognito tab should not display any messages from the
  // disabled incognito tab grid.
  [[EarlGrey selectElementWithMatcher:ContainsPartialText(
                                          kTestSupervisedIncognitoMessage)]
      assertWithMatcher:grey_nil()];
}

// Tests that incognito tabs are destroyed after supervised users sign in.
- (void)testIncognitoTabsDestroyedOnSignin {
  // Create new incognito tabs.
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey openNewIncognitoTab];
  GREYAssertEqual(2, [ChromeEarlGrey incognitoTabCount],
                  @"Incognito tab count should be 2");

  // The latest incognito tab is displayed.
  GREYAssertTrue([ChromeEarlGrey isIncognitoMode],
                 @"Should stay in incognito mode");

  [self signInWithSupervisedAccount];

  // All incognito tabs should be destroyed.
  GREYAssertEqual(0, [ChromeEarlGrey incognitoTabCount],
                  @"Incognito tab count should be 0");

  // If the supervised user was previously on an incognito tab, the disabled
  // incognito tab grid should be displayed.
  [[EarlGrey selectElementWithMatcher:ContainsPartialText(
                                          kTestSupervisedIncognitoMessage)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Check that the edit button is disabled.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridEditButton()]
      assertWithMatcher:grey_not(grey_enabled())];
}

// Tests that incognito tabs are destroyed while supervised users stay in
// a regular tab.
- (void)testIncognitoTabsDestroyedOnSigninInBackground {
  // Create new incognito tabs.
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey openNewIncognitoTab];
  GREYAssertEqual(2, [ChromeEarlGrey incognitoTabCount],
                  @"Incognito tab count should be 2");

  // Open a new regular tab.
  [ChromeEarlGrey openNewTab];
  GREYAssertFalse([ChromeEarlGrey isIncognitoMode],
                  @"Should stay in regular tab.");

  [self signInWithSupervisedAccount];

  // All incognito tabs should be destroyed.
  GREYAssertEqual(0, [ChromeEarlGrey incognitoTabCount],
                  @"Incognito tab count should be 0");

  // The user should stay on the new tab page.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NewTabPageOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

@end
