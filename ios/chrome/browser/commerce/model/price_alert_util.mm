// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/commerce/model/price_alert_util.h"

#import "base/metrics/field_trial_params.h"
#import "components/commerce/core/commerce_feature_list.h"
#import "components/prefs/pref_service.h"
#import "components/unified_consent/url_keyed_data_collection_consent_helper.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"

bool IsPriceAlertsEligible(web::BrowserState* browser_state) {
  if (browser_state->IsOffTheRecord()) {
    return false;
  }
  // Price drop annotations are only enabled for en_US.
  NSLocale* current_locale = [NSLocale currentLocale];
  if (![@"en_US" isEqualToString:current_locale.localeIdentifier]) {
    return false;
  }
  ChromeBrowserState* chrome_browser_state =
      ChromeBrowserState::FromBrowserState(browser_state);
  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForBrowserState(chrome_browser_state);
  DCHECK(authentication_service);
  if (!authentication_service->HasPrimaryIdentity(
          signin::ConsentLevel::kSignin)) {
    return false;
  }
  PrefService* pref_service = chrome_browser_state->GetPrefs();
  if (!unified_consent::UrlKeyedDataCollectionConsentHelper::
           NewAnonymizedDataCollectionConsentHelper(pref_service)
               ->IsEnabled() ||
      !pref_service->GetBoolean(prefs::kTrackPricesOnTabsEnabled)) {
    return false;
  }
  return true;
}
