// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_URL_BLOCKING_MODEL_SAFE_SEARCH_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_POLICY_URL_BLOCKING_MODEL_SAFE_SEARCH_SERVICE_FACTORY_H_

#import <memory>

#import "ios/chrome/browser/policy_url_blocking/model/safe_search_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Owns all `SafeSearchService` instances and associates them with profiles.
class SafeSearchServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<SafeSearchServiceFactory,
                                                SafeSearchService> {
 public:
  explicit SafeSearchServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS& profile) const final;
};

#endif  // IOS_CHROME_BROWSER_POLICY_URL_BLOCKING_MODEL_SAFE_SEARCH_SERVICE_FACTORY_H_
