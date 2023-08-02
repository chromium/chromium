// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/sync/web_view_instance_id_profile_service_factory.h"

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/gcm_driver/instance_id/instance_id_profile_service.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/web_view/internal/sync/web_view_gcm_profile_service_factory.h"
#include "ios/web_view/internal/web_view_browser_state.h"

namespace ios_web_view {

// static
instance_id::InstanceIDProfileService*
WebViewInstanceIDProfileServiceFactory::GetForBrowserState(
    WebViewBrowserState* browser_state) {
  return static_cast<instance_id::InstanceIDProfileService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
WebViewInstanceIDProfileServiceFactory*
WebViewInstanceIDProfileServiceFactory::GetInstance() {
  static base::NoDestructor<WebViewInstanceIDProfileServiceFactory> instance;
  return instance.get();
}

WebViewInstanceIDProfileServiceFactory::WebViewInstanceIDProfileServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "InstanceIDProfileService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(WebViewGCMProfileServiceFactory::GetInstance());
}

WebViewInstanceIDProfileServiceFactory::
    ~WebViewInstanceIDProfileServiceFactory() {}

std::unique_ptr<KeyedService>
WebViewInstanceIDProfileServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  DCHECK(!context->IsOffTheRecord());

  WebViewBrowserState* browser_state =
      WebViewBrowserState::FromBrowserState(context);
  return std::make_unique<instance_id::InstanceIDProfileService>(
      WebViewGCMProfileServiceFactory::GetForBrowserState(browser_state)
          ->driver(),
      browser_state->IsOffTheRecord());
}

}  // namespace ios_web_view
