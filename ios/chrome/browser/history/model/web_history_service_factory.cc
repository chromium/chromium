// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/history/model/web_history_service_factory.h"

#include <utility>

#include "components/history/core/browser/web_history_service.h"
#include "components/prefs/pref_service.h"
#include "components/sync/service/sync_service.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"
#include "ios/chrome/browser/signin/model/identity_manager_factory.h"
#include "ios/chrome/browser/sync/model/sync_service_factory.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"

namespace ios {

WebHistoryServiceFactory::WebHistoryServiceFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(std::move(key), "WebHistoryService") {
  DependsOn(SyncServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

std::unique_ptr<KeyedService> WebHistoryServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<history::WebHistoryService>(
      SyncServiceFactory::GetForProfile(profile),
      IdentityManagerFactory::GetForProfile(profile),
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          profile->GetURLLoaderFactory()));
}

}  // namespace ios
