// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/power_bookmarks/model/power_bookmark_service_factory.h"

#import <utility>

#import "base/task/task_traits.h"
#import "components/power_bookmarks/core/power_bookmark_service.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

PowerBookmarkServiceFactory::PowerBookmarkServiceFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "PowerBookmarkService",
          ProfileSelection::kRedirectedInIncognito,
          ServiceCreation::kCreateWithProfile,
          TestingCreation::kNoServiceForTests) {
  DependsOn(ios::BookmarkModelFactory::GetInstance());
}

std::unique_ptr<KeyedService>
PowerBookmarkServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  bookmarks::BookmarkModel* bookmark_model =
      ios::BookmarkModelFactory::GetForProfile(profile);

  return std::make_unique<power_bookmarks::PowerBookmarkService>(
      bookmark_model);
}
