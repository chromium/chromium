// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"

#include <utility>
#include "base/no_destructor.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/prefs/pref_service.h"
#include "components/undo/bookmark_undo_service.h"
#include "ios/chrome/browser/bookmarks/bookmark_client_impl.h"
#include "ios/chrome/browser/bookmarks/bookmark_sync_service_factory.h"
#import "ios/chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#include "ios/chrome/browser/undo/bookmark_undo_service_factory.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"

namespace ios {

namespace {

std::unique_ptr<KeyedService> BuildBookmarkModel(web::BrowserState* context) {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model(
      new bookmarks::BookmarkModel(std::make_unique<BookmarkClientImpl>(
          browser_state,
          ManagedBookmarkServiceFactory::GetForBrowserState(browser_state),
          ios::BookmarkSyncServiceFactory::GetForBrowserState(browser_state))));
  bookmark_model->Load(browser_state->GetPrefs(),
                       browser_state->GetStatePath());
  ios::BookmarkUndoServiceFactory::GetForBrowserState(browser_state)
      ->Start(bookmark_model.get());
  return bookmark_model;
}

}  // namespace

// static
bookmarks::BookmarkModel* BookmarkModelFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<bookmarks::BookmarkModel*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
bookmarks::BookmarkModel* BookmarkModelFactory::GetForBrowserStateIfExists(
    ChromeBrowserState* browser_state) {
  return static_cast<bookmarks::BookmarkModel*>(
      GetInstance()->GetServiceForBrowserState(browser_state, false));
}

// static
BookmarkModelFactory* BookmarkModelFactory::GetInstance() {
  static base::NoDestructor<BookmarkModelFactory> instance;
  return instance.get();
}

// static
BookmarkModelFactory::TestingFactory BookmarkModelFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildBookmarkModel);
}

BookmarkModelFactory::BookmarkModelFactory()
    : BrowserStateKeyedServiceFactory(
          "BookmarkModel",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ios::BookmarkSyncServiceFactory::GetInstance());
  DependsOn(ios::BookmarkUndoServiceFactory::GetInstance());
  DependsOn(ManagedBookmarkServiceFactory::GetInstance());
}

BookmarkModelFactory::~BookmarkModelFactory() {}

void BookmarkModelFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  bookmarks::RegisterProfilePrefs(registry);
}

std::unique_ptr<KeyedService> BookmarkModelFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return BuildBookmarkModel(context);
}

web::BrowserState* BookmarkModelFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}

bool BookmarkModelFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace ios
