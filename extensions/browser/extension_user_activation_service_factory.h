// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_USER_ACTIVATION_SERVICE_FACTORY_H_
#define EXTENSIONS_BROWSER_EXTENSION_USER_ACTIVATION_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace extensions {

class ExtensionUserActivationService;

class ExtensionUserActivationServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  ExtensionUserActivationServiceFactory(
      const ExtensionUserActivationServiceFactory&) = delete;
  ExtensionUserActivationServiceFactory& operator=(
      const ExtensionUserActivationServiceFactory&) = delete;

  static ExtensionUserActivationService* GetForBrowserContext(
      content::BrowserContext* context);
  static ExtensionUserActivationServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<ExtensionUserActivationServiceFactory>;

  ExtensionUserActivationServiceFactory();
  ~ExtensionUserActivationServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_USER_ACTIVATION_SERVICE_FACTORY_H_
