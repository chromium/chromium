// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"

#import "base/check.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/ios/block_types.h"
#import "ios/chrome/browser/authentication/ui_bundled/change_profile_continuation_provider.h"
#import "ios/chrome/browser/authentication/ui_bundled/continuation.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"

@implementation ShowSigninCommand {
  ChangeProfileContinuationProvider _provider;
}

- (instancetype)initWithOperation:(AuthenticationOperation)operation
                             identity:(id<SystemIdentity>)identity
                          accessPoint:(signin_metrics::AccessPoint)accessPoint
                          promoAction:(signin_metrics::PromoAction)promoAction
                           completion:
                               (SigninCoordinatorCompletionCallback)completion
                 prepareChangeProfile:(ProceduralBlock)prepareChangeProfile
    changeProfileContinuationProvider:
        (const ChangeProfileContinuationProvider&)provider {
  if ((self = [super init])) {
    // Only `InstantSignin` can be opened with an identity selected.
    DCHECK(operation == AuthenticationOperation::kInstantSignin || !identity);
    CHECK(provider);
    _operation = operation;
    _identity = identity;
    _accessPoint = accessPoint;
    _promoAction = promoAction;
    _completion = [completion copy];
    _optionalHistorySync = YES;
    _fullScreenPromo = NO;
    _prepareChangeProfile = prepareChangeProfile;
    _provider = provider;
  }
  return self;
}

- (instancetype)initWithOperation:(AuthenticationOperation)operation
                             identity:(id<SystemIdentity>)identity
                          accessPoint:(signin_metrics::AccessPoint)accessPoint
                          promoAction:(signin_metrics::PromoAction)promoAction
                           completion:
                               (SigninCoordinatorCompletionCallback)completion
    changeProfileContinuationProvider:
        (const ChangeProfileContinuationProvider&)provider {
  return [self initWithOperation:operation
                               identity:identity
                            accessPoint:accessPoint
                            promoAction:promoAction
                             completion:completion
                   prepareChangeProfile:nil
      changeProfileContinuationProvider:provider];
}

- (instancetype)initWithOperation:(AuthenticationOperation)operation
                         identity:(id<SystemIdentity>)identity
                      accessPoint:(signin_metrics::AccessPoint)accessPoint
                      promoAction:(signin_metrics::PromoAction)promoAction
                       completion:
                           (SigninCoordinatorCompletionCallback)completion {
  return [self initWithOperation:operation
                               identity:identity
                            accessPoint:accessPoint
                            promoAction:promoAction
                             completion:completion
      changeProfileContinuationProvider:DoNothingContinuationProvider()];
}

- (instancetype)initWithOperation:(AuthenticationOperation)operation
                          accessPoint:(signin_metrics::AccessPoint)accessPoint
                          promoAction:(signin_metrics::PromoAction)promoAction
    changeProfileContinuationProvider:
        (const ChangeProfileContinuationProvider&)provider {
  return [self initWithOperation:operation
                               identity:nil
                            accessPoint:accessPoint
                            promoAction:promoAction
                             completion:nil
      changeProfileContinuationProvider:provider];
}

- (instancetype)initWithOperation:(AuthenticationOperation)operation
                      accessPoint:(signin_metrics::AccessPoint)accessPoint
                      promoAction:(signin_metrics::PromoAction)promoAction {
  return [self initWithOperation:operation
                               identity:nil
                            accessPoint:accessPoint
                            promoAction:promoAction
                             completion:nil
      changeProfileContinuationProvider:DoNothingContinuationProvider()];
}

- (instancetype)initWithOperation:(AuthenticationOperation)operation
                          accessPoint:(signin_metrics::AccessPoint)accessPoint
    changeProfileContinuationProvider:
        (const ChangeProfileContinuationProvider&)provider {
  return [self initWithOperation:operation
                               identity:nil
                            accessPoint:accessPoint
                            promoAction:signin_metrics::PromoAction::
                                            PROMO_ACTION_NO_SIGNIN_PROMO
                             completion:nil
      changeProfileContinuationProvider:provider];
}

- (instancetype)initWithOperation:(AuthenticationOperation)operation
                      accessPoint:(signin_metrics::AccessPoint)accessPoint {
  return [self initWithOperation:operation
                               identity:nil
                            accessPoint:accessPoint
                            promoAction:signin_metrics::PromoAction::
                                            PROMO_ACTION_NO_SIGNIN_PROMO
                             completion:nil
      changeProfileContinuationProvider:DoNothingContinuationProvider()];
}

- (const ChangeProfileContinuationProvider&)changeProfileContinuationProvider {
  return _provider;
}

@end
