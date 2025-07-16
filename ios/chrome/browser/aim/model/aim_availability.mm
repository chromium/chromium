// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/model/aim_availability.h"

#import "components/omnibox/browser/omnibox_prefs.h"
#import "components/prefs/pref_service.h"
#import "components/search/search.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ui/base/device_form_factor.h"

bool IsAIMAvailable(const PrefService* prefs,
                    const TemplateURLService* template_url_service) {
  CHECK(prefs);
  CHECK(template_url_service);

  // Only on phone.
  if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_PHONE) {
    return false;
  }
  // Only when Google is the DSE.
  if (!search::DefaultSearchProviderIsGoogle(template_url_service)) {
    return false;
  }
  // Only when autorized by policy.
  if (!omnibox::IsAimAllowedByPolicy(prefs)) {
    return false;
  }

  if (experimental_flags::ShouldIgnoreDeviceLocaleConditions()) {
    return true;
  }

  BOOL isUSCountry = [NSLocale.currentLocale.countryCode isEqual:@"US"];
  BOOL isEnglishLocale = [NSLocale.currentLocale.languageCode hasPrefix:@"en"];
  BOOL allowedByLocale = isUSCountry && isEnglishLocale;

  return allowedByLocale;
}
