// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/commerce/push_notification/push_notification_feature.h"

#import "base/metrics/field_trial_params.h"
#import "components/commerce/core/commerce_feature_list.h"
#import "components/commerce/core/shopping_service.h"
#import "ios/chrome/browser/commerce/shopping_service_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"

namespace {
const char kPriceTrackingNotifications[] = "enable_price_notification";
}  // namespace

bool IsPriceTrackingEnabled(ChromeBrowserState* browser_state) {
  if (!IsPriceNotificationsEnabled()) {
    return false;
  }

  DCHECK(browser_state);
  // May be null during testing or if browser state is off-the-record.
  commerce::ShoppingService* service =
      commerce::ShoppingServiceFactory::GetForBrowserState(browser_state);

  return service && service->IsShoppingListEligible();
}

bool IsPriceNotificationsEnabled() {
  return base::GetFieldTrialParamByFeatureAsBool(
      commerce::kCommercePriceTracking, kPriceTrackingNotifications,
      /** default_value */ false);
}
