// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/bookmark_model_metrics_service_factory.h"

#import <utility>

#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/metrics/model/bookmark_model_metrics_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

BookmarkModelMetricsServiceFactory::BookmarkModelMetricsServiceFactory(
    PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(std::move(key),
                                         "BookmarkModelMetricsServiceFactory",
                                         ServiceCreation::kCreateWithProfile,
                                         TestingCreation::kNoServiceForTests) {
  DependsOn(ios::BookmarkModelFactory::GetInstance());
}

std::unique_ptr<KeyedService>
BookmarkModelMetricsServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<BookmarkModelMetricsService>(
      ios::BookmarkModelFactory::GetForProfile(profile), profile);
}
