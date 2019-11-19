// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/first_run_app_interface.h"

#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_reporting_default_state.h"
#include "components/prefs/pref_service.h"
#import "ios/chrome/app/main_controller.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#include "ios/chrome/browser/ui/first_run/welcome_to_chrome_view_controller.h"
#import "ios/chrome/test/app/chrome_test_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// TODO(crbug.com/1019942): Develop a better way to show the first run UI that
// doesn't require exposing private API.
@interface MainController (ExposedForTesting)
- (void)showFirstRunUI;
@end

@implementation FirstRunAppInterface

+ (void)showFirstRunUI {
  [chrome_test_util::GetMainController() showFirstRunUI];
}

+ (void)setUMACollectionEnabled:(BOOL)enabled {
  GetApplicationContext()->GetLocalState()->SetBoolean(
      metrics::prefs::kMetricsReportingEnabled, enabled);
}

+ (BOOL)isUMACollectionEnabled {
  return GetApplicationContext()->GetLocalState()->GetBoolean(
      metrics::prefs::kMetricsReportingEnabled);
}

+ (void)resetUMACollectionEnabledByDefault {
  GetApplicationContext()->GetLocalState()->SetInteger(
      metrics::prefs::kMetricsDefaultOptIn,
      metrics::EnableMetricsDefault::DEFAULT_UNKNOWN);
}

+ (BOOL)isUMACollectionEnabledByDefault {
  return [WelcomeToChromeViewController defaultStatsCheckboxValue];
}

+ (BOOL)isSyncFirstSetupComplete {
  return SyncSetupServiceFactory::GetForBrowserState(
             chrome_test_util::GetOriginalBrowserState())
      ->IsFirstSetupComplete();
}

@end
