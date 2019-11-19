// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/firebase_utils.h"

#import <Foundation/Foundation.h>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"
#import "ios/chrome/app/firebase_buildflags.h"
#include "ios/chrome/browser/application_context.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/distribution/app_distribution_provider.h"
#if BUILDFLAG(FIREBASE_ENABLED)
#import "ios/third_party/firebase/Analytics/FirebaseCore.framework/Headers/FIRApp.h"
#import "ios/third_party/firebase/Analytics/FirebaseCore.framework/Headers/FIRConfiguration.h"
#endif  // BUILDFLAG(FIREBASE_ENABLED)

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const char kFirebaseConfiguredHistogramName[] =
    "FirstRun.IOSFirebaseConfigured";

const int kConversionAttributionWindowInDays = 90;

void InitializeFirebase(bool is_first_run) {
#if BUILDFLAG(FIREBASE_ENABLED)
  PrefService* prefs = GetApplicationContext()->GetLocalState();
  const int64_t install_date = prefs->GetInt64(metrics::prefs::kInstallDate);
  base::TimeDelta installed_delta =
      base::TimeDelta::FromSeconds(base::Time::Now().ToTimeT() - install_date);
  // Initialize Firebase SDK only if there is a possibility that user
  // installed Chrome as a result of some marketing campaigns.
  // 1. If user installed Chrome prior to the first version of Chrome that
  //    supports Firebase SDK, this is a legacy user who would not be
  //    attributable to any marketing event. Firebase SDK should not be
  //    initialized in this case.
  // 2. Installation Attribution is the association of a Chrome installation
  //    to some marketing event. This attribution is valid only if it happens
  //    within a reasonable timeframe. If installation date is older than
  //    the acceptable Attribution Window, there is no need to initialize
  //    Firebase SDK since the first_open event would not be considered for
  //    attribution to this installation to a marketing event.
  FirebaseConfiguredState enabled_state;
  auto* provider =
      ios::GetChromeBrowserProvider()->GetAppDistributionProvider();
  if (provider->IsPreFirebaseLegacyUser(install_date)) {
    enabled_state = FirebaseConfiguredState::kDisabledLegacyInstallation;
  } else if (installed_delta.InDaysFloored() >=
             kConversionAttributionWindowInDays) {
    enabled_state = FirebaseConfiguredState::kDisabledConversionWindow;
  } else {
    [[FIRConfiguration sharedInstance] setLoggerLevel:FIRLoggerLevelMin];
    [FIRApp configure];
    enabled_state = is_first_run ? FirebaseConfiguredState::kEnabledFirstRun
                                 : FirebaseConfiguredState::kEnabledNotFirstRun;
  }
  UMA_HISTOGRAM_ENUMERATION(kFirebaseConfiguredHistogramName, enabled_state);
#else
  // FIRApp class should not exist if Firebase is not enabled.
  DCHECK_EQ(nil, NSClassFromString(@"FIRApp"));
  UMA_HISTOGRAM_ENUMERATION(kFirebaseConfiguredHistogramName,
                            FirebaseConfiguredState::kDisabled);
#endif  // BUILDFLAG(FIREBASE_ENABLED)
}
