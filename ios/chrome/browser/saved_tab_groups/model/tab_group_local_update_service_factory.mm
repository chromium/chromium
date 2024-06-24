// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/saved_tab_groups/model/tab_group_local_update_service_factory.h"

#import "components/keyed_service/core/keyed_service.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_local_update_service.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/sync/model/model_type_store_service_factory.h"
#import "ios/chrome/common/channel_info.h"
#import "ios/web/public/browser_state.h"

namespace tab_groups {

// static
TabGroupLocalUpdateService*
TabGroupLocalUpdateServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  if (browser_state->IsOffTheRecord()) {
    return nullptr;
  }
  return static_cast<TabGroupLocalUpdateService*>(
      GetInstance()->GetServiceForBrowserState(browser_state,
                                               /*create=*/true));
}

TabGroupLocalUpdateServiceFactory*
TabGroupLocalUpdateServiceFactory::GetInstance() {
  static base::NoDestructor<TabGroupLocalUpdateServiceFactory> instance;
  return instance.get();
}

TabGroupLocalUpdateServiceFactory::TabGroupLocalUpdateServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "TabGroupLocalUpdateServiceFactory",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(TabGroupSyncServiceFactory::GetInstance());
  DependsOn(BrowserListFactory::GetInstance());
}

std::unique_ptr<KeyedService>
TabGroupLocalUpdateServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  CHECK(!browser_state->IsOffTheRecord());
  return std::make_unique<TabGroupLocalUpdateService>(
      BrowserListFactory::GetForBrowserState(browser_state),
      TabGroupSyncServiceFactory::GetForBrowserState(browser_state));
}

}  // namespace tab_groups
