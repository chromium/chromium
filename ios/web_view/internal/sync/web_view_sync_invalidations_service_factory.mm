// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/internal/sync/web_view_sync_invalidations_service_factory.h"

#include "base/no_destructor.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/gcm_driver/instance_id/instance_id_profile_service.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/sync/invalidations/sync_invalidations_service_impl.h"
#include "ios/web_view/internal/sync/web_view_gcm_profile_service_factory.h"
#include "ios/web_view/internal/sync/web_view_instance_id_profile_service_factory.h"
#include "ios/web_view/internal/web_view_browser_state.h"

namespace ios_web_view {

// static
syncer::SyncInvalidationsService*
WebViewSyncInvalidationsServiceFactory::GetForBrowserState(
    WebViewBrowserState* browser_state) {
  return static_cast<syncer::SyncInvalidationsService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, /*create=*/true));
}

// static
WebViewSyncInvalidationsServiceFactory*
WebViewSyncInvalidationsServiceFactory::GetInstance() {
  static base::NoDestructor<WebViewSyncInvalidationsServiceFactory> instance;
  return instance.get();
}

WebViewSyncInvalidationsServiceFactory::WebViewSyncInvalidationsServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "SyncInvalidationsService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(WebViewGCMProfileServiceFactory::GetInstance());
  DependsOn(WebViewInstanceIDProfileServiceFactory::GetInstance());
}

WebViewSyncInvalidationsServiceFactory::
    ~WebViewSyncInvalidationsServiceFactory() = default;

std::unique_ptr<KeyedService>
WebViewSyncInvalidationsServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  WebViewBrowserState* browser_state =
      WebViewBrowserState::FromBrowserState(context);

  gcm::GCMDriver* gcm_driver =
      WebViewGCMProfileServiceFactory::GetForBrowserState(browser_state)
          ->driver();
  instance_id::InstanceIDDriver* instance_id_driver =
      WebViewInstanceIDProfileServiceFactory::GetForBrowserState(browser_state)
          ->driver();
  return std::make_unique<syncer::SyncInvalidationsServiceImpl>(
      gcm_driver, instance_id_driver);
}

}  // namespace ios_web_view
