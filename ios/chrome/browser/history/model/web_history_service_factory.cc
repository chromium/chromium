// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/history/model/web_history_service_factory.h"

#include "base/no_destructor.h"
#include "components/history/core/browser/web_history_service.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/prefs/pref_service.h"
#include "components/sync/service/sync_service.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"
#include "ios/chrome/browser/signin/model/identity_manager_factory.h"
#include "ios/chrome/browser/sync/model/sync_service_factory.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"

namespace ios {
namespace {

// Returns true if the user is signed-in and full history sync is enabled,
// false otherwise.
bool IsHistorySyncEnabled(ProfileIOS* profile) {
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  return sync_service && sync_service->GetActiveDataTypes().Has(
                             syncer::HISTORY_DELETE_DIRECTIVES);
}

}  // namespace

// static
history::WebHistoryService* WebHistoryServiceFactory::GetForBrowserState(
    ProfileIOS* profile) {
  return GetForProfile(profile);
}

// static
history::WebHistoryService* WebHistoryServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  // Ensure that the service is not instantiated or used if the user is not
  // signed into sync, or if web history is not enabled.
  if (!IsHistorySyncEnabled(profile)) {
    return nullptr;
  }

  return static_cast<history::WebHistoryService*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
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
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  return std::make_unique<history::WebHistoryService>(
      IdentityManagerFactory::GetForProfile(profile),
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          profile->GetURLLoaderFactory()));
}

}  // namespace ios
