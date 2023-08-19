// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/signin_completion_info.h"

@implementation SigninCompletionInfo

+ (instancetype)signinCompletionInfoWithIdentity:(id<SystemIdentity>)identity {
  return [[SigninCompletionInfo alloc]
            initWithIdentity:identity
      signinCompletionAction:SigninCompletionActionNone];
}

- (instancetype)initWithIdentity:(id<SystemIdentity>)identity
          signinCompletionAction:
              (SigninCompletionAction)signinCompletionAction {
  self = [super init];
  if (self) {
    _identity = identity;
    _signinCompletionAction = signinCompletionAction;
  }
  return self;
}

@end
