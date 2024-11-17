// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/content_settings/model/cookie_settings_factory.h"

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#include "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace ios {

// static
scoped_refptr<content_settings::CookieSettings>
CookieSettingsFactory::GetForProfile(ProfileIOS* profile) {
  return static_cast<content_settings::CookieSettings*>(
      GetInstance()->GetServiceForBrowserState(profile, true).get());
}

// static
CookieSettingsFactory* CookieSettingsFactory::GetInstance() {
  static base::NoDestructor<CookieSettingsFactory> instance;
  return instance.get();
}

CookieSettingsFactory::CookieSettingsFactory()
    : RefcountedBrowserStateKeyedServiceFactory(
          "CookieSettings",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ios::HostContentSettingsMapFactory::GetInstance());
}

CookieSettingsFactory::~CookieSettingsFactory() {}

void CookieSettingsFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  content_settings::CookieSettings::RegisterProfilePrefs(registry);
}

web::BrowserState* CookieSettingsFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  // The incognito profile has its own content settings map. Therefore, it
  // should get its own CookieSettings.
  return GetBrowserStateOwnInstanceInIncognito(context);
}

scoped_refptr<RefcountedKeyedService>
CookieSettingsFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  return base::MakeRefCounted<content_settings::CookieSettings>(
      ios::HostContentSettingsMapFactory::GetForProfile(profile),
      profile->GetPrefs(), /*tracking_protection_settings=*/nullptr,
      profile->IsOffTheRecord(),
      content_settings::CookieSettings::NoFedCmSharingPermissionsCallback(),
      /*tpcd_metadata_manager=*/nullptr);
}

}  // namespace ios
