// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"

#import <LocalAuthentication/LocalAuthentication.h>

#import <optional>

#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "base/time/clock.h"
#import "base/time/default_clock.h"
#import "base/time/time.h"

constexpr char kPasscodeArticleURL[] = "https://support.apple.com/HT204060";

namespace {

// Duration of authentication validity.
constexpr base::TimeDelta kIntervalForValidAuth = base::Minutes(1);

}  // namespace

@implementation ReauthenticationModule {
  // The clock used.
  raw_ptr<base::Clock> _clock;

  // Block that creates a new `LAContext` object everytime one is required,
  // meant to make testing with a mock object possible.
  LAContext* (^_createLAContext)(void);

  // Last succcessful authentication time.
  std::optional<base::Time> _lastSuccessfulReauthTime;
}

- (instancetype)init {
  return [self initWithClock:base::DefaultClock::GetInstance()];
}

- (instancetype)initWithClock:(base::Clock*)clock {
  CHECK(clock);
  self = [super init];
  if (self) {
    _clock = clock;
    _createLAContext = ^{
      return [[LAContext alloc] init];
    };
  }
  return self;
}

#pragma mark - ReauthenticationProtocol

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
                                 handler:(ReauthenticationResultBlock)handler {
  if (canReusePreviousAuth && _lastSuccessfulReauthTime) {
    if ((_clock->Now() - *_lastSuccessfulReauthTime) < kIntervalForValidAuth) {
      // Previous successfull authentication is still valid, reuse it.
      handler(ReauthenticationResult::kSkipped);
      return;
    }
  }

  LAContext* context = _createLAContext();

  // This code is shared by the extension and thus cannot use base::PostTask
  // as there won't be any installed base::SequencedTaskRunner. Instead use
  // dispatch_async(...) to post to the main queue.
  __weak __typeof(self) weakSelf = self;
  auto replyBlock = ^(BOOL success, NSError* error) {
    dispatch_async(dispatch_get_main_queue(), ^{
      [weakSelf reauthCompletedWithSuccess:success handler:handler];
    });
  };

  [context evaluatePolicy:LAPolicyDeviceOwnerAuthentication
          localizedReason:localizedReason
                    reply:replyBlock];
}

- (void)clearAuthValidity {
  _lastSuccessfulReauthTime = std::nullopt;
}

#pragma mark - Private

- (void)reauthCompletedWithSuccess:(BOOL)success
                           handler:(ReauthenticationResultBlock)handler {
  ReauthenticationResult result = ReauthenticationResult::kFailure;
  if (success) {
    _lastSuccessfulReauthTime = _clock->Now();
    result = ReauthenticationResult::kSuccess;
  } else {
    _lastSuccessfulReauthTime = std::nullopt;
    result = ReauthenticationResult::kFailure;
  }

  handler(result);
}

#pragma mark - ForTesting

- (void)setCreateLAContext:(LAContext* (^)(void))createLAContext {
  _createLAContext = createLAContext;
}

- (void)setLastSuccessfulReauthTime:(base::Time)time {
  _lastSuccessfulReauthTime = time;
}

@end
