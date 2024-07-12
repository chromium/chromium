// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_EARL_GREY_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_EARL_GREY_H_

#import <Foundation/Foundation.h>

#import "components/sync/base/user_selectable_type.h"
#import "ios/testing/earl_grey/base_eg_test_helper_impl.h"

@protocol GREYMatcher;
@class FakeSystemIdentity;

namespace signin {
enum class ConsentLevel;
}

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
// `FakeSystemIdentityInteractionManager`. See
// `kFakeAuthAddAccountButtonIdentifier` to trigger the add account flow.
- (void)addFakeIdentityForSSOAuthAddAccountFlow:
            (FakeSystemIdentity*)fakeIdentity
                        withUnknownCapabilities:(BOOL)usingUnknownCapabilities;

// Removes `fakeIdentity` from the fake identity service asynchronously to
// simulate identity removal from the device.
- (void)forgetFakeIdentity:(FakeSystemIdentity*)fakeIdentity;

// Returns YES if the identity was added to the fake identity service.
- (BOOL)isIdentityAdded:(FakeSystemIdentity*)fakeIdentity;

// Returns the gaia ID of the signed-in account.
// If there is no signed-in account returns an empty string.
- (NSString*)primaryAccountGaiaID;

// Checks that no identity is signed in.
- (BOOL)isSignedOut;

// Signs the user out of the primary account. Induces a GREYAssert if the
// app fails to sign out.
- (void)signOut;

// Signs in with the fake identity and access point Settings.
// Adds the fake-identity to the identity manager if necessary.
// Only intended for tests requiring sign-in but not covering the sign-in UI
// behavior to speed up and simplify those tests.
// Will bypass the usual verifications before signin and other
// entry-point-implemented behavior (e.g. history & tabs sync will be disabled,
// no check for management status, sign-in related
// metrics will not be sent)
// Note that, when sync-the-feature is enabled, this function differs from
// `[SigninEarlGreyUI signinWithFakeIdentity:identity]`. The
// UI function enable sync too.
// TODO(crbug.com/40067025): Remove this last remark when sync is disabled.
- (void)signinWithFakeIdentity:(FakeSystemIdentity*)identity;

// TODO(crbug.com/40066949): Remove all tests invoking this when deleting the
// MaybeMigrateSyncingUserToSignedIn() call on //ios (not right after launching
// kMigrateSyncingUserToSignedIn).
- (void)signinAndEnableLegacySyncFeature:(FakeSystemIdentity*)identity;

// Signs in with `identity` without history sync consent.
- (void)signInWithoutHistorySyncWithFakeIdentity:(FakeSystemIdentity*)identity;

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
- (void)verifyPrimaryAccountWithEmail:(NSString*)expectedEmail
                              consent:(signin::ConsentLevel)consent;

// Induces a GREYAssert if an identity is signed in.
- (void)verifySignedOut;

// Induces a GREYAssert if the Sync state does not match `enabled`.
- (void)verifySyncUIEnabled:(BOOL)enabled;

// Induces a GREYAssert if the Sync cell is not hidden.
- (void)verifySyncUIIsHidden;

- (void)setSelectedType:(syncer::UserSelectableType)type enabled:(BOOL)enabled;

// Returns if the data type is enabled for the sync service.
- (BOOL)isSelectedTypeEnabled:(syncer::UserSelectableType)type;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_EARL_GREY_H_
