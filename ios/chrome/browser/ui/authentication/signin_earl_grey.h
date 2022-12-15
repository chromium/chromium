// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_EARL_GREY_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_EARL_GREY_H_

#import <Foundation/Foundation.h>

#import "base/compiler_specific.h"
#import "ios/chrome/browser/signin/capabilities_dict.h"
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
- (void)addFakeIdentity:(FakeSystemIdentity*)fakeIdentity;

// Maps `capabilities` to the `fakeIdentity`. Check fails if the
// `fakeIdentity` has not been added to the fake identity service.
- (void)setCapabilities:(ios::CapabilitiesDict*)capabilities
            forIdentity:fakeIdentity;

// Removes `fakeIdentity` from the fake identity service asynchronously to
// simulate identity removal from the device.
- (void)forgetFakeIdentity:(FakeSystemIdentity*)fakeIdentity;

// Signs the user out of the primary account. Induces a GREYAssert if the
// app fails to sign out.
- (void)signOut;

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
