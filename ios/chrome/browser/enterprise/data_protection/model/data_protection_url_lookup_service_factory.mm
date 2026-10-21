// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_protection/model/data_protection_url_lookup_service_factory.h"

#import <utility>

#import "components/enterprise/data_protection/data_protection_url_lookup_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

DataProtectionUrlLookupServiceFactory::DataProtectionUrlLookupServiceFactory(
    PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(std::move(key),
                                         "DataProtectionUrlLookupService") {}

std::unique_ptr<KeyedService>
DataProtectionUrlLookupServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<
      enterprise_data_protection::DataProtectionUrlLookupService>();
}
