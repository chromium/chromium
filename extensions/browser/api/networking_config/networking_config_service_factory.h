// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_NETWORKING_CONFIG_NETWORKING_CONFIG_SERVICE_FACTORY_H_
#define EXTENSIONS_BROWSER_API_NETWORKING_CONFIG_NETWORKING_CONFIG_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {
class NetworkingConfigService;

class NetworkingConfigServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static NetworkingConfigService* GetForBrowserContext(
      content::BrowserContext* context);

  static NetworkingConfigServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<NetworkingConfigServiceFactory>;

  NetworkingConfigServiceFactory();
  ~NetworkingConfigServiceFactory() override;

  // BrowserContextKeyedServiceFactory
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_NETWORKING_CONFIG_NETWORKING_CONFIG_SERVICE_FACTORY_H_
