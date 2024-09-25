// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_MODEL_DATA_TYPE_STORE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SYNC_MODEL_DATA_TYPE_STORE_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

namespace syncer {
class DataTypeStoreService;
}  // namespace syncer

// Singleton that owns all DataTypeStoreService and associates them with
// ProfileIOS.
class DataTypeStoreServiceFactory : public BrowserStateKeyedServiceFactory {
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
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_SYNC_MODEL_DATA_TYPE_STORE_SERVICE_FACTORY_H_
