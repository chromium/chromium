// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"

#import "base/mac/foundation_util.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_constants.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_app_interface.h"
#import "ios/chrome/browser/ui/authentication/signin_matchers.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/unified_consent_constants.h"
#import "ios/chrome/browser/ui/authentication/views/views_constants.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/accounts_table_view_controller_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_matchers_app_interface.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::PrimarySignInButton;
using chrome_test_util::SecondarySignInButton;
using chrome_test_util::SettingsAccountButton;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SignOutAccountsButton;
using chrome_test_util::IdentityCellMatcherForEmail;

namespace {

// Closes the managed account dialog, if `fakeIdentity` is a managed account.
void CloseSigninManagedAccountDialogIfAny(FakeSystemIdentity* fakeIdentity) {
  // Don't expect a managed account dialog when the account isn't considered
  // managed.
  if ([fakeIdentity.userEmail hasSuffix:@"@gmail.com"]) {
    return;
  }

  // Synchronization off due to an infinite spinner, in the user consent view,
  // under the managed consent dialog. This spinner is started by the sign-in
  // process.
  ScopedSynchronizationDisabler disabler;
  id<GREYMatcher> acceptButton = [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:IDS_IOS_MANAGED_SIGNIN_ACCEPT_BUTTON];
  [ChromeEarlGrey waitForMatcher:acceptButton];
  [[EarlGrey selectElementWithMatcher:acceptButton] performAction:grey_tap()];
}

}  // namespace

@implementation SigninEarlGreyUI

+ (void)signinWithFakeIdentity:(FakeSystemIdentity*)fakeIdentity {
  [self signinWithFakeIdentity:fakeIdentity enableSync:YES];
}

+ (void)signinWithFakeIdentity:(FakeSystemIdentity*)fakeIdentity
                    enableSync:(BOOL)enableSync {
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  if (!enableSync) {
    [ChromeEarlGrey signInWithoutSyncWithIdentity:fakeIdentity];
    ConditionBlock condition = ^bool {
      return [[SigninEarlGreyAppInterface primaryAccountGaiaID]
          isEqual:fakeIdentity.gaiaID];
    };
    BOOL isSigned = base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForActionTimeout, condition);
    GREYAssert(
        isSigned, @"Signed in failed. Expected: %@, Currently signed: %@",
        fakeIdentity.gaiaID, [SigninEarlGreyAppInterface primaryAccountGaiaID]);
    return;
  }
  [ChromeEarlGreyUI openSettingsMenu];
  [[[EarlGrey selectElementWithMatcher:chrome_test_util::PrimarySignInButton()]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionUp, 150)
      onElementWithMatcher:chrome_test_util::SettingsCollectionView()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kIdentityButtonControlIdentifier)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:IdentityCellMatcherForEmail(
                                          fakeIdentity.userEmail)]
      performAction:grey_tap()];
  [self tapSigninConfirmationDialog];
  CloseSigninManagedAccountDialogIfAny(fakeIdentity);

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];

  // Sync utilities require sync to be initialized in order to perform
  // operations on the Sync server.
  [ChromeEarlGrey waitForSyncEngineInitialized:YES
                                   syncTimeout:base::Seconds(10)];
}

+ (void)signOutWithConfirmationChoice:(SignOutConfirmationChoice)confirmation {
  int confirmationLabelID = 0;
  switch (confirmation) {
    case SignOutConfirmationChoiceClearData:
      confirmationLabelID = IDS_IOS_SIGNOUT_DIALOG_CLEAR_DATA_BUTTON;
      break;
    case SignOutConfirmationChoiceKeepData:
      confirmationLabelID = IDS_IOS_SIGNOUT_DIALOG_KEEP_DATA_BUTTON;
      break;
    case SignOutConfirmationChoiceNotSyncing:
      confirmationLabelID = IDS_IOS_SIGNOUT_DIALOG_SIGN_OUT_BUTTON;
      break;
  }
  [self signOutWithButton:SignOutAccountsButton()
      confirmationLabelID:confirmationLabelID];
}

+ (void)tapSigninConfirmationDialog {
  // To confirm the dialog, the scroll view content has to be scrolled to the
  // bottom to transform "MORE" button into the validation button.
  // EarlGrey fails to scroll to the bottom, using grey_scrollToContentEdge(),
  // if the scroll view doesn't bounce and by default a scroll view doesn't
  // bounce when the content fits into the scroll view (the scroll never ends).
  // To test if the content fits into the scroll view,
  // ContentViewSmallerThanScrollView() matcher is used on the signin scroll
  // view.
  // If the matcher fails, then the scroll view should be scrolled to the
  // bottom.
  // Once to the bottom, the consent can be confirmed.
  [ChromeEarlGreyUI waitForAppToIdle];
  id<GREYMatcher> confirmationScrollViewMatcher =
      grey_accessibilityID(kUnifiedConsentScrollViewIdentifier);
  NSError* error = nil;
  [[EarlGrey selectElementWithMatcher:confirmationScrollViewMatcher]
      assertWithMatcher:chrome_test_util::ContentViewSmallerThanScrollView()
                  error:&error];
  if (error) {
    [[EarlGrey selectElementWithMatcher:confirmationScrollViewMatcher]
        performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];
  }
  id<GREYMatcher> buttonMatcher = [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:IDS_IOS_ACCOUNT_UNIFIED_CONSENT_OK_BUTTON];
  [[EarlGrey selectElementWithMatcher:buttonMatcher] performAction:grey_tap()];
}

+ (void)tapAddAccountButton {
  id<GREYMatcher> confirmationScrollViewMatcher =
      grey_accessibilityID(kUnifiedConsentScrollViewIdentifier);
  [ChromeEarlGreyUI waitForAppToIdle];
  NSError* error = nil;
  [[EarlGrey selectElementWithMatcher:confirmationScrollViewMatcher]
      assertWithMatcher:chrome_test_util::ContentViewSmallerThanScrollView()
                  error:&error];
  if (error) {
    // If the consent is bigger than the scroll view, the primary button should
    // be "MORE".
    [[EarlGrey selectElementWithMatcher:
                   chrome_test_util::ButtonWithAccessibilityLabelId(
                       IDS_IOS_ACCOUNT_CONSISTENCY_CONFIRMATION_SCROLL_BUTTON)]
        assertWithMatcher:grey_notNil()];
    [[EarlGrey selectElementWithMatcher:confirmationScrollViewMatcher]
        performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];
  }
  id<GREYMatcher> buttonMatcher = [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:
          IDS_IOS_ACCOUNT_UNIFIED_CONSENT_ADD_ACCOUNT];
  [[EarlGrey selectElementWithMatcher:buttonMatcher] performAction:grey_tap()];
}

+ (void)verifySigninPromoVisibleWithMode:(SigninPromoViewMode)mode {
  [self verifySigninPromoVisibleWithMode:mode closeButton:YES];
}

+ (void)verifySigninPromoVisibleWithMode:(SigninPromoViewMode)mode
                             closeButton:(BOOL)closeButton {
  [ChromeEarlGreyUI waitForAppToIdle];

  [[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];

  switch (mode) {
    case SigninPromoViewModeNoAccounts:
    case SigninPromoViewModeSyncWithPrimaryAccount:
      [[EarlGrey
          selectElementWithMatcher:grey_allOf(SecondarySignInButton(),
                                              grey_sufficientlyVisible(), nil)]
          assertWithMatcher:grey_nil()];
      break;
    case SigninPromoViewModeSigninWithAccount:
      // TODO(crbug.com/1210846): Determine when the SecondarySignInButton
      // should be present and assert that.
      break;
  }

  if (closeButton) {
    [[EarlGrey
        selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                                kSigninPromoCloseButtonId),
                                            grey_sufficientlyVisible(), nil)]
        assertWithMatcher:grey_notNil()];
  }
}

+ (void)verifySigninPromoNotVisible {
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(kSigninPromoViewId),
                                   grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(SecondarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];
}

+ (void)openRemoveAccountConfirmationDialogWithFakeIdentity:
    (FakeSystemIdentity*)fakeIdentity {
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(
                                          fakeIdentity.userEmail)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabel(
                                   l10n_util::GetNSString(
                                       IDS_IOS_REMOVE_GOOGLE_ACCOUNT_TITLE))]
      performAction:grey_tap()];
}

+ (void)openMyGoogleDialogWithFakeIdentity:(FakeSystemIdentity*)fakeIdentity {
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(
                                          fakeIdentity.userEmail)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:
                 ButtonWithAccessibilityLabel(l10n_util::GetNSString(
                     IDS_IOS_MANAGE_YOUR_GOOGLE_ACCOUNT_TITLE))]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];
}

+ (void)tapRemoveAccountFromDeviceWithFakeIdentity:
    (FakeSystemIdentity*)fakeIdentity {
  [self openRemoveAccountConfirmationDialogWithFakeIdentity:fakeIdentity];
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(
                                          l10n_util::GetNSString(
                                              IDS_IOS_REMOVE_ACCOUNT_LABEL))]
      performAction:grey_tap()];
  // Wait until the account is removed.
  [ChromeEarlGreyUI waitForAppToIdle];
}

+ (void)tapPrimarySignInButtonInRecentTabs {
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI
      tapToolsMenuButton:chrome_test_util::RecentTabsDestinationButton()];
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)
      onElementWithMatcher:
          grey_allOf(grey_accessibilityID(
                         kRecentTabsTableViewControllerAccessibilityIdentifier),
                     grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
}

+ (void)tapPrimarySignInButtonInTabSwitcher {
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridOtherDevicesPanelButton()]
      performAction:grey_tap()];
  // The start point needs to avoid the "Done" bar on iPhone, in order to catch
  // the table view and scroll.
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollToContentEdgeWithStartPoint(
                               kGREYContentEdgeBottom, 0.5, 0.5)
      onElementWithMatcher:
          grey_allOf(grey_accessibilityID(
                         kRecentTabsTableViewControllerAccessibilityIdentifier),
                     grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
}

+ (void)verifyWebSigninIsVisible:(BOOL)isVisible {
  NSString* conditionDescription = isVisible
                                       ? @"Web sign-in should be visible"
                                       : @"Web sign-in should not be visible";
  id<GREYMatcher> matcher =
      isVisible ? grey_sufficientlyVisible() : grey_notVisible();
  GREYCondition* condition = [GREYCondition
      conditionWithName:conditionDescription
                  block:^BOOL {
                    NSError* error;
                    [[EarlGrey selectElementWithMatcher:
                                   grey_accessibilityID(
                                       kWebSigninAccessibilityIdentifier)]
                        assertWithMatcher:matcher
                                    error:&error];
                    return error == nil;
                  }];
  GREYAssertTrue([condition waitWithTimeout:10 pollInterval:0.1],
                 conditionDescription);
}

+ (void)submitSyncPassphrase:(NSString*)passphrase {
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kSyncEncryptionPassphraseTextFieldAccessibilityIdentifier)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kSyncEncryptionPassphraseTextFieldAccessibilityIdentifier)]
      performAction:grey_typeText(passphrase)];

  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_kindOfClassName(@"_UIButtonBarButton"),
                                   ButtonWithAccessibilityLabel(
                                       l10n_util::GetNSString(
                                           IDS_IOS_SYNC_DECRYPT_BUTTON)),
                                   nil)] performAction:grey_tap()];
}
#pragma mark - Private

+ (void)signOutWithButton:(id<GREYMatcher>)buttonMatcher
      confirmationLabelID:(int)confirmationLabelID {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  [ChromeEarlGreyUI tapAccountsMenuButton:buttonMatcher];
  id<GREYMatcher> confirmationButtonMatcher = [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:confirmationLabelID];
  [[EarlGrey selectElementWithMatcher:grey_allOf(confirmationButtonMatcher,
                                                 grey_not(buttonMatcher), nil)]
      performAction:grey_tap()];

  // Wait until the user is signed out. Use a longer timeout for cases where
  // sign out also triggers a clear browsing data.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:SettingsDoneButton()
                                  timeout:base::test::ios::
                                              kWaitForClearBrowsingDataTimeout];

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
  [SigninEarlGrey verifySignedOut];
}

@end
