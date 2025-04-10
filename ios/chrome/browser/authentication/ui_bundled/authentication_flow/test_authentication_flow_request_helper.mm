// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/test_authentication_flow_request_helper.h"

#import "base/functional/callback.h"
#import "base/notreached.h"
#import "ios/chrome/app/change_profile_continuation.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_test_util.h"
#import "ios/chrome/browser/authentication/ui_bundled/change_profile_continuation_provider.h"
#import "ios/chrome/browser/authentication/ui_bundled/continuation.h"

// Callbacks for the Authentication Flow. At most one callback must be called.
@implementation TestAuthenticationFlowRequest {
  // Callback after a sign-in succeeded or failed in the same profile. It is set
  // until a callback is called.
  signin_ui::SigninCompletionCallback _signinCompletion;
}

- (instancetype)initWithSigninCompletionCallback:
    (signin_ui::SigninCompletionCallback)signinCompletion {
  if ((self = [super init])) {
    CHECK(signinCompletion);
    _signinCompletion = signinCompletion;
  }
  return self;
}

#pragma mark - AuthenticationFlowRequestHelper

- (void)authenticationFlowDidSignInInSameProfileWithResult:
    (SigninCoordinatorResult)result {
  CHECK(_signinCompletion);
  signin_ui::SigninCompletionCallback signinCompletion = _signinCompletion;
  _signinCompletion = nil;
  signinCompletion(result);
}

@end
