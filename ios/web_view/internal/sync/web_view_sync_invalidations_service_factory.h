// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_SYNC_INVALIDATIONS_SERVICE_FACTORY_H_
#define IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_SYNC_INVALIDATIONS_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace syncer {
class SyncInvalidationsService;
}  // namespace syncer

namespace ios_web_view {

class WebViewBrowserState;

class WebViewSyncInvalidationsServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  WebViewSyncInvalidationsServiceFactory(
      const WebViewSyncInvalidationsServiceFactory&) = delete;
  WebViewSyncInvalidationsServiceFactory& operator=(
      const WebViewSyncInvalidationsServiceFactory&) = delete;

  // Returned value may be nullptr in case if sync invalidations are disabled or
  // not supported.
  static syncer::SyncInvalidationsService* GetForBrowserState(
      WebViewBrowserState* browser_state);

  static WebViewSyncInvalidationsServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<WebViewSyncInvalidationsServiceFactory>;

  WebViewSyncInvalidationsServiceFactory();
  ~WebViewSyncInvalidationsServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_SYNC_INVALIDATIONS_SERVICE_FACTORY_H_
