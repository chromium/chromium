// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_GCM_PROFILE_SERVICE_FACTORY_H_
#define IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_GCM_PROFILE_SERVICE_FACTORY_H_

#include <memory>
#include <string>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace gcm {
class GCMProfileService;
}

namespace ios_web_view {

class WebViewBrowserState;

// Singleton that owns all GCMProfileService and associates them with
// WebViewBrowserState.
class WebViewGCMProfileServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static gcm::GCMProfileService* GetForBrowserState(
      WebViewBrowserState* browser_state);

  static WebViewGCMProfileServiceFactory* GetInstance();

  WebViewGCMProfileServiceFactory(const WebViewGCMProfileServiceFactory&) =
      delete;
  WebViewGCMProfileServiceFactory& operator=(
      const WebViewGCMProfileServiceFactory&) = delete;

  // Returns a string like "org.chromium.chromewebview" that should be used as
  // the GCM category when an app_id is sent as a subtype instead of as a
  // category. This string must never change during the lifetime of an install,
  // since e.g. to unregister an Instance ID token the same category must be
  // passed to GCM as was originally passed when registering it.
  static std::string GetProductCategoryForSubtypes();

 private:
  friend class base::NoDestructor<WebViewGCMProfileServiceFactory>;

  WebViewGCMProfileServiceFactory();
  ~WebViewGCMProfileServiceFactory() override;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_GCM_PROFILE_SERVICE_FACTORY_H_
