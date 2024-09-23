// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/notreached.h"
#import "ios/public/provider/chrome/browser/signin/signin_error_api.h"

namespace ios {
namespace provider {
namespace {

// Domain for chromium signin error API.
NSString* const kChromiumSigninErrorDomain = @"chromium_signin_error_domain";

// Code for chromium signin error API.
enum ChromiumSigninErrorCode {
  kUserCancelled,
  kMissingIdentity,
};

}  // anonymous namespace

NSError* CreateUserCancelledSigninError() {
  return [NSError errorWithDomain:kChromiumSigninErrorDomain
                             code:ChromiumSigninErrorCode::kUserCancelled
                         userInfo:nil];
}

NSError* CreateMissingIdentitySigninError() {
  return [NSError errorWithDomain:kChromiumSigninErrorDomain
                             code:ChromiumSigninErrorCode::kMissingIdentity
                         userInfo:nil];
}

SigninErrorCategory GetSigninErrorCategory(NSError* error) {
  if (![error.domain isEqualToString:kChromiumSigninErrorDomain]) {
    return SigninErrorCategory::kUnknownError;
  }

  switch (static_cast<ChromiumSigninErrorCode>(error.code)) {
    case ChromiumSigninErrorCode::kUserCancelled:
      return SigninErrorCategory::kUserCancellationError;

    case ChromiumSigninErrorCode::kMissingIdentity:
      return SigninErrorCategory::kNetworkError;
  }

  NOTREACHED_IN_MIGRATION();
  return SigninErrorCategory::kUnknownError;
}

}  // namespace provider
}  // namespace ios
