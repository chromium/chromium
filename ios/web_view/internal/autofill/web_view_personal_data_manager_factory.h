// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_AUTOFILL_WEB_VIEW_PERSONAL_DATA_MANAGER_FACTORY_H_
#define IOS_WEB_VIEW_INTERNAL_AUTOFILL_WEB_VIEW_PERSONAL_DATA_MANAGER_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace autofill {
class PersonalDataManager;
}

namespace ios_web_view {
class WebViewBrowserState;

// Singleton that owns all PersonalDataManagers and associates them with
// ios_web_view::WebViewBrowserState.
class WebViewPersonalDataManagerFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static autofill::PersonalDataManager* GetForBrowserState(
      WebViewBrowserState* browser_state);
  static WebViewPersonalDataManagerFactory* GetInstance();

  WebViewPersonalDataManagerFactory(const WebViewPersonalDataManagerFactory&) =
      delete;
  WebViewPersonalDataManagerFactory& operator=(
      const WebViewPersonalDataManagerFactory&) = delete;

 private:
  friend class base::NoDestructor<WebViewPersonalDataManagerFactory>;

  WebViewPersonalDataManagerFactory();
  ~WebViewPersonalDataManagerFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_AUTOFILL_WEB_VIEW_PERSONAL_DATA_MANAGER_FACTORY_H_
