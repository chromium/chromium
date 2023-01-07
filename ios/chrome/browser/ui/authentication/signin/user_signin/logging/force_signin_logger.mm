// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/user_signin/logging/force_signin_logger.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ForceSigninLogger ()

// Presenter for showing sync-related UI.
@property(nonatomic, assign) BOOL hasRecordedSigninStarted;

@end

@implementation ForceSigninLogger

#pragma mark - Public

- (instancetype)initWithPromoAction:(signin_metrics::PromoAction)promoAction
              accountManagerService:
                  (ChromeAccountManagerService*)accountManagerService {
  return [super initWithAccessPoint:signin_metrics::AccessPoint::
                                        ACCESS_POINT_FORCED_SIGNIN
                        promoAction:promoAction
              accountManagerService:accountManagerService];
}

- (void)logSigninStarted {
  if (!self.hasRecordedSigninStarted) {
    self.hasRecordedSigninStarted = YES;
    signin_metrics::LogSigninAccessPointStarted(self.accessPoint,
                                                self.promoAction);
  }
}

@end
