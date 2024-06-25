// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/bookmarks/model/local_or_syncable_bookmark_model_factory.h"

#include <memory>
#include <utility>

#include "base/no_destructor.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#include "ios/chrome/browser/bookmarks/model/legacy_bookmark_model_with_shared_underlying_model.h"
#include "ios/chrome/browser/bookmarks/model/managed_bookmark_service_factory.h"
#include "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"

namespace ios {

namespace {

std::unique_ptr<KeyedService> BuildLegacyBookmarkModel(
    web::BrowserState* context) {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);

  return std::make_unique<LegacyBookmarkModelWithSharedUnderlyingModel>(
      ios::BookmarkModelFactory::GetForBrowserState(browser_state),
      bookmarks::BookmarkModel::NodeTypeForUuidLookup::kLocalOrSyncableNodes,
      ManagedBookmarkServiceFactory::GetForBrowserState(browser_state));
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
  DependsOn(ios::BookmarkModelFactory::GetInstance());
}

LocalOrSyncableBookmarkModelFactory::~LocalOrSyncableBookmarkModelFactory() {}

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
