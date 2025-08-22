// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/isolated_web_apps/reading/response_reader_registry_factory.h"

#include <memory>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_verifier.h"
#include "components/webapps/isolated_web_apps/client.h"
#include "components/webapps/isolated_web_apps/reading/response_reader_factory.h"
#include "components/webapps/isolated_web_apps/reading/response_reader_registry.h"
#include "components/webapps/isolated_web_apps/reading/validator.h"
#include "content/public/browser/isolated_web_apps_policy.h"

namespace web_app {

// static
IsolatedWebAppReaderRegistry* IsolatedWebAppReaderRegistryFactory::Get(
    content::BrowserContext* context) {
  return static_cast<IsolatedWebAppReaderRegistry*>(
      IsolatedWebAppReaderRegistryFactory::GetInstance()
          ->GetServiceForBrowserContext(context, /*create=*/true));
}

// static
IsolatedWebAppReaderRegistryFactory*
IsolatedWebAppReaderRegistryFactory::GetInstance() {
  static base::NoDestructor<IsolatedWebAppReaderRegistryFactory> instance;
  return instance.get();
}

IsolatedWebAppReaderRegistryFactory::IsolatedWebAppReaderRegistryFactory()
    : IsolatedWebAppBrowserContextServiceFactory(
          "IsolatedWebAppReaderRegistry") {}

IsolatedWebAppReaderRegistryFactory::~IsolatedWebAppReaderRegistryFactory() =
    default;

std::unique_ptr<KeyedService>
IsolatedWebAppReaderRegistryFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  CHECK(content::AreIsolatedWebAppsEnabled(context));
  auto reader_factory =
      std::make_unique<IsolatedWebAppResponseReaderFactory>(context);
  return std::make_unique<IsolatedWebAppReaderRegistry>(
      context, std::move(reader_factory));
}

}  // namespace web_app
