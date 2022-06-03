// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/signin/signin_client_factory.h"

#include "base/no_destructor.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/content_settings/cookie_settings_factory.h"
#include "ios/chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "ios/chrome/browser/signin/ios_chrome_signin_client.h"

// static
SigninClient* SigninClientFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<SigninClient*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
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
  ChromeBrowserState* chrome_browser_state =
      ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<IOSChromeSigninClient>(
      chrome_browser_state,
      ios::CookieSettingsFactory::GetForBrowserState(chrome_browser_state),
      ios::HostContentSettingsMapFactory::GetForBrowserState(
          chrome_browser_state));
}
