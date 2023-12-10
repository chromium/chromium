// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"

#import <LocalAuthentication/LocalAuthentication.h>

#import "base/check.h"

constexpr char kPasscodeArticleURL[] = "https://support.apple.com/HT204060";

@interface ReauthenticationModule () <SuccessfulReauthTimeAccessor>

// Date kept to decide if last auth can be reused when
// `lastSuccessfulReauthTime` is `self`.
@property(nonatomic, strong) NSDate* lastSuccessfulReauthTime;

@end

@implementation ReauthenticationModule {
  // Block that creates a new `LAContext` object everytime one is required,
  // meant to make testing with a mock object possible.
  LAContext* (^_createLAContext)(void);

  // Accessor allowing the module to request the update of the time when the
  // successful re-authentication was performed and to get the time of the last
  // successful re-authentication.
  __weak id<SuccessfulReauthTimeAccessor> _successfulReauthTimeAccessor;
}

- (instancetype)init {
  self = [self initWithSuccessfulReauthTimeAccessor:self];
  return self;
}

- (instancetype)initWithSuccessfulReauthTimeAccessor:
    (id<SuccessfulReauthTimeAccessor>)successfulReauthTimeAccessor {
  DCHECK(successfulReauthTimeAccessor);
  self = [super init];
  if (self) {
    _createLAContext = ^{
      return [[LAContext alloc] init];
    };
    _successfulReauthTimeAccessor = successfulReauthTimeAccessor;
  }
  return self;
}

- (BOOL)canAttemptReauthWithBiometrics {
  LAContext* context = _createLAContext();
  // The authentication method is Touch ID or Face ID.
  return
      [context canEvaluatePolicy:LAPolicyDeviceOwnerAuthenticationWithBiometrics
                           error:nil];
}

- (BOOL)canAttemptReauth {
  LAContext* context = _createLAContext();
  // The authentication method is Touch ID, Face ID or passcode.
  return [context canEvaluatePolicy:LAPolicyDeviceOwnerAuthentication
                              error:nil];
}

- (void)attemptReauthWithLocalizedReason:(NSString*)localizedReason
                    canReusePreviousAuth:(BOOL)canReusePreviousAuth
                                 handler:
                                     (void (^)(ReauthenticationResult success))
                                         handler {
  if (canReusePreviousAuth && [self isPreviousAuthValid]) {
    handler(ReauthenticationResult::kSkipped);
    return;
  }

  LAContext* context = _createLAContext();

  __weak ReauthenticationModule* weakSelf = self;
  void (^replyBlock)(BOOL, NSError*) = ^(BOOL success, NSError* error) {
    dispatch_async(dispatch_get_main_queue(), ^{
      ReauthenticationModule* strongSelf = weakSelf;
      if (!strongSelf)
        return;
      if (success) {
        [strongSelf->_successfulReauthTimeAccessor updateSuccessfulReauthTime];
      }
      handler(success ? ReauthenticationResult::kSuccess
                      : ReauthenticationResult::kFailure);
    });
  };

  [context evaluatePolicy:LAPolicyDeviceOwnerAuthentication
          localizedReason:localizedReason
                    reply:replyBlock];
}

- (BOOL)isPreviousAuthValid {
  BOOL previousAuthValid = NO;
  const int kIntervalForValidAuthInSeconds = 60;
  NSDate* lastSuccessfulReauthTime =
      [_successfulReauthTimeAccessor lastSuccessfulReauthTime];
  if (lastSuccessfulReauthTime) {
    NSDate* currentTime = [NSDate date];
    NSTimeInterval timeSincePreviousSuccessfulAuth =
        [currentTime timeIntervalSinceDate:lastSuccessfulReauthTime];
    if (timeSincePreviousSuccessfulAuth < kIntervalForValidAuthInSeconds) {
      previousAuthValid = YES;
    }
  }
  return previousAuthValid;
}

#pragma mark - SuccessfulReauthTimeAccessor

- (void)updateSuccessfulReauthTime {
  self.lastSuccessfulReauthTime = [[NSDate alloc] init];
}

#pragma mark - ForTesting

- (void)setCreateLAContext:(LAContext* (^)(void))createLAContext {
  _createLAContext = createLAContext;
}

@end
