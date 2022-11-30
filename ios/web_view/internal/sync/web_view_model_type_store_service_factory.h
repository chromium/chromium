// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_MODEL_TYPE_STORE_SERVICE_FACTORY_H_
#define IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_MODEL_TYPE_STORE_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace syncer {
class ModelTypeStoreService;
}  // namespace syncer

namespace ios_web_view {
class WebViewBrowserState;

// Singleton that owns all ModelTypeStoreService and associates them with
// WebViewBrowserState
class WebViewModelTypeStoreServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static syncer::ModelTypeStoreService* GetForBrowserState(
      WebViewBrowserState* browser_state);

  static WebViewModelTypeStoreServiceFactory* GetInstance();

  WebViewModelTypeStoreServiceFactory(
      const WebViewModelTypeStoreServiceFactory&) = delete;
  WebViewModelTypeStoreServiceFactory& operator=(
      const WebViewModelTypeStoreServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<WebViewModelTypeStoreServiceFactory>;

  WebViewModelTypeStoreServiceFactory();
  ~WebViewModelTypeStoreServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_MODEL_TYPE_STORE_SERVICE_FACTORY_H_
