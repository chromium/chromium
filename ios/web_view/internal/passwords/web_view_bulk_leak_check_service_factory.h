// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_PASSWORDS_WEB_VIEW_BULK_LEAK_CHECK_SERVICE_FACTORY_H_
#define IOS_WEB_VIEW_INTERNAL_PASSWORDS_WEB_VIEW_BULK_LEAK_CHECK_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace password_manager {
class BulkLeakCheckServiceInterface;
}

namespace ios_web_view {

class WebViewBrowserState;

// Singleton that owns all BulkLeakCheckServices and associates them with
// WebViewBrowserState.
class WebViewBulkLeakCheckServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static WebViewBulkLeakCheckServiceFactory* GetInstance();
  static password_manager::BulkLeakCheckServiceInterface* GetForBrowserState(
      WebViewBrowserState* browser_state);

 private:
  friend class base::NoDestructor<WebViewBulkLeakCheckServiceFactory>;

  WebViewBulkLeakCheckServiceFactory();
  ~WebViewBulkLeakCheckServiceFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_PASSWORDS_WEB_VIEW_BULK_LEAK_CHECK_SERVICE_FACTORY_H_
