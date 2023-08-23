// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_PREFS_FACTORY_H_
#define EXTENSIONS_BROWSER_EXTENSION_PREFS_FACTORY_H_

#include <memory>

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace extensions {

class ExtensionPrefs;

class ExtensionPrefsFactory : public BrowserContextKeyedServiceFactory {
 public:
  static ExtensionPrefs* GetForBrowserContext(content::BrowserContext* context);

  static ExtensionPrefsFactory* GetInstance();

  void SetInstanceForTesting(content::BrowserContext* context,
                             std::unique_ptr<ExtensionPrefs> prefs);

 private:
  friend struct base::DefaultSingletonTraits<ExtensionPrefsFactory>;

  ExtensionPrefsFactory();
  ~ExtensionPrefsFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_PREFS_FACTORY_H_
