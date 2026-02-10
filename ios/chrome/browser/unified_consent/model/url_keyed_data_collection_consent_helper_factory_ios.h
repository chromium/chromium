// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UNIFIED_CONSENT_MODEL_URL_KEYED_DATA_COLLECTION_CONSENT_HELPER_FACTORY_IOS_H_
#define IOS_CHROME_BROWSER_UNIFIED_CONSENT_MODEL_URL_KEYED_DATA_COLLECTION_CONSENT_HELPER_FACTORY_IOS_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;
class UrlKeyedDataCollectionConsentHelperIOS;

class UrlKeyedDataCollectionConsentHelperFactoryIOS
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static UrlKeyedDataCollectionConsentHelperIOS* GetForProfile(
      ProfileIOS* profile);

  static UrlKeyedDataCollectionConsentHelperIOS* GetForProfileIfExists(
      ProfileIOS* profile);

  static UrlKeyedDataCollectionConsentHelperFactoryIOS* GetInstance();

 private:
  friend class base::NoDestructor<
      UrlKeyedDataCollectionConsentHelperFactoryIOS>;

  UrlKeyedDataCollectionConsentHelperFactoryIOS();
  ~UrlKeyedDataCollectionConsentHelperFactoryIOS() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_UNIFIED_CONSENT_MODEL_URL_KEYED_DATA_COLLECTION_CONSENT_HELPER_FACTORY_IOS_H_
