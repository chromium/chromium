// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/first_run_app_interface.h"

#import "components/metrics/metrics_pref_names.h"
#import "components/metrics/metrics_reporting_default_state.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/app/main_controller.h"
#import "ios/chrome/app/main_controller_private.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/main/scene_controller.h"
#import "ios/chrome/browser/ui/main/scene_controller_testing.h"
#import "ios/chrome/test/app/chrome_test_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation FirstRunAppInterface

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

+ (BOOL)isSyncFirstSetupComplete {
  return SyncSetupServiceFactory::GetForBrowserState(
             chrome_test_util::GetOriginalBrowserState())
      ->IsFirstSetupComplete();
}

@end
