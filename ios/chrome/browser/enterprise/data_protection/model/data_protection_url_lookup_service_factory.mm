// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_protection/model/data_protection_url_lookup_service_factory.h"

#import "components/enterprise/data_protection/data_protection_url_lookup_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
enterprise_data_protection::DataProtectionUrlLookupService*
DataProtectionUrlLookupServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<
          enterprise_data_protection::DataProtectionUrlLookupService>(
          profile, /*create=*/true);
}

// static
DataProtectionUrlLookupServiceFactory*
DataProtectionUrlLookupServiceFactory::GetInstance() {
  static base::NoDestructor<DataProtectionUrlLookupServiceFactory> instance;
  return instance.get();
}

DataProtectionUrlLookupServiceFactory::DataProtectionUrlLookupServiceFactory()
    : ProfileKeyedServiceFactoryIOS("DataProtectionUrlLookupService") {}

DataProtectionUrlLookupServiceFactory::
    ~DataProtectionUrlLookupServiceFactory() = default;

std::unique_ptr<KeyedService>
DataProtectionUrlLookupServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<
      enterprise_data_protection::DataProtectionUrlLookupService>();
}
