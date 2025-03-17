// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin_earl_grey.h"

#import "base/test/ios/wait_util.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/authentication/ui_bundled/expected_signin_histograms.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_earl_grey_app_interface.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_table_view_controller_constants.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
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

- (NSSet<NSString*>*)accountsInProfileGaiaIDs {
  return [SigninEarlGreyAppInterface accountsInProfileGaiaIDs];
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

- (void)signinAndWaitForSyncTransportStateActive:(FakeSystemIdentity*)identity {
  [self signinWithFakeIdentity:identity];
  [ChromeEarlGrey waitForSyncTransportStateActiveWithTimeout:base::Seconds(10)];
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
  // Required to avoid any problem since the following test is not dependant
  // to UI, and the previous action has to be totally finished before going
  // through the assert.
  GREYWaitForAppToIdle(@"App failed to idle");

  ConditionBlock condition = ^bool {
    return [SigninEarlGreyAppInterface isSignedOut];
  };
  EG_TEST_HELPER_ASSERT_TRUE(
      WaitUntilConditionOrTimeout(base::test::ios::kWaitForActionTimeout,
                                  condition),
      @"Unexpected signed in user");
}

- (void)setSelectedType:(syncer::UserSelectableType)type enabled:(BOOL)enabled {
  [SigninEarlGreyAppInterface setSelectedType:type enabled:enabled];
}

- (BOOL)isSelectedTypeEnabled:(syncer::UserSelectableType)type {
  return [SigninEarlGreyAppInterface isSelectedTypeEnabled:type];
}

- (void)assertExpectedSigninHistograms:(ExpectedSigninHistograms*)expecteds {
  std::vector<std::pair<NSString*, int>> array = {
      {@"Signin.SignIn.Offered", expecteds.signinSignInOffered},
      {@"Signin.SignIn.Offered.WithDefault",
       expecteds.signinSignInOfferedWithdefault},
      {@"Signin.SignIn.Offered.NewAccountNoExistingAccount",
       expecteds.signinSignInOfferedNewAccountNoExistingAccount},

      {@"Signin.SigninStartedAccessPoint",
       expecteds.signinSigninStartedAccessPoint},
      {@"Signin.SigninStartedAccessPoint.WithDefault",
       expecteds.signinSigninStartedAccessPointWithDefault},
      {@"Signin.SigninStartedAccessPoint.NotDefault",
       expecteds.signinSigninStartedAccessPointNotDefault},
      {@"Signin.SigninStartedAccessPoint.NewAccountNoExistingAccount",
       expecteds.signinSignStartedAccessPointNewAccountNoExistingAccount},
      {@"Signin.SigninStartedAccessPoint.NewAccountExistingAccount",
       expecteds.signinSignStartedAccessPointNewAccountExistingAccount},

      {@"Signin.SignIn.Completed", expecteds.signinSignInCompleted},
      {@"Signin.SigninCompletedAccessPoint",
       expecteds.signinSigninCompletedAccessPoint},
      {@"Signin.SigninCompletedAccessPoint.WithDefault",
       expecteds.signinSigninCompletedAccessPointWithDefault},
      {@"Signin.SigninCompletedAccessPoint.NotDefault",
       expecteds.signinSigninCompletedAccessPointNotDefault},
      {@"Signin.SigninCompletedAccessPoint.NewAccountNoExistingAccount",
       expecteds.signinSigninCompletedAccessPointNewAccountNoExistingAccount},
      {@"Signin.SigninCompletedAccessPoint.NewAccountExistingAccount",
       expecteds.signinSigninCompletedAccessPointNewAccountExistingAccount},

      {@"Signin.SignIn.Started", expecteds.signinSignInStarted},
      {@"Signin.SyncOptIn.Started", expecteds.signinSyncOptInStarted},
      {@"Signin.SyncOptIn.OpenedSyncSettings",
       expecteds.signinSyncOptInOpenedSyncSettings},
  };
  signin_metrics::AccessPoint accessPoint = expecteds.accessPoint;
  for (const std::pair<NSString*, int>& expected : array) {
    NSString* histogram = expected.first;
    int count = expected.second;
    NSError* error =
        [MetricsAppInterface expectCount:count
                               forBucket:static_cast<int>(accessPoint)
                            forHistogram:histogram];
    chrome_test_util::GREYAssertErrorNil(error);
  }
}

@end
