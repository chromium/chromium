// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_SIGNIN_ERROR_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_SIGNIN_ERROR_API_H_

#import <Foundation/Foundation.h>

namespace ios {
namespace provider {

// Signin error category.
//
// Can be used to determine whether the error is caused by the user credentials
// being revoked, deleted or disabled so the appropriate action can be taken.
enum class SigninErrorCategory {
  kUnknownError,  // Not a signin error.

  kNetworkError,                 // Network error, treated as transient.
  kUserCancellationError,        // User cancelation error, treated as no-op.
  kAuthorizationError,           // Authorization error, treated as sign-out.
  kAuthorizationForbiddenError,  // Authorization error, treated as sign-out.
};

// Returns a new signin error corresponding to user cancelled error.
NSError* CreateUserCancelledSigninError();

// Returns a new signin error corresponding to a missing identity error.
NSError* CreateMissingIdentitySigninError();

// Returns the signin error category.
SigninErrorCategory GetSigninErrorCategory(NSError* error);

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_SIGNIN_ERROR_API_H_
