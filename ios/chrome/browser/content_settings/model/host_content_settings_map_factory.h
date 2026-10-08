// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SETTINGS_MODEL_HOST_CONTENT_SETTINGS_MAP_FACTORY_H_
#define IOS_CHROME_BROWSER_CONTENT_SETTINGS_MODEL_HOST_CONTENT_SETTINGS_MAP_FACTORY_H_

#import "base/memory/scoped_refptr.h"
#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "ios/chrome/browser/shared/model/profile/typed_refcounted_profile_keyed_service_factory_ios.h"

namespace ios {

// Singleton that owns all HostContentSettingsMaps and associates them with
// profiles.
class HostContentSettingsMapFactory
    : public TypedRefcountedProfileKeyedServiceFactoryIOS<
          HostContentSettingsMapFactory,
          HostContentSettingsMap> {
 public:
  HostContentSettingsMapFactory(PassKey key);

 private:
  // RefcountedProfileKeyedServiceFactoryIOS implementation.
  bool ServiceIsRequiredForContextInitialization() const override;
  scoped_refptr<RefcountedKeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_CONTENT_SETTINGS_MODEL_HOST_CONTENT_SETTINGS_MAP_FACTORY_H_
