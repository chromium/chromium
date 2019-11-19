// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/content_settings/cookie_settings_factory.h"

#include "base/no_destructor.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/content_settings/host_content_settings_map_factory.h"

namespace ios {

// static
scoped_refptr<content_settings::CookieSettings>
CookieSettingsFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<content_settings::CookieSettings*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true).get());
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

CookieSettingsFactory::~CookieSettingsFactory() {
}

void CookieSettingsFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  content_settings::CookieSettings::RegisterProfilePrefs(registry);
}

web::BrowserState* CookieSettingsFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  // The incognito browser state has its own content settings map. Therefore, it
  // should get its own CookieSettings.
  return GetBrowserStateOwnInstanceInIncognito(context);
}

scoped_refptr<RefcountedKeyedService>
CookieSettingsFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);
  return base::MakeRefCounted<content_settings::CookieSettings>(
      ios::HostContentSettingsMapFactory::GetForBrowserState(browser_state),
      browser_state->GetPrefs(), browser_state->IsOffTheRecord());
}

}  // namespace ios
