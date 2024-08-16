// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_DATA_TYPE_STORE_SERVICE_FACTORY_H_
#define IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_DATA_TYPE_STORE_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace syncer {
class DataTypeStoreService;
}  // namespace syncer

namespace ios_web_view {
class WebViewBrowserState;

// Singleton that owns all DataTypeStoreService and associates them with
// WebViewBrowserState
class WebViewDataTypeStoreServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static syncer::DataTypeStoreService* GetForBrowserState(
      WebViewBrowserState* browser_state);

  static WebViewDataTypeStoreServiceFactory* GetInstance();

  WebViewDataTypeStoreServiceFactory(
      const WebViewDataTypeStoreServiceFactory&) = delete;
  WebViewDataTypeStoreServiceFactory& operator=(
      const WebViewDataTypeStoreServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<WebViewDataTypeStoreServiceFactory>;

  WebViewDataTypeStoreServiceFactory();
  ~WebViewDataTypeStoreServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_DATA_TYPE_STORE_SERVICE_FACTORY_H_
