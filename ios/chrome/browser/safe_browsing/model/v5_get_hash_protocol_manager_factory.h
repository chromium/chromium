// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_V5_GET_HASH_PROTOCOL_MANAGER_FACTORY_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_V5_GET_HASH_PROTOCOL_MANAGER_FACTORY_H_

#import "components/safe_browsing/core/browser/db/v5_get_hash_protocol_manager.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns V5GetHashProtocolManager objects, one for each active
// profile.
class V5GetHashProtocolManagerFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          V5GetHashProtocolManagerFactory,
          safe_browsing::V5GetHashProtocolManager> {
 public:
  explicit V5GetHashProtocolManagerFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_V5_GET_HASH_PROTOCOL_MANAGER_FACTORY_H_
