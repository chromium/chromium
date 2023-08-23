// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/bookmarks/model/local_or_syncable_bookmark_model_factory.h"

#include <memory>
#include <utility>

#include "base/no_destructor.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/storage_type.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/prefs/pref_service.h"
#include "components/undo/bookmark_undo_service.h"
#include "ios/chrome/browser/bookmarks/model/bookmark_client_impl.h"
#include "ios/chrome/browser/bookmarks/model/bookmark_undo_service_factory.h"
#include "ios/chrome/browser/bookmarks/model/local_or_syncable_bookmark_sync_service_factory.h"
#import "ios/chrome/browser/bookmarks/model/managed_bookmark_service_factory.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#include "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"

namespace ios {

namespace {

std::unique_ptr<KeyedService> BuildBookmarkModel(web::BrowserState* context) {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model = std::make_unique<
      bookmarks::BookmarkModel>(std::make_unique<BookmarkClientImpl>(
      browser_state,
      ManagedBookmarkServiceFactory::GetForBrowserState(browser_state),
      ios::LocalOrSyncableBookmarkSyncServiceFactory::GetForBrowserState(
          browser_state),
      ios::BookmarkUndoServiceFactory::GetForBrowserState(browser_state),
      bookmarks::StorageType::kLocalOrSyncable));
  bookmark_model->Load(browser_state->GetStatePath(),
                       bookmarks::StorageType::kLocalOrSyncable);
  ios::BookmarkUndoServiceFactory::GetForBrowserState(browser_state)
      ->StartObservingBookmarkModel(bookmark_model.get());
  return bookmark_model;
}

}  // namespace

// static
bookmarks::BookmarkModel*
LocalOrSyncableBookmarkModelFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<bookmarks::BookmarkModel*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
bookmarks::BookmarkModel*
LocalOrSyncableBookmarkModelFactory::GetForBrowserStateIfExists(
    ChromeBrowserState* browser_state) {
  return static_cast<bookmarks::BookmarkModel*>(
      GetInstance()->GetServiceForBrowserState(browser_state, false));
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
  return base::BindRepeating(&BuildBookmarkModel);
}

LocalOrSyncableBookmarkModelFactory::LocalOrSyncableBookmarkModelFactory()
    : BrowserStateKeyedServiceFactory(
          "LocalOrSyncableBookmarkModel",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ios::LocalOrSyncableBookmarkSyncServiceFactory::GetInstance());
  DependsOn(ios::BookmarkUndoServiceFactory::GetInstance());
  DependsOn(ManagedBookmarkServiceFactory::GetInstance());
}

LocalOrSyncableBookmarkModelFactory::~LocalOrSyncableBookmarkModelFactory() {}

void LocalOrSyncableBookmarkModelFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  bookmarks::RegisterProfilePrefs(registry);
}

std::unique_ptr<KeyedService>
LocalOrSyncableBookmarkModelFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return BuildBookmarkModel(context);
}

web::BrowserState* LocalOrSyncableBookmarkModelFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}

bool LocalOrSyncableBookmarkModelFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace ios
