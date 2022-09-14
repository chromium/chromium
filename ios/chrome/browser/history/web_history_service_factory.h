// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HISTORY_WEB_HISTORY_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_HISTORY_WEB_HISTORY_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;

namespace history {
class WebHistoryService;
}

namespace ios {
// Singleton that owns all WebHistoryServices and associates them with
// ChromeBrowserState.
class WebHistoryServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static history::WebHistoryService* GetForBrowserState(
      ChromeBrowserState* browser_state);
  static WebHistoryServiceFactory* GetInstance();

  WebHistoryServiceFactory(const WebHistoryServiceFactory&) = delete;
  WebHistoryServiceFactory& operator=(const WebHistoryServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<WebHistoryServiceFactory>;

  WebHistoryServiceFactory();
  ~WebHistoryServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_HISTORY_WEB_HISTORY_SERVICE_FACTORY_H_
