// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APP_STORE_BUNDLE_MODEL_APP_STORE_BUNDLE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_APP_STORE_BUNDLE_MODEL_APP_STORE_BUNDLE_SERVICE_FACTORY_H_

#import "ios/chrome/browser/app_store_bundle/model/app_store_bundle_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns all AppStoreBundleService and associates them with
// a ProfileIOS.
class AppStoreBundleServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<AppStoreBundleServiceFactory,
                                                AppStoreBundleService> {
 public:
  explicit AppStoreBundleServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_APP_STORE_BUNDLE_MODEL_APP_STORE_BUNDLE_SERVICE_FACTORY_H_
