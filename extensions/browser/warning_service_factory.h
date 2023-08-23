// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_WARNING_SERVICE_FACTORY_H_
#define EXTENSIONS_BROWSER_WARNING_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace extensions {

class WarningService;

class WarningServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  WarningServiceFactory(const WarningServiceFactory&) = delete;
  WarningServiceFactory& operator=(const WarningServiceFactory&) = delete;

  static WarningService* GetForBrowserContext(content::BrowserContext* context);
  static WarningServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<WarningServiceFactory>;

  WarningServiceFactory();
  ~WarningServiceFactory() override;

  // BrowserContextKeyedServiceFactory implementation
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_WARNING_SERVICE_FACTORY_H_
