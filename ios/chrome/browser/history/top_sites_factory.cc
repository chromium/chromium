// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/history/top_sites_factory.h"

#include <memory>

#include "base/bind.h"
#include "base/no_destructor.h"
#include "components/history/core/browser/history_constants.h"
#include "components/history/core/browser/top_sites_impl.h"
#include "components/keyed_service/core/refcounted_keyed_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#include "ios/chrome/browser/history/history_utils.h"
#include "ios/web/public/thread/web_thread.h"

namespace ios {

// static
scoped_refptr<history::TopSites> TopSitesFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  return base::WrapRefCounted(static_cast<history::TopSites*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true).get()));
}

// static
TopSitesFactory* TopSitesFactory::GetInstance() {
  static base::NoDestructor<TopSitesFactory> instance;
  return instance.get();
}

TopSitesFactory::TopSitesFactory()
    : RefcountedBrowserStateKeyedServiceFactory(
          "TopSites",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ios::HistoryServiceFactory::GetInstance());
}

TopSitesFactory::~TopSitesFactory() {
}

scoped_refptr<RefcountedKeyedService> TopSitesFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);
  history::HistoryService* history_service =
      ios::HistoryServiceFactory::GetForBrowserState(
          browser_state, ServiceAccessType::EXPLICIT_ACCESS);
  scoped_refptr<history::TopSitesImpl> top_sites(new history::TopSitesImpl(
      browser_state->GetPrefs(), history_service,
      history::PrepopulatedPageList(), base::Bind(CanAddURLToHistory)));
  top_sites->Init(
      browser_state->GetStatePath().Append(history::kTopSitesFilename));
  return top_sites;
}

void TopSitesFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  history::TopSitesImpl::RegisterPrefs(registry);
}

bool TopSitesFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace ios
