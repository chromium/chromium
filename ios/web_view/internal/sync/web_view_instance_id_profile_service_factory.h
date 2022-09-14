// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_INSTANCE_ID_PROFILE_SERVICE_FACTORY_H_
#define IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_INSTANCE_ID_PROFILE_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace instance_id {
class InstanceIDProfileService;
}

namespace ios_web_view {

class WebViewBrowserState;

// Singleton that owns all InstanceIDProfileService and associates them with
// WebViewBrowserState.
class WebViewInstanceIDProfileServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static instance_id::InstanceIDProfileService* GetForBrowserState(
      WebViewBrowserState* browser_state);

  static WebViewInstanceIDProfileServiceFactory* GetInstance();

  WebViewInstanceIDProfileServiceFactory(
      const WebViewInstanceIDProfileServiceFactory&) = delete;
  WebViewInstanceIDProfileServiceFactory& operator=(
      const WebViewInstanceIDProfileServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<WebViewInstanceIDProfileServiceFactory>;

  WebViewInstanceIDProfileServiceFactory();
  ~WebViewInstanceIDProfileServiceFactory() override;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_INSTANCE_ID_PROFILE_SERVICE_FACTORY_H_
