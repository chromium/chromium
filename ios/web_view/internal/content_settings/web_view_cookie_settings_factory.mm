// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/internal/content_settings/web_view_cookie_settings_factory.h"

#include "base/no_destructor.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/web_view/internal/content_settings/web_view_host_content_settings_map_factory.h"
#include "ios/web_view/internal/web_view_browser_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

// static
scoped_refptr<content_settings::CookieSettings>
WebViewCookieSettingsFactory::GetForBrowserState(
    ios_web_view::WebViewBrowserState* browser_state) {
  return static_cast<content_settings::CookieSettings*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true).get());
}

// static
WebViewCookieSettingsFactory* WebViewCookieSettingsFactory::GetInstance() {
  static base::NoDestructor<WebViewCookieSettingsFactory> instance;
  return instance.get();
}

WebViewCookieSettingsFactory::WebViewCookieSettingsFactory()
    : RefcountedBrowserStateKeyedServiceFactory(
          "CookieSettings",
          BrowserStateDependencyManager::GetInstance()) {}

WebViewCookieSettingsFactory::~WebViewCookieSettingsFactory() {}

void WebViewCookieSettingsFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  content_settings::CookieSettings::RegisterProfilePrefs(registry);
}

web::BrowserState* WebViewCookieSettingsFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return context;
}

scoped_refptr<RefcountedKeyedService>
WebViewCookieSettingsFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  WebViewBrowserState* browser_state =
      WebViewBrowserState::FromBrowserState(context);
  return base::MakeRefCounted<content_settings::CookieSettings>(
      WebViewHostContentSettingsMapFactory::GetForBrowserState(browser_state),
      browser_state->GetPrefs(), browser_state->IsOffTheRecord());
}

}  // namespace ios_web_view
