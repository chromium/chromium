// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SETTINGS_HOST_CONTENT_SETTINGS_MAP_FACTORY_H_
#define IOS_CHROME_BROWSER_CONTENT_SETTINGS_HOST_CONTENT_SETTINGS_MAP_FACTORY_H_

#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "components/keyed_service/ios/refcounted_browser_state_keyed_service_factory.h"

class ChromeBrowserState;
class HostContentSettingsMap;

namespace ios {
// Singleton that owns all HostContentSettingsMaps and associates them with
// ChromeBrowserState.
class HostContentSettingsMapFactory
    : public RefcountedBrowserStateKeyedServiceFactory {
 public:
  static HostContentSettingsMap* GetForBrowserState(
      ChromeBrowserState* browser_state);
  static HostContentSettingsMapFactory* GetInstance();

  HostContentSettingsMapFactory(const HostContentSettingsMapFactory&) = delete;
  HostContentSettingsMapFactory& operator=(
      const HostContentSettingsMapFactory&) = delete;

 private:
  friend class base::NoDestructor<HostContentSettingsMapFactory>;

  HostContentSettingsMapFactory();
  ~HostContentSettingsMapFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  scoped_refptr<RefcountedKeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_CONTENT_SETTINGS_HOST_CONTENT_SETTINGS_MAP_FACTORY_H_
