// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_EARL_GREY_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_EARL_GREY_H_

#import <Foundation/Foundation.h>

#import "ios/testing/earl_grey/base_eg_test_helper_impl.h"

@protocol GREYMatcher;
@class FakeSystemIdentity;

namespace signin {
enum class ConsentLevel;
}

#define SigninEarlGrey \
  [SigninEarlGreyImpl invokedFromFile:@"" __FILE__ lineNumber:__LINE__]

// Methods used for the EarlGrey tests.
// TODO(crbug.com/974833): Consider moving these into ChromeEarlGrey.
@interface SigninEarlGreyImpl : BaseEGTestHelperImpl

// Adds `fakeIdentity` to the fake identity service.
// Does nothing if the identity is already added.
- (void)addFakeIdentity:(FakeSystemIdentity*)fakeIdentity;

// Adds `fakeIdentity` to the fake system identity interaction manager. This
// is used to simulate adding the `fakeIdentity` through the fake SSO Auth flow
// done by `FakeSystemIdentityInteractionManager`. See
// `kFakeAuthAddAccountButtonIdentifier` to trigger the add account flow.
- (void)addFakeIdentityForSSOAuthAddAccountFlow:
    (FakeSystemIdentity*)fakeIdentity;

// Maps capability to the `fakeIdentity`. Check fails if the
// `fakeIdentity` has not been added to the fake identity service.
- (void)setIsSubjectToParentalControls:(BOOL)value
                           forIdentity:(FakeSystemIdentity*)fakeIdentity;
- (void)setCanHaveEmailAddressDisplayed:(BOOL)value
                            forIdentity:(FakeSystemIdentity*)fakeIdentity;
- (void)setCanShowHistorySyncOptInsWithoutMinorModeRestrictions:(BOOL)value
                                                    forIdentity:
                                                        (FakeSystemIdentity*)
                                                            fakeIdentity;

// Removes `fakeIdentity` from the fake identity service asynchronously to
// simulate identity removal from the device.
- (void)forgetFakeIdentity:(FakeSystemIdentity*)fakeIdentity;

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
// `[SigninEarlGreyAppInterfaceUI signinWithFakeIdentity:identity]`. The
// UI function enable sync too.
// TODO(crbug.com/40067025): Remove this last remark when sync is disabled.
- (void)signinWithFakeIdentity:(FakeSystemIdentity*)identity;

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

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_EARL_GREY_H_
