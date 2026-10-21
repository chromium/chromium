// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/impression_limits/model/impression_limit_service_factory.h"

#import <utility>

#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/commerce/model/shopping_service_factory.h"
#import "ios/chrome/browser/content_suggestions/impression_limits/model/impression_limit_service.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

ImpressionLimitServiceFactory::ImpressionLimitServiceFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(std::move(key),
                                         "ImpressionLimitService",
                                         TestingCreation::kNoServiceForTests) {
  DependsOn(ios::HistoryServiceFactory::GetInstance());
  DependsOn(ios::BookmarkModelFactory::GetInstance());
  DependsOn(commerce::ShoppingServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
ImpressionLimitServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<ImpressionLimitService>(
      profile->GetPrefs(), ios::HistoryServiceFactory::GetForProfile(profile),
      ios::BookmarkModelFactory::GetForProfile(profile),
      commerce::ShoppingServiceFactory::GetForProfile(profile));
}
