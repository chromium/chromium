// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/price_insights/model/price_insights_feature.h"

#import "base/metrics/field_trial_params.h"
#import "components/commerce/core/commerce_feature_list.h"
#import "components/commerce/core/shopping_service.h"
#import "ios/chrome/browser/commerce/model/shopping_service_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"

bool IsPriceInsightsEnabled(ChromeBrowserState* browser_state) {
  if (!base::FeatureList::IsEnabled(commerce::kPriceInsightsIos)) {
    return false;
  }

  DCHECK(browser_state);
  commerce::ShoppingService* service =
      commerce::ShoppingServiceFactory::GetForBrowserState(browser_state);

  if (!service) {
    return false;
  }

  return service->IsPriceInsightsEligible() ||
         service->IsCommercePriceTrackingEnabled();
}
