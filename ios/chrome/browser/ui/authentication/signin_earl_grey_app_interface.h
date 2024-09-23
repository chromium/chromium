// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_EARL_GREY_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_EARL_GREY_APP_INTERFACE_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/signin/model/capabilities_dict.h"
#import "url/gurl.h"

@class FakeSystemIdentity;
@protocol GREYMatcher;

namespace signin {
enum class ConsentLevel;
}

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

// Returns the gaia ID of the signed-in account.
// If there is no signed-in account returns an empty string.
+ (NSString*)primaryAccountGaiaID;

// Returns the email of the primary account base on `consentLevel`.
// If there is no signed-in account returns an empty string.
+ (NSString*)primaryAccountEmailWithConsent:(signin::ConsentLevel)consentLevel;

// Checks that no identity is signed in.
+ (BOOL)isSignedOut;

// Signs out the current user.
+ (void)signOut;

// Signs in with the fake identity and access point Settings.
// Adds the fake-identity to the identity manager if necessary.
// Call `[SigninEarlGrey signinWithFakeIdentity:identity]` instead.
// `fakeIdentity` is added if it was not added yet.
+ (void)signinWithFakeIdentity:(FakeSystemIdentity*)identity;

// TODO(crbug.com/40066949): Remove all tests invoking this when deleting the
// MaybeMigrateSyncingUserToSignedIn() call on //ios (not right after launching
// kMigrateSyncingUserToSignedIn).
// `fakeIdentity` is added if it was not added yet.
+ (void)signinAndEnableLegacySyncFeature:(FakeSystemIdentity*)identity;

// Signs in with `identity` without history sync consent.
// `fakeIdentity` is added if it was not added yet.
+ (void)signInWithoutHistorySyncWithFakeIdentity:(FakeSystemIdentity*)identity;

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

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_EARL_GREY_APP_INTERFACE_H_
