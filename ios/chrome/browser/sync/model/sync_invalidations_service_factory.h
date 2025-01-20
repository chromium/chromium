// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_MODEL_SYNC_INVALIDATIONS_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SYNC_MODEL_SYNC_INVALIDATIONS_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

namespace syncer {
class SyncInvalidationsService;
}  // namespace syncer

class SyncInvalidationsServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  // Returned value may be nullptr in case if sync invalidations are disabled or
  // not supported.
  static syncer::SyncInvalidationsService* GetForProfile(ProfileIOS* profile);
  static SyncInvalidationsServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<SyncInvalidationsServiceFactory>;

  SyncInvalidationsServiceFactory();
  ~SyncInvalidationsServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_SYNC_MODEL_SYNC_INVALIDATIONS_SERVICE_FACTORY_H_
