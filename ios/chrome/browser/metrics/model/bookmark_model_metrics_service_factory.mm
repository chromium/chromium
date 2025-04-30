// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/bookmark_model_metrics_service_factory.h"

#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/metrics/model/bookmark_model_metrics_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
BookmarkModelMetricsService* BookmarkModelMetricsServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return static_cast<BookmarkModelMetricsService*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

// static
BookmarkModelMetricsServiceFactory*
BookmarkModelMetricsServiceFactory::GetInstance() {
  static base::NoDestructor<BookmarkModelMetricsServiceFactory> instance;
  return instance.get();
}

BookmarkModelMetricsServiceFactory::BookmarkModelMetricsServiceFactory()
    : ProfileKeyedServiceFactoryIOS("BookmarkModelMetricsServiceFactory",
                                    ServiceCreation::kCreateWithProfile,
                                    TestingCreation::kNoServiceForTests) {
  DependsOn(ios::BookmarkModelFactory::GetInstance());
}

BookmarkModelMetricsServiceFactory::~BookmarkModelMetricsServiceFactory() {}

std::unique_ptr<KeyedService>
BookmarkModelMetricsServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  return std::make_unique<BookmarkModelMetricsService>(
      ios::BookmarkModelFactory::GetForProfile(profile), profile);
}
