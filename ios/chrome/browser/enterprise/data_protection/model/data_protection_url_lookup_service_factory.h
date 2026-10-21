// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_MODEL_DATA_PROTECTION_URL_LOOKUP_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_MODEL_DATA_PROTECTION_URL_LOOKUP_SERVICE_FACTORY_H_

#import "components/enterprise/data_protection/data_protection_url_lookup_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton factory that creates and manages one instance of
// DataProtectionUrlLookupService per ProfileIOS.
// This service is not created for Incognito profiles as the features
// it supports (e.g., real-time URL checks for screenshot protection)
// are not active in Incognito mode.
class DataProtectionUrlLookupServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          DataProtectionUrlLookupServiceFactory,
          enterprise_data_protection::DataProtectionUrlLookupService> {
 public:
  explicit DataProtectionUrlLookupServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_MODEL_DATA_PROTECTION_URL_LOOKUP_SERVICE_FACTORY_H_
