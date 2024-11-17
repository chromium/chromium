// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"

#import "base/check.h"

@implementation ShowSigninCommand

- (instancetype)initWithOperation:(AuthenticationOperation)operation
                         identity:(id<SystemIdentity>)identity
                      accessPoint:(signin_metrics::AccessPoint)accessPoint
                      promoAction:(signin_metrics::PromoAction)promoAction
                       completion:
                           (ShowSigninCommandCompletionCallback)completion {
  if ((self = [super init])) {
    // Only `InstantSignin` can be opened with an identity selected.
    DCHECK(operation == AuthenticationOperation::kInstantSignin || !identity);
    _operation = operation;
    _identity = identity;
    _accessPoint = accessPoint;
    _promoAction = promoAction;
    _completion = [completion copy];
  }
  return self;
}

- (instancetype)initWithOperation:(AuthenticationOperation)operation
                      accessPoint:(signin_metrics::AccessPoint)accessPoint
                      promoAction:(signin_metrics::PromoAction)promoAction {
  return [self initWithOperation:operation
                        identity:nil
                     accessPoint:accessPoint
                     promoAction:promoAction
                      completion:nil];
}

- (instancetype)initWithOperation:(AuthenticationOperation)operation
                      accessPoint:(signin_metrics::AccessPoint)accessPoint {
  return [self initWithOperation:operation
                        identity:nil
                     accessPoint:accessPoint
                     promoAction:signin_metrics::PromoAction::
                                     PROMO_ACTION_NO_SIGNIN_PROMO
                      completion:nil];
}

@end
