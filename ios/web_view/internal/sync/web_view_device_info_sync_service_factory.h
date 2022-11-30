// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_DEVICE_INFO_SYNC_SERVICE_FACTORY_H_
#define IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_DEVICE_INFO_SYNC_SERVICE_FACTORY_H_

#include <memory>

#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace syncer {
class DeviceInfoSyncService;
}  // namespace syncer

namespace ios_web_view {
class WebViewBrowserState;

// Singleton that owns all DeviceInfoSyncService and associates them with
// WebViewBrowserState
class WebViewDeviceInfoSyncServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static syncer::DeviceInfoSyncService* GetForBrowserState(
      WebViewBrowserState* browser_state);

  static WebViewDeviceInfoSyncServiceFactory* GetInstance();

  WebViewDeviceInfoSyncServiceFactory(
      const WebViewDeviceInfoSyncServiceFactory&) = delete;
  WebViewDeviceInfoSyncServiceFactory& operator=(
      const WebViewDeviceInfoSyncServiceFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<
      WebViewDeviceInfoSyncServiceFactory>;

  WebViewDeviceInfoSyncServiceFactory();
  ~WebViewDeviceInfoSyncServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_DEVICE_INFO_SYNC_SERVICE_FACTORY_H_
