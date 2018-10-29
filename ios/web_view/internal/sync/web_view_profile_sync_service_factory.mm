// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/sync/web_view_profile_sync_service_factory.h"

#include <utility>

#include "base/memory/singleton.h"
#include "base/time/time.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/invalidation/impl/profile_invalidation_provider.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/signin/core/browser/device_id_helper.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/startup_controller.h"
#include "components/sync/driver/sync_util.h"
#include "ios/web/public/web_thread.h"
#include "ios/web_view/internal/app/application_context.h"
#include "ios/web_view/internal/autofill/web_view_personal_data_manager_factory.h"
#include "ios/web_view/internal/passwords/web_view_password_store_factory.h"
#include "ios/web_view/internal/signin/web_view_identity_manager_factory.h"
#include "ios/web_view/internal/signin/web_view_oauth2_token_service_factory.h"
#import "ios/web_view/internal/sync/web_view_gcm_profile_service_factory.h"
#import "ios/web_view/internal/sync/web_view_model_type_store_service_factory.h"
#import "ios/web_view/internal/sync/web_view_profile_invalidation_provider_factory.h"
#import "ios/web_view/internal/sync/web_view_sync_client.h"
#include "ios/web_view/internal/web_view_browser_state.h"
#include "ios/web_view/internal/webdata_services/web_view_web_data_service_wrapper_factory.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

using browser_sync::ProfileSyncService;

// static
WebViewProfileSyncServiceFactory*
WebViewProfileSyncServiceFactory::GetInstance() {
  return base::Singleton<WebViewProfileSyncServiceFactory>::get();
}

// static
ProfileSyncService* WebViewProfileSyncServiceFactory::GetForBrowserState(
    WebViewBrowserState* browser_state) {
  return static_cast<ProfileSyncService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

WebViewProfileSyncServiceFactory::WebViewProfileSyncServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "ProfileSyncService",
          BrowserStateDependencyManager::GetInstance()) {
  // The ProfileSyncService depends on various SyncableServices being around
  // when it is shut down.  Specify those dependencies here to build the proper
  // destruction order.
  DependsOn(WebViewIdentityManagerFactory::GetInstance());
  DependsOn(WebViewOAuth2TokenServiceFactory::GetInstance());
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

  identity::IdentityManager* identity_manager =
      WebViewIdentityManagerFactory::GetForBrowserState(browser_state);
  WebViewGCMProfileServiceFactory::GetForBrowserState(browser_state);

  ProfileSyncService::InitParams init_params;
  init_params.identity_manager = identity_manager;
  init_params.start_behavior = ProfileSyncService::MANUAL_START;
  init_params.sync_client = std::make_unique<WebViewSyncClient>(browser_state);
  init_params.url_loader_factory = browser_state->GetSharedURLLoaderFactory();
  // ios/web_view has no need to update network time.
  init_params.network_time_update_callback = base::DoNothing();
  init_params.signin_scoped_device_id_callback = base::BindRepeating(
      &signin::GetSigninScopedDeviceId, browser_state->GetPrefs());
  init_params.network_connection_tracker =
      ApplicationContext::GetInstance()->GetNetworkConnectionTracker();
  init_params.invalidations_identity_providers.push_back(
      WebViewProfileInvalidationProviderFactory::GetForBrowserState(
          browser_state)
          ->GetIdentityProvider());

  auto profile_sync_service =
      std::make_unique<ProfileSyncService>(std::move(init_params));
  profile_sync_service->Initialize();
  return profile_sync_service;
}

}  // namespace ios_web_view
