// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_AUTOFILL_WEB_VIEW_STRIKE_DATABASE_FACTORY_H_
#define IOS_WEB_VIEW_INTERNAL_AUTOFILL_WEB_VIEW_STRIKE_DATABASE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace autofill {
class StrikeDatabase;
}

namespace ios_web_view {
class WebViewBrowserState;

// Singleton that owns all StrikeDatabases and associates them with
// ios_web_view::WebViewBrowserState.
class WebViewStrikeDatabaseFactory : public BrowserStateKeyedServiceFactory {
 public:
  static autofill::StrikeDatabase* GetForBrowserState(
      WebViewBrowserState* browser_state);
  static WebViewStrikeDatabaseFactory* GetInstance();

  WebViewStrikeDatabaseFactory(const WebViewStrikeDatabaseFactory&) = delete;
  WebViewStrikeDatabaseFactory& operator=(const WebViewStrikeDatabaseFactory&) =
      delete;

 private:
  friend class base::NoDestructor<WebViewStrikeDatabaseFactory>;

  WebViewStrikeDatabaseFactory();
  ~WebViewStrikeDatabaseFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_AUTOFILL_WEB_VIEW_STRIKE_DATABASE_FACTORY_H_
