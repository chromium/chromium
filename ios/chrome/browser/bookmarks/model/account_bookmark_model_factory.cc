// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/bookmarks/model/account_bookmark_model_factory.h"

#include <utility>
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/bookmark_features.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/features.h"
#include "components/undo/bookmark_undo_service.h"
#include "ios/chrome/browser/bookmarks/model/account_bookmark_sync_service_factory.h"
#include "ios/chrome/browser/bookmarks/model/bookmark_client_impl.h"
#include "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#include "ios/chrome/browser/bookmarks/model/bookmark_undo_service_factory.h"
#include "ios/chrome/browser/bookmarks/model/legacy_bookmark_model_with_dedicated_underlying_model.h"
#include "ios/chrome/browser/bookmarks/model/legacy_bookmark_model_with_shared_underlying_model.h"
#include "ios/chrome/browser/bookmarks/model/local_or_syncable_bookmark_model_factory.h"
#include "ios/chrome/browser/bookmarks/model/local_or_syncable_bookmark_sync_service_factory.h"
#include "ios/chrome/browser/history/model/history_service_factory.h"
#include "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"

namespace ios {

namespace {

std::unique_ptr<KeyedService> BuildLegacyBookmarkModelWithSharedUnderlyingModel(
    ChromeBrowserState* browser_state) {
  CHECK(base::FeatureList::IsEnabled(
      syncer::kEnableBookmarkFoldersForAccountStorage));
  // Using nullptr for `ManagedBookmarkService`, since managed bookmarks
  // affect only the local bookmark storage.
  return std::make_unique<LegacyBookmarkModelWithSharedUnderlyingModel>(
      ios::BookmarkModelFactory::
          GetModelForBrowserStateIfUnificationEnabledOrDie(browser_state),
      bookmarks::BookmarkModel::NodeTypeForUuidLookup::kAccountNodes,
      /*managed_bookmark_service=*/nullptr);
}

std::unique_ptr<KeyedService>
BuildLegacyBookmarkModelWithDedicatedUnderlyingModel(
    ChromeBrowserState* browser_state) {
  CHECK(!base::FeatureList::IsEnabled(
      syncer::kEnableBookmarkFoldersForAccountStorage));
  // When using a dedicated BookmarkModel instance, account bookmarks are
  // actually stored as descendants of the local-or-syncable permanent folders
  // in the dedicated BookmarkModel instance (created here). Their sync
  // metadata is also persisted as regular (local-or-syncable) sync metadata, so
  // BookmarkClientImpl takes a null `account_bookmark_sync_service`.
  //
  // Also, using nullptr for `ManagedBookmarkService`, since managed bookmarks
  // affect only the local bookmark storage.
  auto bookmark_model = std::make_unique<bookmarks::BookmarkModel>(
      std::make_unique<BookmarkClientImpl>(
          browser_state, /*managed_bookmark_service=*/nullptr,
          ios::AccountBookmarkSyncServiceFactory::GetForBrowserState(
              browser_state),
          /*account_bookmark_sync_service=*/nullptr,
          ios::BookmarkUndoServiceFactory::GetForBrowserState(browser_state)));
  bookmark_model->LoadAccountBookmarksFileAsLocalOrSyncableBookmarks(
      browser_state->GetStatePath());
  ios::BookmarkUndoServiceFactory::GetForBrowserState(browser_state)
      ->StartObservingBookmarkModel(bookmark_model.get());
  return std::make_unique<LegacyBookmarkModelWithDedicatedUnderlyingModel>(
      std::move(bookmark_model), /*managed_bookmark_service=*/nullptr);
}

std::unique_ptr<KeyedService> BuildLegacyBookmarkModel(
    web::BrowserState* context) {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);

  if (base::FeatureList::IsEnabled(
          syncer::kEnableBookmarkFoldersForAccountStorage)) {
    return BuildLegacyBookmarkModelWithSharedUnderlyingModel(browser_state);
  }

  return BuildLegacyBookmarkModelWithDedicatedUnderlyingModel(browser_state);
}

}  // namespace

// static
LegacyBookmarkModel* AccountBookmarkModelFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<LegacyBookmarkModel*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
bookmarks::BookmarkModel* AccountBookmarkModelFactory::
    GetDedicatedUnderlyingModelForBrowserStateIfUnificationDisabledOrDie(
        ChromeBrowserState* browser_state) {
  CHECK(!base::FeatureList::IsEnabled(
      syncer::kEnableBookmarkFoldersForAccountStorage));
  LegacyBookmarkModel* model = GetForBrowserState(browser_state);
  return model ? model->underlying_model() : nullptr;
}

// static
AccountBookmarkModelFactory* AccountBookmarkModelFactory::GetInstance() {
  static base::NoDestructor<AccountBookmarkModelFactory> instance;
  return instance.get();
}

// static
AccountBookmarkModelFactory::TestingFactory
AccountBookmarkModelFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildLegacyBookmarkModel);
}

AccountBookmarkModelFactory::AccountBookmarkModelFactory()
    : BrowserStateKeyedServiceFactory(
          "AccountBookmarkModel",
          BrowserStateDependencyManager::GetInstance()) {
  if (base::FeatureList::IsEnabled(
          syncer::kEnableBookmarkFoldersForAccountStorage)) {
    DependsOn(ios::BookmarkModelFactory::GetInstance());
  } else {
    // Bookmark-related prefs are registered by the LocalOrSyncable factory.
    DependsOn(ios::LocalOrSyncableBookmarkModelFactory::GetInstance());
    DependsOn(ios::AccountBookmarkSyncServiceFactory::GetInstance());
    DependsOn(ios::BookmarkUndoServiceFactory::GetInstance());
  }
}

AccountBookmarkModelFactory::~AccountBookmarkModelFactory() = default;

std::unique_ptr<KeyedService>
AccountBookmarkModelFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return BuildLegacyBookmarkModel(context);
}

web::BrowserState* AccountBookmarkModelFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}

}  // namespace ios
