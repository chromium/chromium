// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/user_signin/logging/first_run_signin_logger.h"

#import "base/metrics/user_metrics.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_utils.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using signin_metrics::AccessPoint;
using signin_metrics::LogSigninAccessPointStarted;
using signin_metrics::PromoAction;
using signin_metrics::RecordSigninUserActionForAccessPoint;

@interface FirstRunSigninLogger ()

// Presenter for showing sync-related UI.
@property(nonatomic, assign) BOOL hasRecordedSigninStarted;

@end

@implementation FirstRunSigninLogger

#pragma mark - Public

- (void)logSigninStarted {
  if (!self.hasRecordedSigninStarted) {
    self.hasRecordedSigninStarted = YES;
    LogSigninAccessPointStarted(self.accessPoint, self.promoAction);
    RecordSigninUserActionForAccessPoint(self.accessPoint, self.promoAction);
  }
  signin::RecordVersionSeen();
}

@end
