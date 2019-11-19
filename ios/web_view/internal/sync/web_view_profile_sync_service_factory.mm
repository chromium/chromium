// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/sync/web_view_profile_sync_service_factory.h"

#include <utility>

#include "base/bind_helpers.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/invalidation/impl/profile_invalidation_provider.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/profile_sync_service.h"
#include "components/sync/driver/startup_controller.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_util.h"
#include "ios/web/public/thread/web_thread.h"
#include "ios/web_view/internal/app/application_context.h"
#include "ios/web_view/internal/autofill/web_view_personal_data_manager_factory.h"
#include "ios/web_view/internal/passwords/web_view_password_store_factory.h"
#include "ios/web_view/internal/signin/web_view_identity_manager_factory.h"
#import "ios/web_view/internal/sync/web_view_device_info_sync_service_factory.h"
#import "ios/web_view/internal/sync/web_view_gcm_profile_service_factory.h"
#import "ios/web_view/internal/sync/web_view_model_type_store_service_factory.h"
#import "ios/web_view/internal/sync/web_view_profile_invalidation_provider_factory.h"
#import "ios/web_view/internal/sync/web_view_sync_client.h"
#include "ios/web_view/internal/web_view_browser_state.h"
#include "ios/web_view/internal/webdata_services/web_view_web_data_service_wrapper_factory.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

// static
WebViewProfileSyncServiceFactory*
WebViewProfileSyncServiceFactory::GetInstance() {
  static base::NoDestructor<WebViewProfileSyncServiceFactory> instance;
  return instance.get();
}

// static
syncer::SyncService* WebViewProfileSyncServiceFactory::GetForBrowserState(
    WebViewBrowserState* browser_state) {
  return static_cast<syncer::SyncService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

WebViewProfileSyncServiceFactory::WebViewProfileSyncServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "ProfileSyncService",
          BrowserStateDependencyManager::GetInstance()) {
  // The ProfileSyncService depends on various SyncableServices being around
  // when it is shut down.  Specify those dependencies here to build the proper
  // destruction order.
  DependsOn(WebViewDeviceInfoSyncServiceFactory::GetInstance());
  DependsOn(WebViewIdentityManagerFactory::GetInstance());
  DependsOn(WebViewPersonalDataManagerFactory::GetInstance());
  DependsOn(WebViewWebDataServiceWrapperFactory::GetInstance());
  DependsOn(WebViewPasswordStoreFactory::GetInstance());
  DependsOn(WebViewGCMProfileServiceFactory::GetInstance());
  DependsOn(WebViewProfileInvalidationProviderFactory::GetInstance());
  DependsOn(WebViewModelTypeStoreServiceFactory::GetInstance());
}

WebViewProfileSyncServiceFactory::~WebViewProfileSyncServiceFactory() {}

std::unique_ptr<KeyedService>
WebViewProfileSyncServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  WebViewBrowserState* browser_state =
      WebViewBrowserState::FromBrowserState(context);

  signin::IdentityManager* identity_manager =
      WebViewIdentityManagerFactory::GetForBrowserState(browser_state);
  WebViewGCMProfileServiceFactory::GetForBrowserState(browser_state);

  syncer::ProfileSyncService::InitParams init_params;
  init_params.identity_manager = identity_manager;
  init_params.start_behavior = syncer::ProfileSyncService::MANUAL_START;
  init_params.sync_client = std::make_unique<WebViewSyncClient>(browser_state);
  init_params.url_loader_factory = browser_state->GetSharedURLLoaderFactory();
  // ios/web_view has no need to update network time.
  init_params.network_time_update_callback = base::DoNothing();
  init_params.network_connection_tracker =
      ApplicationContext::GetInstance()->GetNetworkConnectionTracker();
  init_params.channel = version_info::Channel::STABLE;
  init_params.invalidations_identity_providers.push_back(
      WebViewProfileInvalidationProviderFactory::GetForBrowserState(
          browser_state)
          ->GetIdentityProvider());
  init_params.autofill_enable_account_wallet_storage =
      base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableAccountWalletStorage);

  auto profile_sync_service =
      std::make_unique<syncer::ProfileSyncService>(std::move(init_params));
  profile_sync_service->Initialize();

  // Hook PSS into PersonalDataManager (a circular dependency).
  autofill::PersonalDataManager* pdm =
      WebViewPersonalDataManagerFactory::GetForBrowserState(browser_state);
  pdm->OnSyncServiceInitialized(profile_sync_service.get());

  return profile_sync_service;
}

}  // namespace ios_web_view
