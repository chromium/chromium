// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_EARL_GREY_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_EARL_GREY_APP_INTERFACE_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/signin/capabilities_dict.h"
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

// Adds `fakeIdentity` to the fake identity service.
+ (void)addFakeIdentity:(FakeSystemIdentity*)fakeIdentity;

// Adds `fakeIdentity` to the fake system identity interaction manager. This
// is used to simulate adding the `fakeIdentity` through the fake SSO Auth flow
// done by `FakeSystemIdentityInteractionManager`. See
// `kFakeAuthAddAccountButtonIdentifier` to trigger the add account flow.
+ (void)addFakeIdentityForSSOAuthAddAccountFlow:
    (FakeSystemIdentity*)fakeIdentity;

// Removes `fakeIdentity` from the fake chrome identity service asynchronously
// to simulate identity removal from the device.
+ (void)forgetFakeIdentity:(FakeSystemIdentity*)fakeIdentity;

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

// Triggers the reauth dialog. This is done by sending ShowSigninCommand to
// SceneController, without any UI interaction to open the dialog.
// TODO(crbug.com/1454101): To be consistent, this method should be renamed to
// `triggerSigninAndSyncReauthWithFakeIdentity:`.
+ (void)triggerReauthDialogWithFakeIdentity:(FakeSystemIdentity*)identity;

// Triggers the web sign-in consistency dialog. This is done by calling
// directly the current SceneController.
// `url` that triggered the web sign-in/consistency dialog.
+ (void)triggerConsistencyPromoSigninDialogWithURL:(NSURL*)url;

// Clears the signed-in accounts preference, used to verify if the signed-in
// accounts view should be presented.
+ (void)clearLastSignedInAccounts;

// Presents the signed-in accounts view controller if it needs to be presented.
+ (void)presentSignInAccountsViewControllerIfNecessary;

// Capability setters for `fakeIdentity`.
// Capabilities can only be set after the identity has been added to storage.
// Must be called after `addFakeIdentity`.
+ (void)setIsSubjectToParentalControls:(BOOL)value
                           forIdentity:(FakeSystemIdentity*)fakeIdentity;
+ (void)setCanHaveEmailAddressDisplayed:(BOOL)value
                            forIdentity:(FakeSystemIdentity*)fakeIdentity;
+ (void)setCanOfferExtendedChromeSyncPromos:(BOOL)value
                                forIdentity:(FakeSystemIdentity*)fakeIdentity;

+ (void)setSelectedType:(syncer::UserSelectableType)type enabled:(BOOL)enabled;

// Returns if the data type is enabled for the sync service.
+ (BOOL)isSelectedTypeEnabled:(syncer::UserSelectableType)type;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_EARL_GREY_APP_INTERFACE_H_
