// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs_search/tabs_search_service_factory.h"

#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/history/history_service_factory.h"
#import "ios/chrome/browser/history/web_history_service_factory.h"
#import "ios/chrome/browser/main/browser_list_factory.h"
#import "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/sync/session_sync_service_factory.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/tabs_search/tabs_search_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

TabsSearchService* TabsSearchServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<TabsSearchService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

TabsSearchServiceFactory* TabsSearchServiceFactory::GetInstance() {
  static base::NoDestructor<TabsSearchServiceFactory> instance;
  return instance.get();
}

TabsSearchServiceFactory::TabsSearchServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "TabsSearchService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(BrowserListFactory::GetInstance());
  DependsOn(IOSChromeTabRestoreServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(ios::HistoryServiceFactory::GetInstance());
  DependsOn(SessionSyncServiceFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
}

TabsSearchServiceFactory::~TabsSearchServiceFactory() = default;

std::unique_ptr<KeyedService> TabsSearchServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<TabsSearchService>(browser_state);
}

web::BrowserState* TabsSearchServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateOwnInstanceInIncognito(context);
}
