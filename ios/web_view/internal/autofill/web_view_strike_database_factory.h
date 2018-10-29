// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_AUTOFILL_WEB_VIEW_STRIKE_DATABASE_FACTORY_H_
#define IOS_WEB_VIEW_INTERNAL_AUTOFILL_WEB_VIEW_STRIKE_DATABASE_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

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

 private:
  friend struct base::DefaultSingletonTraits<WebViewStrikeDatabaseFactory>;

  WebViewStrikeDatabaseFactory();
  ~WebViewStrikeDatabaseFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;

  DISALLOW_COPY_AND_ASSIGN(WebViewStrikeDatabaseFactory);
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_AUTOFILL_WEB_VIEW_STRIKE_DATABASE_FACTORY_H_
