// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/google_services/test/google_services_settings_app_interface.h"

#import "components/metrics/metrics_reporting_choice_service.h"

@implementation GoogleServicesSettingsAppInterface

+ (BOOL)shouldUseMetricsConsentRestructure {
  return metrics::MetricsReportingChoiceService::
      ShouldUseMetricsConsentRestructure();
}

@end
