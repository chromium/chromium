// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/unified_consent/model/url_keyed_data_collection_consent_helper_factory_ios.h"

#import <utility>

#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/unified_consent/model/url_keyed_data_collection_consent_helper_ios.h"

UrlKeyedDataCollectionConsentHelperFactoryIOS::
    UrlKeyedDataCollectionConsentHelperFactoryIOS(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "UrlKeyedDataCollectionConsentHelperFactoryIOS",
          ServiceCreation::kCreateWithProfile,
          TestingCreation::kNoServiceForTests) {}

std::unique_ptr<KeyedService>
UrlKeyedDataCollectionConsentHelperFactoryIOS::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<UrlKeyedDataCollectionConsentHelperIOS>(
      profile->GetPrefs());
}
