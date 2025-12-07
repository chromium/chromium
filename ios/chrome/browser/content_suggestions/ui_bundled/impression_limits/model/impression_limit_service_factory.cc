// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/impression_limits/model/impression_limit_service_factory.h"

#import "components/keyed_service/core/service_access_type.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/commerce/model/shopping_service_factory.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/impression_limits/model/impression_limit_service.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
ImpressionLimitServiceFactory* ImpressionLimitServiceFactory::GetInstance() {
  static base::NoDestructor<ImpressionLimitServiceFactory> instance;
  return instance.get();
}

// static
ImpressionLimitService* ImpressionLimitServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<ImpressionLimitService>(
      profile, /*create=*/true);
}

// static
ImpressionLimitService* ImpressionLimitServiceFactory::GetForProfileIfExists(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<ImpressionLimitService>(
      profile, /*create=*/false);
}

ImpressionLimitServiceFactory::ImpressionLimitServiceFactory()
    : ProfileKeyedServiceFactoryIOS("ImpressionLimitService",
                                    TestingCreation::kNoServiceForTests) {
  DependsOn(ios::HistoryServiceFactory::GetInstance());
  DependsOn(ios::BookmarkModelFactory::GetInstance());
  DependsOn(commerce::ShoppingServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
ImpressionLimitServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<ImpressionLimitService>(
      profile->GetPrefs(),
      ios::HistoryServiceFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS),
      ios::BookmarkModelFactory::GetForProfile(profile),
      commerce::ShoppingServiceFactory::GetForProfile(profile));
}
