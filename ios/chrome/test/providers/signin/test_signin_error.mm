// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/notreached.h"
#import "ios/public/provider/chrome/browser/signin/signin_error_api.h"

namespace ios {
namespace provider {
namespace {

// Domain for test signin error API.
NSString* const kTestSigninErrorDomain = @"test_signin_error_domain";

// Code for test signin error API.
enum TestSigninErrorCode {
  kUserCancelled,
  kMissingIdentity,
};

}  // anonymous namespace

NSError* CreateUserCancelledSigninError() {
  return [NSError errorWithDomain:kTestSigninErrorDomain
                             code:TestSigninErrorCode::kUserCancelled
                         userInfo:nil];
}

NSError* CreateMissingIdentitySigninError() {
  return [NSError errorWithDomain:kTestSigninErrorDomain
                             code:TestSigninErrorCode::kMissingIdentity
                         userInfo:nil];
}

SigninErrorCategory GetSigninErrorCategory(NSError* error) {
  if (![error.domain isEqualToString:kTestSigninErrorDomain]) {
    return SigninErrorCategory::kUnknownError;
  }

  switch (static_cast<TestSigninErrorCode>(error.code)) {
    case TestSigninErrorCode::kUserCancelled:
      return SigninErrorCategory::kUserCancellationError;

    case TestSigninErrorCode::kMissingIdentity:
      return SigninErrorCategory::kNetworkError;
  }

  NOTREACHED_IN_MIGRATION();
  return SigninErrorCategory::kUnknownError;
}

}  // namespace provider
}  // namespace ios
