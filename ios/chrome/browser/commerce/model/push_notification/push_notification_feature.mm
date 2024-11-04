// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/commerce/model/push_notification/push_notification_feature.h"

#import "base/check_is_test.h"
#import "components/commerce/core/shopping_service.h"
#import "ios/chrome/browser/commerce/model/shopping_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

bool IsPriceTrackingEnabled(ProfileIOS* profile) {
  DCHECK(profile);

  // May be null during testing or if profile is off-the-record.
  commerce::ShoppingService* service =
      commerce::ShoppingServiceFactory::GetForProfile(profile);

  return service && service->IsShoppingListEligible();
}
