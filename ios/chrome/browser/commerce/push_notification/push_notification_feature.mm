// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/commerce/push_notification/push_notification_feature.h"

#import "base/metrics/field_trial_params.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/commerce/core/commerce_feature_list.h"
#import "components/commerce/core/shopping_service.h"
#import "ios/chrome/browser/commerce/shopping_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"

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
  std::string country = base::ToLowerASCII(commerce::GetCurrentCountryCode(
      GetApplicationContext()->GetVariationsService()));
  std::string current_locale = base::ToLowerASCII(
      base::SysNSStringToUTF8([NSLocale currentLocale].localeIdentifier));

  // commerce::IsEnabledForCountryAndLocale expectes format with "-", not "_"
  // (as observed locally). E.g. "en-US", not "en_US".
  // https://developer.apple.com/library/archive/documentation/MacOSX/Conceptual/BPInternational/LanguageandLocaleIDs/LanguageandLocaleIDs.html
  base::ReplaceChars(current_locale, "_", "-", &current_locale);
  return commerce::IsEnabledForCountryAndLocale(
      commerce::kCommercePriceTrackingRegionLaunched, country, current_locale);
}
