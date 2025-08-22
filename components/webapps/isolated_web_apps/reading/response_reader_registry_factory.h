// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_READING_RESPONSE_READER_REGISTRY_FACTORY_H_
#define COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_READING_RESPONSE_READER_REGISTRY_FACTORY_H_

#include "base/no_destructor.h"
#include "components/webapps/isolated_web_apps/service/isolated_web_app_browser_context_service_factory.h"

namespace web_app {

class IsolatedWebAppReaderRegistry;

// Singleton that owns all `IsolatedWebAppReaderRegistry`s and associates them
// with a `Profile`.
//
// We don't actually use the `Profile` inside
// `IsolatedWebAppReaderRegistry`. Having a separate
// `IsolatedWebAppReaderRegistry` per `Profile` is purely a security measure,
// which makes sure that the integrity of an Isolated Web App is verified on a
// per-profile basis.
class IsolatedWebAppReaderRegistryFactory
    : public IsolatedWebAppBrowserContextServiceFactory {
 public:
  IsolatedWebAppReaderRegistryFactory(
      const IsolatedWebAppReaderRegistryFactory&) = delete;
  IsolatedWebAppReaderRegistryFactory& operator=(
      const IsolatedWebAppReaderRegistryFactory&) = delete;

  static IsolatedWebAppReaderRegistryFactory* GetInstance();

  static IsolatedWebAppReaderRegistry* Get(content::BrowserContext*);

 private:
  friend base::NoDestructor<IsolatedWebAppReaderRegistryFactory>;

  IsolatedWebAppReaderRegistryFactory();
  ~IsolatedWebAppReaderRegistryFactory() override;

  // BrowserContextKeyedServiceFactory
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace web_app

#endif  // COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_READING_RESPONSE_READER_REGISTRY_FACTORY_H_
