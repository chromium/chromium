// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_IMAGE_LOADER_FACTORY_H_
#define EXTENSIONS_BROWSER_IMAGE_LOADER_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class ImageLoader;

// Singleton that owns all ImageLoaders and associates them with
// BrowserContexts. Listens for the BrowserContext's destruction notification
// and cleans up the associated ImageLoader. Uses the original BrowserContext
// for incognito contexts.
class ImageLoaderFactory : public BrowserContextKeyedServiceFactory {
 public:
  static ImageLoader* GetForBrowserContext(content::BrowserContext* context);

  static ImageLoaderFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<ImageLoaderFactory>;

  ImageLoaderFactory();
  ~ImageLoaderFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_IMAGE_LOADER_FACTORY_H_
