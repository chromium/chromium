// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/signin/model/signin_client_factory.h"

#include "base/no_destructor.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/chrome/browser/content_settings/model/cookie_settings_factory.h"
#include "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"
#include "ios/chrome/browser/signin/model/ios_chrome_signin_client.h"

// static
SigninClient* SigninClientFactory::GetForProfile(ProfileIOS* profile) {
  return static_cast<SigninClient*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

// static
SigninClientFactory* SigninClientFactory::GetInstance() {
  static base::NoDestructor<SigninClientFactory> instance;
  return instance.get();
}

SigninClientFactory::SigninClientFactory()
    : BrowserStateKeyedServiceFactory(
          "SigninClient",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ios::CookieSettingsFactory::GetInstance());
  DependsOn(ios::HostContentSettingsMapFactory::GetInstance());
}

SigninClientFactory::~SigninClientFactory() {}

std::unique_ptr<KeyedService> SigninClientFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  return std::make_unique<IOSChromeSigninClient>(
      profile, ios::CookieSettingsFactory::GetForProfile(profile),
      ios::HostContentSettingsMapFactory::GetForProfile(profile));
}
