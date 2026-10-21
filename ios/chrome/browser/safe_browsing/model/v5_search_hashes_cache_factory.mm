// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/v5_search_hashes_cache_factory.h"

#import <utility>

#import "components/safe_browsing/core/browser/db/v5_search_hashes_cache.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

V5SearchHashesCacheFactory::V5SearchHashesCacheFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "V5SearchHashesCache",
          ProfileSelection::kOwnInstanceInIncognito) {
  DependsOn(ios::HistoryServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
V5SearchHashesCacheFactory::BuildServiceInstanceFor(ProfileIOS* profile) const {
  return std::make_unique<safe_browsing::V5SearchHashesCache>(
      ios::HistoryServiceFactory::GetForProfile(profile));
}
