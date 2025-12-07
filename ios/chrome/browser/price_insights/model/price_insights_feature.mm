// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/price_insights/model/price_insights_feature.h"

#import "base/metrics/field_trial_params.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/commerce/core/commerce_feature_list.h"
#import "components/commerce/core/feature_utils.h"
#import "components/commerce/core/shopping_service.h"
#import "components/variations/service/variations_service_utils.h"
#import "ios/chrome/browser/commerce/model/shopping_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"

bool IsPriceInsightsRegionEnabled() {
  return commerce::IsRegionLockedFeatureEnabled(
      commerce::kPriceInsights,
      GetCurrentCountryCode(GetApplicationContext()->GetVariationsService()),
      GetApplicationContext()->GetApplicationLocaleStorage()->Get());
}

bool IsPriceInsightsEnabled(ProfileIOS* profile) {
  DCHECK(profile);
  commerce::ShoppingService* service =
      commerce::ShoppingServiceFactory::GetForProfile(profile);

  if (!service) {
    return false;
  }

  return commerce::IsPriceInsightsEligible(service->GetAccountChecker());
}
