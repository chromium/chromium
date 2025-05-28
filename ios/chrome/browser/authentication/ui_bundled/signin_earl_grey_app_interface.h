// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_EARL_GREY_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_EARL_GREY_APP_INTERFACE_H_

#import <UIKit/UIKit.h>

#import "components/policy/core/browser/signin/profile_separation_policies.h"
#import "ios/chrome/browser/signin/model/capabilities_dict.h"
#import "url/gurl.h"

@class FakeSystemIdentity;
@protocol GREYMatcher;

namespace syncer {
enum class UserSelectableType;
}

// SigninEarlGreyAppInterface contains the app-side implementation for
// helpers that primarily work via direct model access. These helpers are
// compiled into the app binary and can be called from either app or test code.
@interface SigninEarlGreyAppInterface : NSObject

// Adds `fakeIdentity` to the fake identity service with capabilities set or
// unset. Does nothing if the fake identity is already added.
+ (void)addFakeIdentity:(FakeSystemIdentity*)fakeIdentity
    withUnknownCapabilities:(BOOL)usingUnknownCapabilities;

// Adds `fakeIdentity` and set the capabilities before firing the list changed
// notification.
+ (void)addFakeIdentity:(FakeSystemIdentity*)fakeIdentity
       withCapabilities:(NSDictionary<NSString*, NSNumber*>*)capabilities;

// Adds `fakeIdentity` to the fake system identity interaction manager, with
// capabilities set or unset. This is used to simulate adding the `fakeIdentity`
// through the fake SSO Auth flow done by
// `FakeSystemIdentityInteractionManager`. See
// `kFakeAuthAddAccountButtonIdentifier` to trigger the add account flow.
+ (void)addFakeIdentityForSSOAuthAddAccountFlow:
            (FakeSystemIdentity*)fakeIdentity
                        withUnknownCapabilities:(BOOL)usingUnknownCapabilities;

// Removes `fakeIdentity` from the fake chrome identity service asynchronously
// to simulate identity removal from the device.
+ (void)forgetFakeIdentity:(FakeSystemIdentity*)fakeIdentity;

// Returns YES if the identity was added to the fake identity service.
+ (BOOL)isIdentityAdded:(FakeSystemIdentity*)fakeIdentity;

// Simulates a persistent authentication error for an account.
+ (void)setPersistentAuthErrorForAccount:(NSString*)accountGaiaId;

// Returns the gaia ID of the primary account.
// If there is no primary account returns an empty string.
+ (NSString*)primaryAccountGaiaID;

// Returns the email of the primary account.
// If there is no signed-in account returns an empty string.
+ (NSString*)primaryAccountEmail;

// Returns the gaia IDs of all accounts in the current profile.
+ (NSSet<NSString*>*)accountsInProfileGaiaIDs;

// Checks that no identity is signed in.
+ (BOOL)isSignedOut;

// Signs out the current user.
+ (void)signOut;

// Signs in with the fake identity.
// Adds the fake-identity to the identity manager if necessary.
// Call `[SigninEarlGrey signinWithFakeIdentity:identity]` instead.
// `fakeIdentity` is added if it was not added yet.
+ (void)signinWithFakeIdentity:(FakeSystemIdentity*)identity;

// Signs in with the fake managed identity.
// Adds the fake-identity to the identity manager if necessary.
// If separate profiles for managed accounts are enabled, converts the personal
// profile into a managed one.
// Call `[SigninEarlGrey
// signinWithFakeManagedIdentityInPersonalProfile:identity]` instead.
// `fakeIdentity` is added if it was not added yet.
+ (void)signinWithFakeManagedIdentityInPersonalProfile:
    (FakeSystemIdentity*)identity;

// Triggers the reauth dialog. This is done by sending ShowSigninCommand to
// SceneController, without any UI interaction to open the dialog.
// TODO(crbug.com/40916763): To be consistent, this method should be renamed to
// `triggerSigninAndSyncReauthWithFakeIdentity:`.
+ (void)triggerReauthDialogWithFakeIdentity:(FakeSystemIdentity*)identity;

// Triggers the web sign-in consistency dialog. This is done by calling
// directly the current SceneController.
// `url` that triggered the web sign-in/consistency dialog.
+ (void)triggerConsistencyPromoSigninDialogWithURL:(NSURL*)url;

// Presents the signed-in accounts view controller if it needs to be presented.
+ (void)presentSignInAccountsViewControllerIfNecessary;

+ (void)setSelectedType:(syncer::UserSelectableType)type enabled:(BOOL)enabled;

// Returns if the data type is enabled for the sync service.
+ (BOOL)isSelectedTypeEnabled:(syncer::UserSelectableType)type;

// Set/clear a global flag to return fake default responses for all profile
// separation policy fetch requests (unless a specific response is set for the
// next request, see `setPolicyResponseForNextProfileSeparationPolicyRequest:`).
// If a test sets this (typically in `setUpForTestCase`), it must also unset it
// again (in `tearDown`).
+ (void)setUseFakeResponsesForProfileSeparationPolicyRequests;
+ (void)clearUseFakeResponsesForProfileSeparationPolicyRequests;

// Stores a policy that will be returned for the next fetch profile separation
// policy request.
+ (void)setPolicyResponseForNextProfileSeparationPolicyRequest:
    (policy::ProfileSeparationDataMigrationSettings)
        profileSeparationDataMigrationSettings;

// Returns whether the feature to put each managed account into its own separate
// profile is enabled. This depends on the `kSeparateProfilesForManagedAccounts`
// feature flag, plus some additional conditions which can't be directly checked
// in the test app.
+ (BOOL)areSeparateProfilesForManagedAccountsEnabled;

// Returns whether the account particle disc on the NTP should open the account
// menu. This depends on the `kSeparateProfilesForManagedAccounts` and
// `kIdentityDiscAccountMenu` feature flags, plus some additional conditions
// which can't be directly checked in the test app.
+ (BOOL)isIdentityDiscAccountMenuEnabled;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_EARL_GREY_APP_INTERFACE_H_
