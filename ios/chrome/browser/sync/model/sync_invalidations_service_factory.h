// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_MODEL_SYNC_INVALIDATIONS_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SYNC_MODEL_SYNC_INVALIDATIONS_SERVICE_FACTORY_H_

#import <memory>

#import "components/sync/invalidations/sync_invalidations_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

class SyncInvalidationsServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          SyncInvalidationsServiceFactory,
          syncer::SyncInvalidationsService> {
 public:
  explicit SyncInvalidationsServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_SYNC_MODEL_SYNC_INVALIDATIONS_SERVICE_FACTORY_H_
