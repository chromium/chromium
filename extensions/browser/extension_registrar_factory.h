// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_REGISTRAR_FACTORY_H_
#define EXTENSIONS_BROWSER_EXTENSION_REGISTRAR_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace extensions {

class ExtensionRegistrar;

// Factory for ExtensionRegistrar objects. ExtensionRegistrar objects are shared
// between an incognito browser context and its original browser context.
class ExtensionRegistrarFactory : public BrowserContextKeyedServiceFactory {
 public:
  ExtensionRegistrarFactory(const ExtensionRegistrarFactory&) = delete;
  ExtensionRegistrarFactory& operator=(const ExtensionRegistrarFactory&) =
      delete;

  static ExtensionRegistrar* GetForBrowserContext(
      content::BrowserContext* context);

  static ExtensionRegistrarFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<ExtensionRegistrarFactory>;

  ExtensionRegistrarFactory();
  ~ExtensionRegistrarFactory() override;

  // BrowserContextKeyedServiceFactory implementation:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_REGISTRAR_FACTORY_H_
