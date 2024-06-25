// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/bookmarks/model/account_bookmark_model_factory.h"

#include <utility>

#include "base/no_destructor.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#include "ios/chrome/browser/bookmarks/model/legacy_bookmark_model_with_shared_underlying_model.h"
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

  // Using nullptr for `ManagedBookmarkService`, since managed bookmarks
  // affect only the local bookmark storage.
  return std::make_unique<LegacyBookmarkModelWithSharedUnderlyingModel>(
      ios::BookmarkModelFactory::GetForBrowserState(browser_state),
      bookmarks::BookmarkModel::NodeTypeForUuidLookup::kAccountNodes,
      /*managed_bookmark_service=*/nullptr);
}

}  // namespace

// static
LegacyBookmarkModel* AccountBookmarkModelFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<LegacyBookmarkModel*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
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
  DependsOn(ios::BookmarkModelFactory::GetInstance());
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
