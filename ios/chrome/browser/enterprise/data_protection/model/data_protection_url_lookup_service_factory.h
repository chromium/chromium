// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_MODEL_DATA_PROTECTION_URL_LOOKUP_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_MODEL_DATA_PROTECTION_URL_LOOKUP_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace enterprise_data_protection {
class DataProtectionUrlLookupService;
}  // namespace enterprise_data_protection

// Singleton factory that creates and manages one instance of
// DataProtectionUrlLookupService per ProfileIOS.
// This service is not created for Incognito profiles as the features
// it supports (e.g., real-time URL checks for screenshot protection)
// are not active in Incognito mode.
class DataProtectionUrlLookupServiceFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  // Returns the DataProtectionUrlLookupService instance for the given
  // `profile`. Returns nullptr if the profile is an Incognito profile.
  // The service is created if it doesn't exist yet for the profile.
  static enterprise_data_protection::DataProtectionUrlLookupService*
  GetForProfile(ProfileIOS* profile);

  // Returns the singleton instance of the
  // DataProtectionUrlLookupServiceFactory.
  static DataProtectionUrlLookupServiceFactory* GetInstance();

  // DataProtectionUrlLookupServiceFactory is a singleton and should not be
  // copied.
  DataProtectionUrlLookupServiceFactory(
      const DataProtectionUrlLookupServiceFactory&) = delete;
  DataProtectionUrlLookupServiceFactory& operator=(
      const DataProtectionUrlLookupServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<DataProtectionUrlLookupServiceFactory>;

  DataProtectionUrlLookupServiceFactory();
  ~DataProtectionUrlLookupServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_MODEL_DATA_PROTECTION_URL_LOOKUP_SERVICE_FACTORY_H_
