// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_PREF_VALUE_MAP_FACTORY_H_
#define EXTENSIONS_BROWSER_EXTENSION_PREF_VALUE_MAP_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class ExtensionPrefValueMap;

// The usual factory boilerplate for ExtensionPrefValueMap.
class ExtensionPrefValueMapFactory : public BrowserContextKeyedServiceFactory {
 public:
  static ExtensionPrefValueMap* GetForBrowserContext(
      content::BrowserContext* context);

  static ExtensionPrefValueMapFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<ExtensionPrefValueMapFactory>;

  ExtensionPrefValueMapFactory();
  ~ExtensionPrefValueMapFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

#endif  // EXTENSIONS_BROWSER_EXTENSION_PREF_VALUE_MAP_FACTORY_H_
