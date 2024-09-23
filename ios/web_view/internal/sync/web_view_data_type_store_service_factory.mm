// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/sync/web_view_data_type_store_service_factory.h"

#import <utility>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/sync/model/data_type_store_service_impl.h"
#import "ios/web_view/internal/web_view_browser_state.h"

namespace ios_web_view {

// static
WebViewDataTypeStoreServiceFactory*
WebViewDataTypeStoreServiceFactory::GetInstance() {
  static base::NoDestructor<WebViewDataTypeStoreServiceFactory> instance;
  return instance.get();
}

// static
syncer::DataTypeStoreService*
WebViewDataTypeStoreServiceFactory::GetForBrowserState(
    WebViewBrowserState* browser_state) {
  return static_cast<syncer::DataTypeStoreService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

WebViewDataTypeStoreServiceFactory::WebViewDataTypeStoreServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "DataTypeStoreService",
          BrowserStateDependencyManager::GetInstance()) {}

WebViewDataTypeStoreServiceFactory::~WebViewDataTypeStoreServiceFactory() {}

std::unique_ptr<KeyedService>
WebViewDataTypeStoreServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  WebViewBrowserState* browser_state =
      WebViewBrowserState::FromBrowserState(context);
  return std::make_unique<syncer::DataTypeStoreServiceImpl>(
      browser_state->GetStatePath(), browser_state->GetPrefs());
}

web::BrowserState* WebViewDataTypeStoreServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  WebViewBrowserState* browser_state =
      WebViewBrowserState::FromBrowserState(context);
  return browser_state->GetRecordingBrowserState();
}

}  // namespace ios_web_view
