// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_MODEL_DATA_TYPE_STORE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SYNC_MODEL_DATA_TYPE_STORE_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace syncer {
class DataTypeStoreService;
}  // namespace syncer

// Singleton that owns all DataTypeStoreService and associates them with
// ProfileIOS.
class DataTypeStoreServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static syncer::DataTypeStoreService* GetForProfile(ProfileIOS* profile);
  static DataTypeStoreServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<DataTypeStoreServiceFactory>;

  DataTypeStoreServiceFactory();
  ~DataTypeStoreServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_SYNC_MODEL_DATA_TYPE_STORE_SERVICE_FACTORY_H_
