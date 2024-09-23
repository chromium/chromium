// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SETTINGS_MODEL_HOST_CONTENT_SETTINGS_MAP_FACTORY_H_
#define IOS_CHROME_BROWSER_CONTENT_SETTINGS_MODEL_HOST_CONTENT_SETTINGS_MAP_FACTORY_H_

#import "base/memory/ref_counted.h"
#import "base/no_destructor.h"
#import "components/keyed_service/ios/refcounted_browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class HostContentSettingsMap;

namespace ios {
// Singleton that owns all HostContentSettingsMaps and associates them with
// profiles.
class HostContentSettingsMapFactory
    : public RefcountedBrowserStateKeyedServiceFactory {
 public:
  // TODO(crbug.com/358301380): remove this method.
  static HostContentSettingsMap* GetForBrowserState(ProfileIOS* profile);

  static HostContentSettingsMap* GetForProfile(ProfileIOS* profile);
  static HostContentSettingsMapFactory* GetInstance();

  HostContentSettingsMapFactory(const HostContentSettingsMapFactory&) = delete;
  HostContentSettingsMapFactory& operator=(
      const HostContentSettingsMapFactory&) = delete;

 private:
  friend class base::NoDestructor<HostContentSettingsMapFactory>;

  HostContentSettingsMapFactory();
  ~HostContentSettingsMapFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  bool ServiceIsRequiredForContextInitialization() const override;
  scoped_refptr<RefcountedKeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_CONTENT_SETTINGS_MODEL_HOST_CONTENT_SETTINGS_MAP_FACTORY_H_
