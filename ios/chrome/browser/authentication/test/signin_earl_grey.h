// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_TEST_SIGNIN_EARL_GREY_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_TEST_SIGNIN_EARL_GREY_H_

#import <Foundation/Foundation.h>

#import "base/containers/flat_set.h"
#import "components/policy/core/browser/signin/profile_separation_policies.h"
#import "components/sync/base/user_selectable_type.h"
#import "google_apis/gaia/core_account_id.h"
#import "ios/testing/earl_grey/base_eg_test_helper_impl.h"

@class ExpectedSigninHistograms;

class GaiaId;
@protocol GREYMatcher;
@class FakeSystemIdentity;

namespace signin_metrics {
enum class AccessPoint : int;
}  // namespace signin_metrics

class GURL;

#define SigninEarlGrey \
  [SigninEarlGreyImpl invokedFromFile:@"" __FILE__ lineNumber:__LINE__]

// Methods used for the EarlGrey tests.
// TODO(crbug.com/41465348): Consider moving these into ChromeEarlGrey.
@interface SigninEarlGreyImpl : BaseEGTestHelperImpl

// Calls -[SigninEarlGreyImpl addFakeIdentity:withUnknownCapabilities:NO].
- (void)addFakeIdentity:(FakeSystemIdentity*)fakeIdentity;

// Adds `fakeIdentity` to the fake identity service with capabilities set or
// unset. Does nothing if the identity is already added.
- (void)addFakeIdentity:(FakeSystemIdentity*)fakeIdentity
    withUnknownCapabilities:(BOOL)usingUnknownCapabilities;

// Adds `fakeIdentity` and set the capabilities before firing the list changed
// notification.
- (void)addFakeIdentity:(FakeSystemIdentity*)fakeIdentity
       withCapabilities:(NSDictionary<NSString*, NSNumber*>*)capabilities;

// Calls -[SigninEarlGreyImpl
// addFakeIdentityForSSOAuthAddAccountFlow:withUnknownCapabilities:NO].
- (void)addFakeIdentityForSSOAuthAddAccountFlow:
    (FakeSystemIdentity*)fakeIdentity;

// Adds `fakeIdentity` to the fake system identity interaction manager with
// capabilities set or unset. This is used to simulate adding the `fakeIdentity`
// through the fake SSO Auth flow done by
// `FakeSystemIdentityInteractionManager`. Use
// `[SigninEarlGreyUI addFakeAccountInFakeAddAccountMenu:fakeIdentity];` to
// trigger the add account flow.
- (void)addFakeIdentityForSSOAuthAddAccountFlow:
            (FakeSystemIdentity*)fakeIdentity
                        withUnknownCapabilities:(BOOL)usingUnknownCapabilities;

// Removes `fakeIdentity` from the fake identity service asynchronously to
// simulate identity removal from the device.
- (void)forgetFakeIdentity:(FakeSystemIdentity*)fakeIdentity;

// Returns YES if the identity was added to the fake identity service.
- (BOOL)isIdentityAdded:(FakeSystemIdentity*)fakeIdentity;

// Simulates a persistent authentication error for an account.
- (void)setPersistentAuthErrorForAccount:(const CoreAccountId&)accountId;

// Returns the gaia ID of the signed-in account.
// If there is no signed-in account returns an empty string.
- (GaiaId)primaryAccountGaiaID;

// Returns the gaia IDs of all accounts in the current profile.
- (const base::flat_set<GaiaId>)accountsInProfileGaiaIDs;

// Checks that no identity is signed in.
- (BOOL)isSignedOut;

// Signs the user out of the primary account. Induces a GREYAssert if the
// app fails to sign out.
- (void)signOut;

// Signs in with the fake identity and, if `waitForSync` is true, waits for the
// Sync machinery to become active.
// Adds the fake-identity to the identity manager if necessary.
// Only intended for tests requiring sign-in but not covering the sign-in UI
// behavior to speed up and simplify those tests.
// Will bypass the usual verifications before signin and other
// entry-point-implemented behavior (e.g. history & tabs sync will be disabled,
// no check for management status, sign-in related metrics will not be sent).
- (void)signinWithFakeIdentity:(FakeSystemIdentity*)identity
    waitForSyncTransportActive:(BOOL)waitForSync;

// Same as `signinWithFakeIdentity:identity waitForSyncTransportActive:YES`.
- (void)signinWithFakeIdentity:(FakeSystemIdentity*)identity;

// Signs in with the fake identity and, if `waitForSync` is true, waits for the
// Sync machinery to become active.
// Adds the fake-identity to the identity manager if necessary.
// If separate profiles for managed accounts are enabled, converts the personal
// profile into a managed one.
// Only intended for tests requiring sign-in but not covering the sign-in UI
// behavior to speed up and simplify those tests.
// Will bypass the usual verifications before signin and other
// entry-point-implemented behavior (e.g. history & tabs sync will be disabled,
// no check for management status, sign-in related metrics will not be sent).
- (void)signinWithFakeManagedIdentityInPersonalProfile:
            (FakeSystemIdentity*)identity
                            waitForSyncTransportActive:(BOOL)waitForSync;

// Same as `signinWithFakeManagedIdentityInPersonalProfile:identity
// waitForSyncTransportActive:YES`.
- (void)signinWithFakeManagedIdentityInPersonalProfile:
    (FakeSystemIdentity*)identity;

// Triggers the web sign-in consistency dialog. This is done by calling
// directly the current SceneController.
// `url` that triggered the web sign-in/consistency dialog.
- (void)triggerConsistencyPromoSigninDialogWithURL:(GURL)url;

// Triggers the reauth dialog. This is done by sending ShowSigninCommand to
// SceneController, without any UI interaction to open the dialog.
// TODO(crbug.com/40916763): To be consistent, this method should be renamed to
// `triggerSigninAndSyncReauthWithFakeIdentity:`.
- (void)triggerReauthDialogWithFakeIdentity:(FakeSystemIdentity*)identity;

// Induces a GREYAssert if `fakeIdentity` is not signed in to the active
// profile.
- (void)verifySignedInWithFakeIdentity:(FakeSystemIdentity*)fakeIdentity;

// Induces a GREYAssert if the user is not signed in with `expectedEmail`.
- (void)verifyPrimaryAccountWithEmail:(NSString*)expectedEmail;

// Induces a GREYAssert if an identity is signed in.
- (void)verifySignedOut;

- (void)setSelectedType:(syncer::UserSelectableType)type enabled:(BOOL)enabled;

// Returns if the data type is enabled for the sync service.
- (BOOL)isSelectedTypeEnabled:(syncer::UserSelectableType)type;

// Checks that fore each histogram listed above as properties, it’s emitted the
// number of time indicated in the property for `accessPoint`.
- (void)assertExpectedSigninHistograms:(ExpectedSigninHistograms*)expecteds;

// Set/clear a global flag to return fake default responses for all profile
// separation policy fetch requests (unless a specific response is set for the
// next request, see `setPolicyResponseForNextProfileSeparationPolicyRequest:`).
// If a test sets this (typically in `setUpForTestCase`), it must also unset it
// again (in `tearDown`).
- (void)setUseFakeResponsesForProfileSeparationPolicyRequests;
- (void)clearUseFakeResponsesForProfileSeparationPolicyRequests;

// Stores a policy that will be returned for the next fetch profile separation
// policy request.
- (void)setPolicyResponseForNextProfileSeparationPolicyRequest:
    (policy::ProfileSeparationDataMigrationSettings)
        profileSeparationDataMigrationSettings;

// Closes the managed account sign-in confirmation dialog when necessary, if
// `fakeIdentity` is a managed account. That dialog may be shown on signing in
// if User Policy is enabled.
- (void)closeManagedAccountSignInDialogIfAny:(FakeSystemIdentity*)fakeIdentity;

// Returns whether the feature to put each managed account into its own separate
// profile is enabled. This depends on the `kSeparateProfilesForManagedAccounts`
// feature flag, plus some additional conditions which can't be directly checked
// in the test app.
- (BOOL)areSeparateProfilesForManagedAccountsEnabled;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_TEST_SIGNIN_EARL_GREY_H_
