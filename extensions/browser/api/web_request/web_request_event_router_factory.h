// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_EVENT_ROUTER_FACTORY_H_
#define EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_EVENT_ROUTER_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {

class WebRequestEventRouter;

class WebRequestEventRouterFactory : public BrowserContextKeyedServiceFactory {
 public:
  WebRequestEventRouterFactory(const WebRequestEventRouterFactory&) = delete;
  WebRequestEventRouterFactory& operator=(const WebRequestEventRouterFactory&) =
      delete;

  static WebRequestEventRouter* GetForBrowserContext(
      content::BrowserContext* context);
  static WebRequestEventRouterFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<WebRequestEventRouterFactory>;

  WebRequestEventRouterFactory();
  ~WebRequestEventRouterFactory() override;

  // BrowserContextKeyedServiceFactory implementation
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_EVENT_ROUTER_FACTORY_H_
