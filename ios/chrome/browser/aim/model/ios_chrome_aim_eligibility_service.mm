// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/model/ios_chrome_aim_eligibility_service.h"

#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

IOSChromeAimEligibilityService::IOSChromeAimEligibilityService(
    PrefService* pref_service,
    TemplateURLService* template_url_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    bool is_off_the_record)
    : AimEligibilityService(*pref_service,
                            template_url_service,
                            url_loader_factory,
                            identity_manager,
                            is_off_the_record) {}

IOSChromeAimEligibilityService::~IOSChromeAimEligibilityService() = default;

std::string IOSChromeAimEligibilityService::GetCountryCode() const {
  if (experimental_flags::ShouldIgnoreDeviceLocaleConditions()) {
    return "us";
  }
  return base::SysNSStringToUTF8(
      [[NSLocale currentLocale].countryCode lowercaseString]);
}

std::string IOSChromeAimEligibilityService::GetLocale() const {
  if (experimental_flags::ShouldIgnoreDeviceLocaleConditions()) {
    return "en_US";
  }
  return base::SysNSStringToUTF8([NSLocale currentLocale].localeIdentifier);
}
