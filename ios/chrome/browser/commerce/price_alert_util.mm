// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/commerce/price_alert_util.h"

#include "components/prefs/pref_service.h"
#include "components/unified_consent/pref_names.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kPriceTrackingWithOptimizationGuideParam[] =
    "price_tracking_with_optimization_guide";
}  // namespace

bool IsPriceAlertsEligible(web::BrowserState* browser_state) {
  if (browser_state->IsOffTheRecord()) {
    return false;
  }
  ChromeBrowserState* chrome_browser_state =
      ChromeBrowserState::FromBrowserState(browser_state);
  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForBrowserState(chrome_browser_state);
  if (!authentication_service || !authentication_service->HasPrimaryIdentity(
                                     signin::ConsentLevel::kSignin)) {
    return false;
  }
  const PrefService& prefs = *chrome_browser_state->GetPrefs();
  if (!prefs.GetBoolean(
          unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled)) {
    return false;
  }
  return true;
}

bool IsPriceAlertsEnabled() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kCommercePriceTracking, kPriceTrackingWithOptimizationGuideParam,
      /** default_value */ false);
}
