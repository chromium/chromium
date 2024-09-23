// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"

#import "base/test/ios/wait_util.h"
#import "components/signin/public/base/consent_level.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_app_interface.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

using base::test::ios::WaitUntilConditionOrTimeout;

@implementation SigninEarlGreyImpl

- (void)addFakeIdentity:(FakeSystemIdentity*)fakeIdentity {
  [self addFakeIdentity:fakeIdentity withUnknownCapabilities:NO];
}

- (void)addFakeIdentity:(FakeSystemIdentity*)fakeIdentity
    withUnknownCapabilities:(BOOL)usingUnknownCapabilities {
  [SigninEarlGreyAppInterface addFakeIdentity:fakeIdentity
                      withUnknownCapabilities:usingUnknownCapabilities];
}

- (void)addFakeIdentity:(FakeSystemIdentity*)fakeIdentity
       withCapabilities:(NSDictionary<NSString*, NSNumber*>*)capabilities {
  [SigninEarlGreyAppInterface addFakeIdentity:fakeIdentity
                             withCapabilities:capabilities];
}

- (void)addFakeIdentityForSSOAuthAddAccountFlow:
    (FakeSystemIdentity*)fakeIdentity {
  [self addFakeIdentityForSSOAuthAddAccountFlow:fakeIdentity
                        withUnknownCapabilities:NO];
}

- (void)addFakeIdentityForSSOAuthAddAccountFlow:
            (FakeSystemIdentity*)fakeIdentity
                        withUnknownCapabilities:(BOOL)usingUnknownCapabilities {
  [SigninEarlGreyAppInterface
      addFakeIdentityForSSOAuthAddAccountFlow:fakeIdentity
                      withUnknownCapabilities:usingUnknownCapabilities];
}

- (void)forgetFakeIdentity:(FakeSystemIdentity*)fakeIdentity {
  [SigninEarlGreyAppInterface forgetFakeIdentity:fakeIdentity];
}

- (BOOL)isIdentityAdded:(FakeSystemIdentity*)fakeIdentity {
  return [SigninEarlGreyAppInterface isIdentityAdded:fakeIdentity];
}

- (NSString*)primaryAccountGaiaID {
  return [SigninEarlGreyAppInterface primaryAccountGaiaID];
}

- (BOOL)isSignedOut {
  return [SigninEarlGreyAppInterface isSignedOut];
}

- (void)signOut {
  [SigninEarlGreyAppInterface signOut];
  [self verifySignedOut];
}

- (void)signinWithFakeIdentity:(FakeSystemIdentity*)identity {
  [SigninEarlGreyAppInterface signinWithFakeIdentity:identity];
  [self verifySignedInWithFakeIdentity:identity];
}

- (void)signinAndEnableLegacySyncFeature:(FakeSystemIdentity*)identity {
  [SigninEarlGreyAppInterface signinAndEnableLegacySyncFeature:identity];
  [self verifyPrimaryAccountWithEmail:identity.userEmail
                              consent:signin::ConsentLevel::kSync];
}

- (void)signInWithoutHistorySyncWithFakeIdentity:(FakeSystemIdentity*)identity {
  [SigninEarlGreyAppInterface
      signInWithoutHistorySyncWithFakeIdentity:identity];
}

- (void)triggerReauthDialogWithFakeIdentity:(FakeSystemIdentity*)identity {
  [SigninEarlGreyAppInterface triggerReauthDialogWithFakeIdentity:identity];
}

- (void)triggerConsistencyPromoSigninDialogWithURL:(GURL)url {
  [SigninEarlGreyAppInterface
      triggerConsistencyPromoSigninDialogWithURL:net::NSURLWithGURL(url)];
}

- (void)verifySignedInWithFakeIdentity:(FakeSystemIdentity*)fakeIdentity {
  BOOL fakeIdentityIsNonNil = fakeIdentity != nil;
  EG_TEST_HELPER_ASSERT_TRUE(fakeIdentityIsNonNil, @"Need to give an identity");

  // Required to avoid any problem since the following test is not dependant
  // to UI, and the previous action has to be totally finished before going
  // through the assert.
  GREYAssert(WaitUntilConditionOrTimeout(
                 base::test::ios::kWaitForActionTimeout,
                 ^bool {
                   NSString* primaryAccountGaiaID =
                       [SigninEarlGreyAppInterface primaryAccountGaiaID];
                   return primaryAccountGaiaID.length > 0;
                 }),
             @"Sign in did not complete.");
  GREYWaitForAppToIdle(@"App failed to idle");

  NSString* primaryAccountGaiaID =
      [SigninEarlGreyAppInterface primaryAccountGaiaID];

  NSString* errorStr = [NSString
      stringWithFormat:@"Unexpected Gaia ID of the signed in user [expected = "
                       @"\"%@\", actual = \"%@\"]",
                       fakeIdentity.gaiaID, primaryAccountGaiaID];
  EG_TEST_HELPER_ASSERT_TRUE(
      [fakeIdentity.gaiaID isEqualToString:primaryAccountGaiaID], errorStr);
}

- (void)verifyPrimaryAccountWithEmail:(NSString*)expectedEmail
                              consent:(signin::ConsentLevel)consent {
  EG_TEST_HELPER_ASSERT_TRUE(expectedEmail.length, @"Need to give an identity");

  // Required to avoid any problem since the following test is not dependant
  // to UI, and the previous action has to be totally finished before going
  // through the assert.
  GREYAssert(WaitUntilConditionOrTimeout(
                 base::test::ios::kWaitForActionTimeout,
                 ^bool {
                   NSString* primaryAccountEmail = [SigninEarlGreyAppInterface
                       primaryAccountEmailWithConsent:consent];
                   return primaryAccountEmail.length > 0;
                 }),
             @"Sign in did not complete.");
  GREYWaitForAppToIdle(@"App failed to idle");

  NSString* primaryAccountEmail =
      [SigninEarlGreyAppInterface primaryAccountEmailWithConsent:consent];

  NSString* errorStr = [NSString
      stringWithFormat:@"Unexpected email of the signed in user [expected = "
                       @"\"%@\", actual = \"%@\", consent %d]",
                       expectedEmail, primaryAccountEmail,
                       static_cast<int>(consent)];
  EG_TEST_HELPER_ASSERT_TRUE(
      [expectedEmail isEqualToString:primaryAccountEmail], errorStr);
}

- (void)verifySignedOut {
  // Required to avoid any problem since the following test is not dependant to
  // UI, and the previous action has to be totally finished before going through
  // the assert.
  GREYWaitForAppToIdle(@"App failed to idle");

  ConditionBlock condition = ^bool {
    return [SigninEarlGreyAppInterface isSignedOut];
  };
  EG_TEST_HELPER_ASSERT_TRUE(
      WaitUntilConditionOrTimeout(base::test::ios::kWaitForActionTimeout,
                                  condition),
      @"Unexpected signed in user");
}

- (void)verifySyncUIEnabled:(BOOL)enabled {
  NSString* accessibilityString =
      enabled ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
              : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);

  id<GREYMatcher> getSettingsGoogleSyncAndServicesCellMatcher =
      grey_allOf(grey_accessibilityValue(accessibilityString),
                 grey_accessibilityID(kSettingsGoogleSyncAndServicesCellId),
                 grey_sufficientlyVisible(), nil);

  [[EarlGrey
      selectElementWithMatcher:getSettingsGoogleSyncAndServicesCellMatcher]
      assertWithMatcher:grey_notNil()];
}

- (void)verifySyncUIIsHidden {
  id<GREYMatcher> getSettingsGoogleSyncAndServicesCellMatcher = grey_allOf(
      grey_accessibilityValue(l10n_util::GetNSString(IDS_IOS_SETTING_OFF)),
      grey_accessibilityID(kSettingsGoogleSyncAndServicesCellId),
      grey_sufficientlyVisible(), nil);

  [[EarlGrey
      selectElementWithMatcher:getSettingsGoogleSyncAndServicesCellMatcher]
      assertWithMatcher:grey_nil()];
}

- (void)setSelectedType:(syncer::UserSelectableType)type enabled:(BOOL)enabled {
  [SigninEarlGreyAppInterface setSelectedType:type enabled:enabled];
}

- (BOOL)isSelectedTypeEnabled:(syncer::UserSelectableType)type {
  return [SigninEarlGreyAppInterface isSelectedTypeEnabled:type];
}

@end
