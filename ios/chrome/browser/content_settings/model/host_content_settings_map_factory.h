// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SETTINGS_MODEL_HOST_CONTENT_SETTINGS_MAP_FACTORY_H_
#define IOS_CHROME_BROWSER_CONTENT_SETTINGS_MODEL_HOST_CONTENT_SETTINGS_MAP_FACTORY_H_

#import "base/memory/scoped_refptr.h"
#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/refcounted_profile_keyed_service_factory_ios.h"

class HostContentSettingsMap;

namespace ios {

// Singleton that owns all HostContentSettingsMaps and associates them with
// profiles.
class HostContentSettingsMapFactory
    : public RefcountedProfileKeyedServiceFactoryIOS {
 public:
  static HostContentSettingsMap* GetForProfile(ProfileIOS* profile);
  static HostContentSettingsMapFactory* GetInstance();

 private:
  friend class base::NoDestructor<HostContentSettingsMapFactory>;

  HostContentSettingsMapFactory();
  ~HostContentSettingsMapFactory() override;

  // RefcountedProfileKeyedServiceFactoryIOS implementation.
  bool ServiceIsRequiredForContextInitialization() const override;
  scoped_refptr<RefcountedKeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_CONTENT_SETTINGS_MODEL_HOST_CONTENT_SETTINGS_MAP_FACTORY_H_
