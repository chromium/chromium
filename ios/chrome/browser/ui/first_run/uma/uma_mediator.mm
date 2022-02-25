// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/uma/uma_mediator.h"

#import "components/metrics/metrics_pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/application_context.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation UMAMediator

#pragma mark - UMATableViewControllerModelDelegate

- (BOOL)reportingMetricEnabled {
  return GetApplicationContext()->GetLocalState()->GetBoolean(
      metrics::prefs::kMetricsReportingEnabled);
}

- (void)setReportingMetricEnabled:(BOOL)isEnabled {
  GetApplicationContext()->GetLocalState()->SetBoolean(
      metrics::prefs::kMetricsReportingEnabled, isEnabled);
}

@end
