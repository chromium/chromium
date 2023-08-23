// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_PREFS_HELPER_FACTORY_H_
#define EXTENSIONS_BROWSER_EXTENSION_PREFS_HELPER_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace extensions {

class ExtensionPrefsHelper;

class ExtensionPrefsHelperFactory : public BrowserContextKeyedServiceFactory {
 public:
  static ExtensionPrefsHelper* GetForBrowserContext(
      content::BrowserContext* context);

  static ExtensionPrefsHelperFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<ExtensionPrefsHelperFactory>;

  ExtensionPrefsHelperFactory();
  ~ExtensionPrefsHelperFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_PREFS_HELPER_FACTORY_H_
