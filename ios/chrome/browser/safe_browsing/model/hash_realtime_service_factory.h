// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_HASH_REALTIME_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_HASH_REALTIME_SERVICE_FACTORY_H_

#import "components/safe_browsing/core/browser/hashprefix_realtime/hash_realtime_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"
#import "services/network/public/mojom/network_context.mojom.h"

// Singleton that owns HashRealTimeService objects, one for each active profile.
// It returns nullptr for incognito profiles.
class HashRealTimeServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          HashRealTimeServiceFactory,
          safe_browsing::HashRealTimeService> {
 public:
  explicit HashRealTimeServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_HASH_REALTIME_SERVICE_FACTORY_H_
