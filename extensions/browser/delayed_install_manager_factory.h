// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_DELAYED_INSTALL_MANAGER_FACTORY_H_
#define EXTENSIONS_BROWSER_DELAYED_INSTALL_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class DelayedInstallManager;

// Factory for DelayedInstallManager objects. DelayedInstallManager objects
// are shared between an incognito browser context and its original browser
// context.
class DelayedInstallManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  DelayedInstallManagerFactory(const DelayedInstallManagerFactory&) = delete;
  DelayedInstallManagerFactory& operator=(const DelayedInstallManagerFactory&) =
      delete;

  static DelayedInstallManager* GetForBrowserContext(
      content::BrowserContext* context);

  static DelayedInstallManagerFactory* GetInstance();

 private:
  friend base::NoDestructor<DelayedInstallManagerFactory>;

  DelayedInstallManagerFactory();
  ~DelayedInstallManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_DELAYED_INSTALL_MANAGER_FACTORY_H_
