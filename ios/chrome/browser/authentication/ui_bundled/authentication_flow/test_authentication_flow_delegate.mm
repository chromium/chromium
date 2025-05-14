// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/test_authentication_flow_delegate.h"

#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/notreached.h"
#import "ios/chrome/app/change_profile_continuation.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_test_util.h"
#import "ios/chrome/browser/authentication/ui_bundled/change_profile_continuation_provider.h"
#import "ios/chrome/browser/authentication/ui_bundled/continuation.h"

// Callbacks for the Authentication Flow. At most one callback must be called.
@implementation TestAuthenticationFlowDelegate {
  // Callback after a sign-in succeeded or failed in the same profile. It is set
  // until a callback is called.
  signin_ui::SigninCompletionCallback _signinCompletion;
  // Callback after a sign-in in a different profile. It should not use any
  // object owned, even indirectly, by a ChromeCoordinator as they all will
  // stopped during the profile change.
  ChangeProfileContinuationProvider _changeProfileContinuationProvider;
  // Whether one of the callback was called`:` was called.
  BOOL _callbackCalled;
}

- (instancetype)initWithSigninCompletionCallback:
                    (signin_ui::SigninCompletionCallback)signinCompletion
               changeProfileContinuationProvider:
                   (const ChangeProfileContinuationProvider&)
                       changeProfileContinuationProvider {
  if ((self = [super init])) {
    CHECK(signinCompletion);
    _changeProfileContinuationProvider =
        std::move(changeProfileContinuationProvider);
    _signinCompletion = signinCompletion;
  }
  return self;
}

- (instancetype)initWithSigninCompletionCallback:
    (signin_ui::SigninCompletionCallback)signinCompletion {
  return
      [self initWithSigninCompletionCallback:signinCompletion
           changeProfileContinuationProvider:DoNothingContinuationProvider()];
}

#pragma mark - AuthenticationFlowDelegate

- (void)authenticationFlowDidSignInInSameProfileWithResult:
    (SigninCoordinatorResult)result {
  CHECK(_signinCompletion);
  CHECK(!_callbackCalled);
  _callbackCalled = YES;
  signin_ui::SigninCompletionCallback signinCompletion = _signinCompletion;
  _signinCompletion = nil;
  signinCompletion(result);
}

- (ChangeProfileContinuation)authenticationFlowWillChangeProfile {
  CHECK(!_callbackCalled);
  _callbackCalled = YES;
  CHECK(_changeProfileContinuationProvider);
  return _changeProfileContinuationProvider.Run();
}

@end
