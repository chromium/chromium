// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_CONFIG_MAP_FACTORY_H_
#define EXTENSIONS_BROWSER_EXTENSION_CONFIG_MAP_FACTORY_H_

#include <memory>

#include "base/types/pass_key.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class ExtensionConfigMap;

class ExtensionConfigMapFactory : public BrowserContextKeyedServiceFactory {
 public:
  explicit ExtensionConfigMapFactory(base::PassKey<ExtensionConfigMapFactory>);
  ExtensionConfigMapFactory(const ExtensionConfigMapFactory&) = delete;
  ExtensionConfigMapFactory& operator=(const ExtensionConfigMapFactory&) =
      delete;
  ~ExtensionConfigMapFactory() override;

  // Returns the ExtensionConfigMap associated with `context` if one already
  // exists, or nullptr.
  static ExtensionConfigMap* GetForBrowserContext(
      content::BrowserContext* context);
  // Returns the ExtensionConfigMap associated with `context`, creating it if
  // it does not already exist.
  static ExtensionConfigMap* GetOrCreateForBrowserContext(
      content::BrowserContext* context);
  static ExtensionConfigMapFactory* GetInstance();

 private:
  // BrowserContextKeyedServiceFactory:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_CONFIG_MAP_FACTORY_H_
