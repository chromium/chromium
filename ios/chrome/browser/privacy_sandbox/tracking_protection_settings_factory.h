// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PRIVACY_SANDBOX_TRACKING_PROTECTION_SETTINGS_FACTORY_H_
#define IOS_CHROME_BROWSER_PRIVACY_SANDBOX_TRACKING_PROTECTION_SETTINGS_FACTORY_H_

#include "base/no_destructor.h"
#include "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

namespace privacy_sandbox {
class TrackingProtectionSettings;
}  // namespace privacy_sandbox

class ProfileIOS;

// Factory for TrackingProtectionSettings.
class TrackingProtectionSettingsFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static TrackingProtectionSettingsFactory* GetInstance();
  static privacy_sandbox::TrackingProtectionSettings* GetForProfile(
      ProfileIOS* profile);

 private:
  friend class base::NoDestructor<TrackingProtectionSettingsFactory>;

  TrackingProtectionSettingsFactory();
  ~TrackingProtectionSettingsFactory() override;

  // ProfileKeyedServiceFactoryIOS:
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_PRIVACY_SANDBOX_TRACKING_PROTECTION_SETTINGS_FACTORY_H_
