// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/image_loader_factory.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/image_loader.h"

namespace extensions {

// static
ImageLoader* ImageLoaderFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ImageLoader*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

ImageLoaderFactory* ImageLoaderFactory::GetInstance() {
  return base::Singleton<ImageLoaderFactory>::get();
}

ImageLoaderFactory::ImageLoaderFactory()
    : BrowserContextKeyedServiceFactory(
        "ImageLoader",
        BrowserContextDependencyManager::GetInstance()) {
}

ImageLoaderFactory::~ImageLoaderFactory() {
}

KeyedService* ImageLoaderFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new ImageLoader;
}

content::BrowserContext* ImageLoaderFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return ExtensionsBrowserClient::Get()->GetOriginalContext(context);
}

}  // namespace extensions
