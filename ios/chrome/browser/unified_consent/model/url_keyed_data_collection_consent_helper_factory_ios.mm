// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/unified_consent/model/url_keyed_data_collection_consent_helper_factory_ios.h"

#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/unified_consent/model/url_keyed_data_collection_consent_helper_ios.h"

UrlKeyedDataCollectionConsentHelperFactoryIOS::
    UrlKeyedDataCollectionConsentHelperFactoryIOS()
    : ProfileKeyedServiceFactoryIOS(
          "UrlKeyedDataCollectionConsentHelperFactoryIOS",
          ServiceCreation::kCreateWithProfile,
          TestingCreation::kNoServiceForTests) {}

UrlKeyedDataCollectionConsentHelperFactoryIOS::
    ~UrlKeyedDataCollectionConsentHelperFactoryIOS() = default;

// static
UrlKeyedDataCollectionConsentHelperIOS*
UrlKeyedDataCollectionConsentHelperFactoryIOS::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<UrlKeyedDataCollectionConsentHelperIOS>(
          profile, /*create=*/true);
}

// static
UrlKeyedDataCollectionConsentHelperIOS*
UrlKeyedDataCollectionConsentHelperFactoryIOS::GetForProfileIfExists(
    ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<UrlKeyedDataCollectionConsentHelperIOS>(
          profile, /*create=*/false);
}

// static
UrlKeyedDataCollectionConsentHelperFactoryIOS*
UrlKeyedDataCollectionConsentHelperFactoryIOS::GetInstance() {
  static base::NoDestructor<UrlKeyedDataCollectionConsentHelperFactoryIOS>
      instance;
  return instance.get();
}

std::unique_ptr<KeyedService>
UrlKeyedDataCollectionConsentHelperFactoryIOS::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<UrlKeyedDataCollectionConsentHelperIOS>(
      profile->GetPrefs());
}
