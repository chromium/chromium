// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/app/mock_reauthentication_module.h"

#import "base/check.h"
#import "base/task/sequenced_task_runner.h"
#import "base/test/ios/wait_util.h"

// Type of the block used for delivering mocked reauth results.
typedef void (^ReauthenticationResultHandler)(ReauthenticationResult success);

@interface MockReauthenticationModule ()

// Last handler passed to attemptReauthWithLocalizedReason.
// Used for letting tests control the timing of emitting mock reauth results.
// This allows test to validate states before/after mocked reauth result is
// emitted.
@property(nonatomic) ReauthenticationResultHandler reauthResultHandler;

@end

@implementation MockReauthenticationModule

@synthesize localizedReasonForAuthentication =
    _localizedReasonForAuthentication;
@synthesize expectedResult = _expectedResult;
@synthesize canAttemptWithBiometrics = _canAttemptWithBiometrics;
@synthesize canAttempt = _canAttempt;

- (instancetype)init {
  self = [super init];
  if (self) {
    _shouldSkipReAuth = YES;
  }
  return self;
}

- (void)setExpectedResult:(ReauthenticationResult)expectedResult {
  _canAttemptWithBiometrics = YES;
  _canAttempt = YES;
  _expectedResult = expectedResult;
}

- (BOOL)canAttemptReauthWithBiometrics {
  return _canAttemptWithBiometrics;
}

- (BOOL)canAttemptReauth {
  return _canAttempt;
}

- (void)attemptReauthWithLocalizedReason:(NSString*)localizedReason
                    canReusePreviousAuth:(BOOL)canReusePreviousAuth
                                 handler:(ReauthenticationResultHandler)
                                             reauthResultHandler {
  self.localizedReasonForAuthentication = localizedReason;

  if (self.shouldSkipReAuth) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(reauthResultHandler, _expectedResult));
  } else {
    if (self.reauthResultHandler) {
      // When multiple reauth requests are done without waiting for the result,
      // mimic native behavior and make the oldest request fail.
      self.reauthResultHandler(ReauthenticationResult::kFailure);
    }
    self.reauthResultHandler = reauthResultHandler;
  }
}

- (void)returnMockedReauthenticationResult {
  CHECK(!self.shouldSkipReAuth);

  // Some test cases set the handler after UI events (e.g. animations).
  // Avoid race conditions by waiting until the handler is set.
  __weak __typeof(self) weakSelf = self;
  CHECK(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^{
        return [weakSelf reauthResultHandler] != nullptr;
      }));

  self.reauthResultHandler(_expectedResult);
  self.reauthResultHandler = nil;
}

@end
