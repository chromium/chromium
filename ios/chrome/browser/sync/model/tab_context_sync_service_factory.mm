// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sync/model/tab_context_sync_service_factory.h"

#import <memory>
#import <optional>
#import <utility>

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/raw_ptr.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/sync/base/passphrase_enums.h"
#import "components/sync/base/report_unrecoverable_error.h"
#import "components/sync/model/client_tag_based_data_type_processor.h"
#import "components/sync/model/data_type_store_service.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "components/sync_tab_context/http_rpc_based_ephemeral_key_fetcher.h"
#import "components/sync_tab_context/http_rpc_constants.h"
#import "components/sync_tab_context/tab_context_sync_client.h"
#import "components/sync_tab_context/tab_context_sync_service_impl.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/data_type_store_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/common/channel_info.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

class TabContextSyncClientIOS : public sync_tab_context::TabContextSyncClient {
 public:
  explicit TabContextSyncClientIOS(ProfileIOS* profile) : profile_(profile) {
    CHECK(profile_);
  }

  ~TabContextSyncClientIOS() override = default;

  std::optional<syncer::PassphraseType> GetPassphraseType() const override {
    // `SyncServiceFactory` depends on `TabContextSyncServiceFactory`, so
    // `SyncService` must be retrieved lazily at runtime rather than injected at
    // construction time.
    const syncer::SyncService* const sync_service =
        SyncServiceFactory::GetForProfile(profile_);
    if (!sync_service) {
      return std::nullopt;
    }
    return sync_service->GetUserSettings()->GetPassphraseType();
  }

 private:
  const raw_ptr<ProfileIOS> profile_;
};

}  // namespace

TabContextSyncServiceFactory::TabContextSyncServiceFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(std::move(key),
                                         "TabContextSyncService",
                                         TestingCreation::kNoServiceForTests) {
  DependsOn(DataTypeStoreServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

std::unique_ptr<KeyedService>
TabContextSyncServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  syncer::DataTypeStoreService* const store_service =
      DataTypeStoreServiceFactory::GetForProfile(profile);
  signin::IdentityManager* const identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!store_service || !identity_manager) {
    return nullptr;
  }

  // `base::Unretained(profile)` is safe because `ProfileIOS` outlives all
  // `KeyedService` instances associated with it.
  auto ephemeral_key_fetcher =
      std::make_unique<sync_tab_context::HttpRpcBasedEphemeralKeyFetcher>(
          identity_manager,
          base::BindRepeating(&ProfileIOS::GetSharedURLLoaderFactory,
                              base::Unretained(profile)),
          sync_tab_context::GetEphemeralKeyServerUrl());

  return std::make_unique<sync_tab_context::TabContextSyncServiceImpl>(
      std::make_unique<TabContextSyncClientIOS>(profile),
      store_service->GetStoreFactory(), std::move(ephemeral_key_fetcher),
      base::BindRepeating(&syncer::ReportUnrecoverableError, ::GetChannel()));
}
