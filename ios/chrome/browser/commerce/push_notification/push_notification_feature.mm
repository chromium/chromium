// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/commerce/push_notification/push_notification_feature.h"

#import "base/metrics/field_trial_params.h"
#import "components/commerce/core/commerce_feature_list.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kPriceTrackingNotifications[] = "enable_price_notification";
}  // namespace

// Determine if price drop notifications are enabled and the ShoppingService is
// available.
bool IsPriceNotificationsEnabled() {
  return base::FeatureList::IsEnabled(commerce::kShoppingList) &&
         base::GetFieldTrialParamByFeatureAsBool(
             commerce::kCommercePriceTracking, kPriceTrackingNotifications,
             /** default_value */ false);
}
