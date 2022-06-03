// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/share_extension/share_extension_service_factory.h"

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#include "ios/chrome/browser/share_extension/share_extension_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
  DependsOn(ios::BookmarkModelFactory::GetInstance());
  DependsOn(ReadingListModelFactory::GetInstance());
}

ShareExtensionServiceFactory::~ShareExtensionServiceFactory() {}

std::unique_ptr<KeyedService>
ShareExtensionServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* chrome_browser_state =
      ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<ShareExtensionService>(
      ios::BookmarkModelFactory::GetForBrowserState(chrome_browser_state),
      ReadingListModelFactory::GetForBrowserState(chrome_browser_state));
}

web::BrowserState* ShareExtensionServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}
