// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy_url_blocking/model/safe_search_service_factory.h"

#import <memory>
#import <utility>

#import "ios/chrome/browser/policy_url_blocking/model/safe_search_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

SafeSearchServiceFactory::SafeSearchServiceFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "SafeSearchService",
          ProfileSelection::kOwnInstanceInIncognito) {}

std::unique_ptr<KeyedService> SafeSearchServiceFactory::BuildServiceInstanceFor(
    ProfileIOS& profile) const {
  return std::make_unique<SafeSearchService>(
      profile.GetSharedURLLoaderFactory());
}
