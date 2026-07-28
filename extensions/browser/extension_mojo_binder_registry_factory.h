// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_MOJO_BINDER_REGISTRY_FACTORY_H_
#define EXTENSIONS_BROWSER_EXTENSION_MOJO_BINDER_REGISTRY_FACTORY_H_

#include <memory>

#include "base/types/pass_key.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class ExtensionMojoBinderRegistry;

class ExtensionMojoBinderRegistryFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  explicit ExtensionMojoBinderRegistryFactory(
      base::PassKey<ExtensionMojoBinderRegistryFactory>);
  ExtensionMojoBinderRegistryFactory(
      const ExtensionMojoBinderRegistryFactory&) = delete;
  ExtensionMojoBinderRegistryFactory& operator=(
      const ExtensionMojoBinderRegistryFactory&) = delete;
  ~ExtensionMojoBinderRegistryFactory() override;

  static ExtensionMojoBinderRegistry* GetForBrowserContext(
      content::BrowserContext* context);
  static ExtensionMojoBinderRegistryFactory* GetInstance();

 private:
  // BrowserContextKeyedServiceFactory:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_MOJO_BINDER_REGISTRY_FACTORY_H_
