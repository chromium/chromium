// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/sync/web_view_model_type_store_service_factory.h"

#include <utility>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/sync/model/model_type_store_service_impl.h"
#include "ios/web_view/internal/web_view_browser_state.h"

namespace ios_web_view {

// static
WebViewModelTypeStoreServiceFactory*
WebViewModelTypeStoreServiceFactory::GetInstance() {
  static base::NoDestructor<WebViewModelTypeStoreServiceFactory> instance;
  return instance.get();
}

// static
syncer::ModelTypeStoreService*
WebViewModelTypeStoreServiceFactory::GetForBrowserState(
    WebViewBrowserState* browser_state) {
  return static_cast<syncer::ModelTypeStoreService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

WebViewModelTypeStoreServiceFactory::WebViewModelTypeStoreServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "ModelTypeStoreService",
          BrowserStateDependencyManager::GetInstance()) {}

WebViewModelTypeStoreServiceFactory::~WebViewModelTypeStoreServiceFactory() {}

std::unique_ptr<KeyedService>
WebViewModelTypeStoreServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  WebViewBrowserState* browser_state =
      WebViewBrowserState::FromBrowserState(context);
  return std::make_unique<syncer::ModelTypeStoreServiceImpl>(
      browser_state->GetStatePath());
}

web::BrowserState* WebViewModelTypeStoreServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  WebViewBrowserState* browser_state =
      WebViewBrowserState::FromBrowserState(context);
  return browser_state->GetRecordingBrowserState();
}

}  // namespace ios_web_view
