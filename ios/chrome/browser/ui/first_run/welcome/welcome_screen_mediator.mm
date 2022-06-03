// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/welcome/welcome_screen_mediator.h"

#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_reporting_default_state.h"
#include "components/prefs/pref_service.h"
#import "ios/chrome/browser/application_context.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Default value for metrics reporting state. "YES" corresponding to "opt-out"
// state.
const BOOL kDefaultStatsCheckboxValue = YES;

}  // namespace

@interface WelcomeScreenMediator ()

@end

@implementation WelcomeScreenMediator

#pragma mark - Public

- (instancetype)init {
  self = [super init];
  if (self) {
    [self recordMetricsReportingDefaultState];
  }
  return self;
}

- (BOOL)isCheckboxSelectedByDefault {
  return kDefaultStatsCheckboxValue;
}

- (void)setMetricsReportingEnabled:(BOOL)enabled {
  GetApplicationContext()->GetLocalState()->SetBoolean(
      metrics::prefs::kMetricsReportingEnabled, enabled);
}

#pragma mark - Private

// Records what the default opt-in state for metrics reporting is in the local
// prefs, based on whether the consent checkbox should be selected by default.
- (void)recordMetricsReportingDefaultState {
  // Record metrics reporting as opt-in/opt-out only once.
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    // Don't call RecordMetricsReportingDefaultState twice. This can happen if
    // the app is quit before accepting the TOS, or via experiment settings.
    if (metrics::GetMetricsReportingDefaultState(
            GetApplicationContext()->GetLocalState()) !=
        metrics::EnableMetricsDefault::DEFAULT_UNKNOWN) {
      return;
    }

    metrics::RecordMetricsReportingDefaultState(
        GetApplicationContext()->GetLocalState(),
        kDefaultStatsCheckboxValue ? metrics::EnableMetricsDefault::OPT_OUT
                                   : metrics::EnableMetricsDefault::OPT_IN);
  });
}

@end
