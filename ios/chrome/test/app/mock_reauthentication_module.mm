// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/app/mock_reauthentication_module.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation MockReauthenticationModule

@synthesize localizedReasonForAuthentication =
    _localizedReasonForAuthentication;
@synthesize expectedResult = _expectedResult;
@synthesize canAttempt = _canAttempt;

- (void)setExpectedResult:(ReauthenticationResult)expectedResult {
  _canAttempt = YES;
  _expectedResult = expectedResult;
}

- (BOOL)canAttemptReauth {
  return _canAttempt;
}

- (void)attemptReauthWithLocalizedReason:(NSString*)localizedReason
                    canReusePreviousAuth:(BOOL)canReusePreviousAuth
                                 handler:
                                     (void (^)(ReauthenticationResult success))
                                         showCopyPasswordsHandler {
  self.localizedReasonForAuthentication = localizedReason;
  showCopyPasswordsHandler(_expectedResult);
}

@end
