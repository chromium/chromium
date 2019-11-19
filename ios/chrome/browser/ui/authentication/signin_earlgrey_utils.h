// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_EARLGREY_UTILS_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_EARLGREY_UTILS_H_

#import <Foundation/Foundation.h>

#include "base/compiler_specific.h"
#import "ios/testing/earl_grey/base_eg_test_helper_impl.h"

@class ChromeIdentity;

#define SigninEarlGreyUtils \
  [SigninEarlGreyUtilsImpl invokedFromFile:@"" __FILE__ lineNumber:__LINE__]

// Methods used for the EarlGrey tests.
// TODO(crbug.com/974833): Consider moving these into ChromeEarlGrey.
@interface SigninEarlGreyUtilsImpl : BaseEGTestHelperImpl

// Returns a fake identity.
- (ChromeIdentity*)fakeIdentity1;

// Returns a second fake identity.
- (ChromeIdentity*)fakeIdentity2;

// Returns a fake managed identity.
- (ChromeIdentity*)fakeManagedIdentity;

// Adds |identity| to the fake identity service.
- (void)addIdentity:(ChromeIdentity*)identity;

// Induces a GREYAssert if |identity| is not signed in to the active profile.
- (void)checkSignedInWithIdentity:(ChromeIdentity*)identity;

// Induces a GREYAssert if an identity is signed in.
- (void)checkSignedOut;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_EARLGREY_UTILS_H_
