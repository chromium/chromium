// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/model/ios_chrome_aim_eligibility_service.h"

#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/pref_service.h"
#import "components/variations/service/variations_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

IOSChromeAimEligibilityService::IOSChromeAimEligibilityService(
    PrefService* pref_service,
    TemplateURLService* template_url_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    Configuration configuration)
    : AimEligibilityService(*pref_service,
                            template_url_service,
                            url_loader_factory,
                            identity_manager,
                            GetLocaleImpl(),
                            std::move(configuration)) {}

IOSChromeAimEligibilityService::~IOSChromeAimEligibilityService() = default;

std::string IOSChromeAimEligibilityService::GetLocaleImpl() const {
  std::string locale;
  if (experimental_flags::ShouldIgnoreDeviceLocaleConditions()) {
    locale = "en-US";
  } else {
    NSString* localeIdentifier = [NSLocale currentLocale].localeIdentifier;
    if (!localeIdentifier) {
      // Locale might be nil on simulator
      localeIdentifier = @"en-US";
    }

    locale = base::SysNSStringToUTF8(localeIdentifier);
  }
  base::ReplaceChars(locale, "_", "-", &locale);
  return locale;
}

variations::VariationsService*
IOSChromeAimEligibilityService::GetVariationsService() const {
  return GetApplicationContext()
             ? GetApplicationContext()->GetVariationsService()
             : nullptr;
}
