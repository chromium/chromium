// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_SYNC_SERVICE_FACTORY_H_
#define IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_SYNC_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace syncer {
class SyncService;
}  // namespace syncer

namespace ios_web_view {
class WebViewBrowserState;

// Singleton that owns all SyncService and associates them with
// WebViewBrowserState.
class WebViewSyncServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static syncer::SyncService* GetForBrowserState(
      WebViewBrowserState* browser_state);

  static WebViewSyncServiceFactory* GetInstance();

  WebViewSyncServiceFactory(const WebViewSyncServiceFactory&) = delete;
  WebViewSyncServiceFactory& operator=(const WebViewSyncServiceFactory&) =
      delete;

 private:
  friend class base::NoDestructor<WebViewSyncServiceFactory>;

  WebViewSyncServiceFactory();
  ~WebViewSyncServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_SYNC_SERVICE_FACTORY_H_
