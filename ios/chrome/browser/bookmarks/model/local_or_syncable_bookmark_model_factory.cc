// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/bookmarks/model/local_or_syncable_bookmark_model_factory.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/features.h"
#include "components/undo/bookmark_undo_service.h"
#include "ios/chrome/browser/bookmarks/model/bookmark_client_impl.h"
#include "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#include "ios/chrome/browser/bookmarks/model/bookmark_undo_service_factory.h"
#include "ios/chrome/browser/bookmarks/model/legacy_bookmark_model_with_dedicated_underlying_model.h"
#include "ios/chrome/browser/bookmarks/model/legacy_bookmark_model_with_shared_underlying_model.h"
#include "ios/chrome/browser/bookmarks/model/local_or_syncable_bookmark_sync_service_factory.h"
#include "ios/chrome/browser/bookmarks/model/managed_bookmark_service_factory.h"
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
  return std::make_unique<LegacyBookmarkModelWithSharedUnderlyingModel>(
      ios::BookmarkModelFactory::
          GetModelForBrowserStateIfUnificationEnabledOrDie(browser_state),
      bookmarks::BookmarkModel::NodeTypeForUuidLookup::kLocalOrSyncableNodes,
      ManagedBookmarkServiceFactory::GetForBrowserState(browser_state));
}

std::unique_ptr<KeyedService>
BuildLegacyBookmarkModelWithDedicatedUnderlyingModel(
    ChromeBrowserState* browser_state) {
  CHECK(!base::FeatureList::IsEnabled(
      syncer::kEnableBookmarkFoldersForAccountStorage));

  bookmarks::ManagedBookmarkService* managed_bookmark_service =
      ManagedBookmarkServiceFactory::GetForBrowserState(browser_state);

  // When using a dedicated BookmarkModel instance, another factory
  // (AccountBookmarkModelFactory) deals with account bookmarks. Hence,
  // dependencies related to account bookmarks can be null here. This includes
  // BookmarkClientImpl, which AccountBookmarkModelFactory instantiates
  // separately.
  auto bookmark_model = std::make_unique<bookmarks::BookmarkModel>(
      std::make_unique<BookmarkClientImpl>(
          browser_state, managed_bookmark_service,
          ios::LocalOrSyncableBookmarkSyncServiceFactory::GetForBrowserState(
              browser_state),
          /*account_bookmark_sync_service=*/nullptr,
          ios::BookmarkUndoServiceFactory::GetForBrowserState(browser_state)));
  bookmark_model->Load(browser_state->GetStatePath());
  ios::BookmarkUndoServiceFactory::GetForBrowserState(browser_state)
      ->StartObservingBookmarkModel(bookmark_model.get());
  return std::make_unique<LegacyBookmarkModelWithDedicatedUnderlyingModel>(
      std::move(bookmark_model), managed_bookmark_service);
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
LegacyBookmarkModel* LocalOrSyncableBookmarkModelFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<LegacyBookmarkModel*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
LegacyBookmarkModel*
LocalOrSyncableBookmarkModelFactory::GetForBrowserStateIfExists(
    ChromeBrowserState* browser_state) {
  return static_cast<LegacyBookmarkModel*>(
      GetInstance()->GetServiceForBrowserState(browser_state, false));
}

// static
bookmarks::BookmarkModel* LocalOrSyncableBookmarkModelFactory::
    GetDedicatedUnderlyingModelForBrowserStateIfUnificationDisabledOrDie(
        ChromeBrowserState* browser_state) {
  CHECK(!base::FeatureList::IsEnabled(
      syncer::kEnableBookmarkFoldersForAccountStorage));
  LegacyBookmarkModel* model = GetForBrowserState(browser_state);
  return model ? model->underlying_model() : nullptr;
}

// static
LocalOrSyncableBookmarkModelFactory*
LocalOrSyncableBookmarkModelFactory::GetInstance() {
  static base::NoDestructor<LocalOrSyncableBookmarkModelFactory> instance;
  return instance.get();
}

// static
LocalOrSyncableBookmarkModelFactory::TestingFactory
LocalOrSyncableBookmarkModelFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildLegacyBookmarkModel);
}

LocalOrSyncableBookmarkModelFactory::LocalOrSyncableBookmarkModelFactory()
    : BrowserStateKeyedServiceFactory(
          "LocalOrSyncableBookmarkModel",
          BrowserStateDependencyManager::GetInstance()) {
  if (base::FeatureList::IsEnabled(
          syncer::kEnableBookmarkFoldersForAccountStorage)) {
    DependsOn(ios::BookmarkModelFactory::GetInstance());
  } else {
    DependsOn(ios::LocalOrSyncableBookmarkSyncServiceFactory::GetInstance());
    DependsOn(ios::BookmarkUndoServiceFactory::GetInstance());
    DependsOn(ManagedBookmarkServiceFactory::GetInstance());
  }
}

LocalOrSyncableBookmarkModelFactory::~LocalOrSyncableBookmarkModelFactory() {}

void LocalOrSyncableBookmarkModelFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  if (!base::FeatureList::IsEnabled(
          syncer::kEnableBookmarkFoldersForAccountStorage)) {
    bookmarks::RegisterProfilePrefs(registry);
  }
}

std::unique_ptr<KeyedService>
LocalOrSyncableBookmarkModelFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return BuildLegacyBookmarkModel(context);
}

web::BrowserState* LocalOrSyncableBookmarkModelFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}

bool LocalOrSyncableBookmarkModelFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace ios
