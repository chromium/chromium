// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_EXTENSION_SYSTEM_FACTORY_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_EXTENSION_SYSTEM_FACTORY_H_

#include "base/memory/singleton.h"
#include "extensions/browser/extension_system_provider.h"

namespace extensions {

// A factory that provides ShellExtensionSystem for app_shell.
class ShellExtensionSystemFactory : public ExtensionSystemProvider {
 public:
  ShellExtensionSystemFactory(const ShellExtensionSystemFactory&) = delete;
  ShellExtensionSystemFactory& operator=(const ShellExtensionSystemFactory&) =
      delete;

  // ExtensionSystemProvider implementation:
  ExtensionSystem* GetForBrowserContext(
      content::BrowserContext* context) override;

  static ShellExtensionSystemFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<ShellExtensionSystemFactory>;

  ShellExtensionSystemFactory();
  ~ShellExtensionSystemFactory() override;

  // BrowserContextKeyedServiceFactory implementation:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_EXTENSION_SYSTEM_FACTORY_H_
