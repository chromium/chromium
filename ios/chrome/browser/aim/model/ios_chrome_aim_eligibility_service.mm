// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/model/ios_chrome_aim_eligibility_service.h"

#import "base/functional/bind.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/application_locale_storage/application_locale_storage.h"
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
                            std::move(configuration)) {
  if (GetApplicationContext() &&
      GetApplicationContext()->GetApplicationLocaleStorage()) {
    locale_change_subscription_ =
        GetApplicationContext()
            ->GetApplicationLocaleStorage()
            ->RegisterOnLocaleChangedCallback(base::BindRepeating(
                &IOSChromeAimEligibilityService::OnLocaleChanged,
                weak_factory_.GetWeakPtr()));
  }
}

IOSChromeAimEligibilityService::~IOSChromeAimEligibilityService() = default;

std::string IOSChromeAimEligibilityService::GetLocaleImpl() const {
  if (experimental_flags::ShouldIgnoreDeviceLocaleConditions()) {
    return "en-US";
  }
  std::string locale =
      GetApplicationContext() &&
              GetApplicationContext()->GetApplicationLocaleStorage()
          ? GetApplicationContext()->GetApplicationLocaleStorage()->Get(
                ApplicationLocaleStorage::LocaleFormat::kBCP47)
          : "";
  if (locale.empty()) {
    NSString* locale_identifier = [NSLocale currentLocale].localeIdentifier;
    if (locale_identifier) {
      locale = base::SysNSStringToUTF8(locale_identifier);
      base::ReplaceChars(locale, "_", "-", &locale);
    }
  }
  if (locale.empty()) {
    // Locale might be nil on simulator or uninitialized in test environments.
    locale = "en-US";
  }
  return locale;
}

variations::VariationsService*
IOSChromeAimEligibilityService::GetVariationsService() const {
  return GetApplicationContext()
             ? GetApplicationContext()->GetVariationsService()
             : nullptr;
}

void IOSChromeAimEligibilityService::OnLocaleChanged(
    const std::string& /*new_locale*/) {
  FetchEligibility(RequestSource::kLocaleChange);
}
