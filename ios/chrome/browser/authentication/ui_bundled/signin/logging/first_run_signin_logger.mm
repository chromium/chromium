// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin/logging/first_run_signin_logger.h"

#import "base/metrics/histogram_functions.h"
#import "ios/chrome/browser/first_run/model/first_run_metrics.h"

@interface FirstRunSigninLogger ()

// Presenter for showing sync-related UI.
@property(nonatomic, assign) BOOL hasRecordedSigninStarted;

@end

@implementation FirstRunSigninLogger

#pragma mark - SigninLogger

- (void)logSigninStarted {
  if (!self.hasRecordedSigninStarted) {
    signin_metrics::LogSignInStarted(self.accessPoint);
    signin_metrics::LogSigninAccessPointStarted(self.accessPoint,
                                                self.promoAction);
    signin_metrics::RecordSigninUserActionForAccessPoint(self.accessPoint);
    base::UmaHistogramEnumeration(first_run::kFirstRunStageHistogram,
                                  first_run::kWelcomeAndSigninScreenStart);
  }
  self.hasRecordedSigninStarted = YES;
}

@end
