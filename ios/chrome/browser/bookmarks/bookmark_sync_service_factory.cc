// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/bookmarks/bookmark_sync_service_factory.h"

#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/sync_bookmarks/bookmark_sync_service.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/undo/bookmark_undo_service_factory.h"

namespace ios {

// static
sync_bookmarks::BookmarkSyncService*
BookmarkSyncServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<sync_bookmarks::BookmarkSyncService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
BookmarkSyncServiceFactory* BookmarkSyncServiceFactory::GetInstance() {
  static base::NoDestructor<BookmarkSyncServiceFactory> instance;
  return instance.get();
}

BookmarkSyncServiceFactory::BookmarkSyncServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "BookmarkSyncServiceFactory",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ios::BookmarkUndoServiceFactory::GetInstance());
}

BookmarkSyncServiceFactory::~BookmarkSyncServiceFactory() {}

std::unique_ptr<KeyedService>
BookmarkSyncServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  std::unique_ptr<sync_bookmarks::BookmarkSyncService> bookmark_sync_service(
      new sync_bookmarks::BookmarkSyncService(
          BookmarkUndoServiceFactory::GetForBrowserStateIfExists(
              browser_state)));
  return bookmark_sync_service;
}

web::BrowserState* BookmarkSyncServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}

}  // namespace ios
