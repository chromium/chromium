// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"

#include "base/containers/extend.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/undo/bookmark_undo_service.h"
#include "ios/chrome/browser/bookmarks/model/account_bookmark_sync_service_factory.h"
#include "ios/chrome/browser/bookmarks/model/bookmark_client_impl.h"
#include "ios/chrome/browser/bookmarks/model/bookmark_storage_type.h"
#include "ios/chrome/browser/bookmarks/model/bookmark_undo_service_factory.h"
#include "ios/chrome/browser/bookmarks/model/local_or_syncable_bookmark_sync_service_factory.h"
#include "ios/chrome/browser/bookmarks/model/managed_bookmark_service_factory.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace ios {

namespace {

std::unique_ptr<KeyedService> BuildBookmarkModel(ProfileIOS* profile) {
  auto bookmark_model = std::make_unique<bookmarks::BookmarkModel>(
      std::make_unique<BookmarkClientImpl>(
          profile, ManagedBookmarkServiceFactory::GetForProfile(profile),
          ios::LocalOrSyncableBookmarkSyncServiceFactory::GetForProfile(
              profile),
          ios::AccountBookmarkSyncServiceFactory::GetForProfile(profile),
          ios::BookmarkUndoServiceFactory::GetForProfile(profile)));
  bookmark_model->Load(profile->GetStatePath());
  ios::BookmarkUndoServiceFactory::GetForProfile(profile)
      ->StartObservingBookmarkModel(bookmark_model.get());
  return bookmark_model;
}

}  // namespace

// static
bookmarks::BookmarkModel* BookmarkModelFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<bookmarks::BookmarkModel>(
      profile, /*create=*/true);
}

// static
bookmarks::BookmarkModel* BookmarkModelFactory::GetForProfileIfExists(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<bookmarks::BookmarkModel>(
      profile, /*create=*/false);
}

// static
BookmarkModelFactory* BookmarkModelFactory::GetInstance() {
  static base::NoDestructor<BookmarkModelFactory> instance;
  return instance.get();
}

// static
BookmarkModelFactory::TestingFactory BookmarkModelFactory::GetDefaultFactory() {
  return base::BindOnce(&BuildBookmarkModel);
}

BookmarkModelFactory::BookmarkModelFactory()
    : ProfileKeyedServiceFactoryIOS("BookmarkModel",
                                    ProfileSelection::kRedirectedInIncognito,
                                    TestingCreation::kNoServiceForTests) {
  DependsOn(ios::AccountBookmarkSyncServiceFactory::GetInstance());
  DependsOn(ios::LocalOrSyncableBookmarkSyncServiceFactory::GetInstance());
  DependsOn(ios::BookmarkUndoServiceFactory::GetInstance());
  DependsOn(ManagedBookmarkServiceFactory::GetInstance());
}

BookmarkModelFactory::~BookmarkModelFactory() = default;

void BookmarkModelFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  bookmarks::RegisterProfilePrefs(registry);
}

std::unique_ptr<KeyedService> BookmarkModelFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return BuildBookmarkModel(profile);
}

}  // namespace ios
