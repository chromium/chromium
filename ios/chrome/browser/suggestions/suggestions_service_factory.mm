// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/suggestions/suggestions_service_factory.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/task/post_task.h"
#include "base/time/default_tick_clock.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/suggestions/blacklist_store.h"
#include "components/suggestions/suggestions_service_impl.h"
#include "components/suggestions/suggestions_store.h"
#include "components/sync/driver/sync_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/thread/web_thread.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace suggestions {

// static
SuggestionsServiceFactory* SuggestionsServiceFactory::GetInstance() {
  static base::NoDestructor<SuggestionsServiceFactory> instance;
  return instance.get();
}

// static
SuggestionsService* SuggestionsServiceFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<SuggestionsService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

SuggestionsServiceFactory::SuggestionsServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "SuggestionsService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(ProfileSyncServiceFactory::GetInstance());
}

SuggestionsServiceFactory::~SuggestionsServiceFactory() {
}

std::unique_ptr<KeyedService>
SuggestionsServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForBrowserState(browser_state);
  syncer::SyncService* sync_service =
      ProfileSyncServiceFactory::GetForBrowserState(browser_state);

  std::unique_ptr<SuggestionsStore> suggestions_store(
      new SuggestionsStore(browser_state->GetPrefs()));
  std::unique_ptr<BlacklistStore> blacklist_store(
      new BlacklistStore(browser_state->GetPrefs()));

  return std::make_unique<SuggestionsServiceImpl>(
      identity_manager, sync_service,
      browser_state->GetSharedURLLoaderFactory(), std::move(suggestions_store),
      std::move(blacklist_store), base::DefaultTickClock::GetInstance());
}

void SuggestionsServiceFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  SuggestionsServiceImpl::RegisterProfilePrefs(registry);
}

}  // namespace suggestions
