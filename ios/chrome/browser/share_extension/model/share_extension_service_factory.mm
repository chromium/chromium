// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/share_extension/model/share_extension_service_factory.h"

#import "base/feature_list.h"
#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/sync/base/features.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/bookmarks/model/legacy_bookmark_model.h"
#import "ios/chrome/browser/bookmarks/model/local_or_syncable_bookmark_model_factory.h"
#import "ios/chrome/browser/reading_list/model/reading_list_model_factory.h"
#import "ios/chrome/browser/share_extension/model/share_extension_service.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"

// static
ShareExtensionService* ShareExtensionServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<ShareExtensionService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
ShareExtensionService* ShareExtensionServiceFactory::GetForBrowserStateIfExists(
    ChromeBrowserState* browser_state) {
  return static_cast<ShareExtensionService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, false));
}

// static
ShareExtensionServiceFactory* ShareExtensionServiceFactory::GetInstance() {
  static base::NoDestructor<ShareExtensionServiceFactory> instance;
  return instance.get();
}

ShareExtensionServiceFactory::ShareExtensionServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "ShareExtensionService",
          BrowserStateDependencyManager::GetInstance()) {
  if (base::FeatureList::IsEnabled(
          syncer::kEnableBookmarkFoldersForAccountStorage)) {
    DependsOn(ios::BookmarkModelFactory::GetInstance());
  } else {
    DependsOn(ios::LocalOrSyncableBookmarkModelFactory::GetInstance());
  }
  DependsOn(ReadingListModelFactory::GetInstance());
}

ShareExtensionServiceFactory::~ShareExtensionServiceFactory() {}

std::unique_ptr<KeyedService>
ShareExtensionServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* chrome_browser_state =
      ChromeBrowserState::FromBrowserState(context);

  bookmarks::BookmarkModel* bookmark_model =
      base::FeatureList::IsEnabled(
          syncer::kEnableBookmarkFoldersForAccountStorage)
          ? ios::BookmarkModelFactory::
                GetModelForBrowserStateIfUnificationEnabledOrDie(
                    chrome_browser_state)
          : ios::LocalOrSyncableBookmarkModelFactory::
                GetDedicatedUnderlyingModelForBrowserStateIfUnificationDisabledOrDie(
                    chrome_browser_state);

  return std::make_unique<ShareExtensionService>(
      bookmark_model,
      ReadingListModelFactory::GetForBrowserState(chrome_browser_state));
}

web::BrowserState* ShareExtensionServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}
