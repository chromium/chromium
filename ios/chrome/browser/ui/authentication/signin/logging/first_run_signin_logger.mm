// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/logging/first_run_signin_logger.h"

#import "base/metrics/histogram_functions.h"
#import "ios/chrome/browser/first_run/model/first_run_metrics.h"

@interface FirstRunSigninLogger ()

// Presenter for showing sync-related UI.
@property(nonatomic, assign) BOOL hasRecordedSigninStarted;

@end

@implementation FirstRunSigninLogger

#pragma mark - Public

- (void)logSigninStarted {
  if (!self.hasRecordedSigninStarted) {
    self.hasRecordedSigninStarted = YES;
    signin_metrics::LogSigninAccessPointStarted(self.accessPoint,
                                                self.promoAction);
    signin_metrics::RecordSigninUserActionForAccessPoint(self.accessPoint);
    base::UmaHistogramEnumeration(first_run::kFirstRunStageHistogram,
                                  first_run::kWelcomeAndSigninScreenStart);
  }
}

@end
