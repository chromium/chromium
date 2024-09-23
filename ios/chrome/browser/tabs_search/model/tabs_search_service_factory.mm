// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs_search/model/tabs_search_service_factory.h"

#import "base/check.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/history/model/web_history_service_factory.h"
#import "ios/chrome/browser/sessions/model/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/session_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/tabs_search/model/tabs_search_service.h"

namespace {

history::WebHistoryService* WebHistoryServiceGetter(
    base::WeakPtr<ProfileIOS> weak_profile) {
  DCHECK(weak_profile.get())
      << "Getter should not be called after ProfileIOS destruction.";
  return ios::WebHistoryServiceFactory::GetForProfile(weak_profile.get());
}

}  // namespace

// static
TabsSearchService* TabsSearchServiceFactory::GetForBrowserState(
    ProfileIOS* profile) {
  return GetForProfile(profile);
}

// static
TabsSearchService* TabsSearchServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return static_cast<TabsSearchService*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
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
  DependsOn(ios::WebHistoryServiceFactory::GetInstance());
  DependsOn(SessionSyncServiceFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
}

TabsSearchServiceFactory::~TabsSearchServiceFactory() = default;

std::unique_ptr<KeyedService> TabsSearchServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);

  const bool is_off_the_record = profile->IsOffTheRecord();
  return std::make_unique<TabsSearchService>(
      is_off_the_record, BrowserListFactory::GetForProfile(profile),
      IdentityManagerFactory::GetForProfile(profile),
      SyncServiceFactory::GetForProfile(profile),
      IOSChromeTabRestoreServiceFactory::GetForProfile(profile),
      SessionSyncServiceFactory::GetForProfile(profile),
      is_off_the_record ? nullptr
                        : ios::HistoryServiceFactory::GetForProfile(
                              profile, ServiceAccessType::EXPLICIT_ACCESS),
      is_off_the_record ? TabsSearchService::WebHistoryServiceGetter()
                        : base::BindRepeating(&WebHistoryServiceGetter,
                                              profile->AsWeakPtr()));
}

web::BrowserState* TabsSearchServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateOwnInstanceInIncognito(context);
}
