// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SETTINGS_MODEL_COOKIE_SETTINGS_FACTORY_H_
#define IOS_CHROME_BROWSER_CONTENT_SETTINGS_MODEL_COOKIE_SETTINGS_FACTORY_H_

#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "components/keyed_service/ios/refcounted_browser_state_keyed_service_factory.h"

class ChromeBrowserState;

namespace content_settings {
class CookieSettings;
}

namespace ios {
// Singleton that owns all CookieSettings and associates them with
// ChromeBrowserState.
class CookieSettingsFactory : public RefcountedBrowserStateKeyedServiceFactory {
 public:
  static scoped_refptr<content_settings::CookieSettings> GetForBrowserState(
      ChromeBrowserState* browser_state);
  static CookieSettingsFactory* GetInstance();

  CookieSettingsFactory(const CookieSettingsFactory&) = delete;
  CookieSettingsFactory& operator=(const CookieSettingsFactory&) = delete;

 private:
  friend class base::NoDestructor<CookieSettingsFactory>;

  CookieSettingsFactory();
  ~CookieSettingsFactory() override;

  // RefcountedBrowserStateKeyedServiceFactory implementation.
  void RegisterBrowserStatePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
  scoped_refptr<RefcountedKeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_CONTENT_SETTINGS_MODEL_COOKIE_SETTINGS_FACTORY_H_
