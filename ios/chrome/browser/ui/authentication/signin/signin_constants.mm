// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kUserSigninAttemptedNotification = @"kUserSigninAttempted";
NSString* const kSkipSigninAccessibilityIdentifier =
    @"kSkipSigninAccessibilityIdentifier";
NSString* const kAddAccountAccessibilityIdentifier =
    @"kAddAccountAccessibilityIdentifier";
NSString* const kConfirmationAccessibilityIdentifier =
    @"kConfirmationAccessibilityIdentifier";
NSString* const kMoreAccessibilityIdentifier = @"kMoreAccessibilityIdentifier";

@implementation SigninCompletionInfo

+ (instancetype)signinCompletionInfoWithIdentity:(ChromeIdentity*)identity {
  return [[SigninCompletionInfo alloc]
            initWithIdentity:identity
      signinCompletionAction:SigninCompletionActionNone];
}

- (instancetype)initWithIdentity:(ChromeIdentity*)identity
          signinCompletionAction:
              (SigninCompletionAction)signinCompletionAction {
  self = [super init];
  if (self) {
    _identity = identity;
    _signinCompletionAction = signinCompletionAction;
  }
  return self;
}

- (void)setCompletionURL:(GURL)completionURL {
  if (_completionURL == completionURL)
    return;
  DCHECK(completionURL.is_valid());
  DCHECK(_signinCompletionAction == SigninCompletionActionOpenCompletionURL);
  _completionURL = completionURL;
}

@end
