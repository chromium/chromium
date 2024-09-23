// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/commerce/model/push_notification/push_notification_feature.h"

#import "base/metrics/field_trial_params.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/commerce/core/commerce_feature_list.h"
#import "components/commerce/core/shopping_service.h"
#import "components/variations/service/variations_service_utils.h"
#import "ios/chrome/browser/commerce/model/shopping_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

bool IsPriceTrackingEnabled(ProfileIOS* profile) {
  if (!IsPriceNotificationsEnabled()) {
    return false;
  }

  DCHECK(profile);
  // May be null during testing or if profile is off-the-record.
  commerce::ShoppingService* service =
      commerce::ShoppingServiceFactory::GetForBrowserState(profile);

  return service && service->IsShoppingListEligible();
}

bool IsPriceNotificationsEnabled() {
  std::string country = base::ToLowerASCII(variations::GetCurrentCountryCode(
      GetApplicationContext()->GetVariationsService()));
  std::string current_locale = GetApplicationContext()->GetApplicationLocale();

  // commerce::IsEnabledForCountryAndLocale expectes format with "-", not "_"
  // (as observed locally). E.g. "en-US", not "en_US".
  // https://developer.apple.com/library/archive/documentation/MacOSX/Conceptual/BPInternational/LanguageandLocaleIDs/LanguageandLocaleIDs.html
  base::ReplaceChars(current_locale, "_", "-", &current_locale);
  return commerce::IsEnabledForCountryAndLocale(
      commerce::kCommercePriceTrackingRegionLaunched, country, current_locale);
}
