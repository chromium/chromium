// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_EARLGREY_UTILS_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_EARLGREY_UTILS_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

@class ChromeIdentity;
@protocol GREYMatcher;

// SignInEarlGreyAppInterface contains the app-side implementation for helpers
// that primarily work via direct model access. These helpers are compiled into
// the app binary and can be called from either app or test code.
@interface SignInEarlGreyUtilsAppInterface : NSObject

// Adds |identity| to the fake identity service.
+ (void)addIdentity:(ChromeIdentity*)identity;

// Returns the gaia ID of the signed-in account.
+ (NSString*)primaryAccountGaiaID;

// Checks that no identity is signed in.
+ (BOOL)isSignedOut;

// Returns a matcher for an identity picker cell for |email|.
+ (id<GREYMatcher>)identityCellMatcherForEmail:(NSString*)email;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_EARLGREY_UTILS_APP_INTERFACE_H_
