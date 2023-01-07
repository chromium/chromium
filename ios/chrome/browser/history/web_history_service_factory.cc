// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/history/web_history_service_factory.h"

#include "base/no_destructor.h"
#include "components/history/core/browser/web_history_service.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/sync_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/chrome/browser/sync/sync_service_factory.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"

namespace ios {
namespace {

// Returns true if the user is signed-in and full history sync is enabled,
// false otherwise.
bool IsHistorySyncEnabled(ChromeBrowserState* browser_state) {
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForBrowserState(browser_state);
  return sync_service && sync_service->IsSyncFeatureActive() &&
         sync_service->GetActiveDataTypes().Has(
             syncer::HISTORY_DELETE_DIRECTIVES);
}

}  // namespace

// static
history::WebHistoryService* WebHistoryServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  // Ensure that the service is not instantiated or used if the user is not
  // signed into sync, or if web history is not enabled.
  if (!IsHistorySyncEnabled(browser_state))
    return nullptr;

  return static_cast<history::WebHistoryService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
WebHistoryServiceFactory* WebHistoryServiceFactory::GetInstance() {
  static base::NoDestructor<WebHistoryServiceFactory> instance;
  return instance.get();
}

WebHistoryServiceFactory::WebHistoryServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "WebHistoryService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(SyncServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

WebHistoryServiceFactory::~WebHistoryServiceFactory() {
}

std::unique_ptr<KeyedService> WebHistoryServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<history::WebHistoryService>(
      IdentityManagerFactory::GetForBrowserState(browser_state),
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          browser_state->GetURLLoaderFactory()));
}

}  // namespace ios
