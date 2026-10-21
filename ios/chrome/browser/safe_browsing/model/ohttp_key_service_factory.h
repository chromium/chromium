// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_OHTTP_KEY_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_OHTTP_KEY_SERVICE_FACTORY_H_

#include "components/safe_browsing/core/browser/hashprefix_realtime/ohttp_key_service.h"
#include "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns OhttpKeyService objects, one for each active
// ProfileIOS. It returns nullptr for incognito ProfileIOS.
class OhttpKeyServiceFactory final : public TypedProfileKeyedServiceFactoryIOS<
                                         OhttpKeyServiceFactory,
                                         safe_browsing::OhttpKeyService> {
 public:
  explicit OhttpKeyServiceFactory(PassKey key);

  // Returns the default factory. Can be used to force instantiation during
  // testing.
  static TestingFactory GetDefaultFactory();

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_OHTTP_KEY_SERVICE_FACTORY_H_
