// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_SERVICE_ISOLATED_WEB_APP_BROWSER_CONTEXT_SERVICE_FACTORY_H_
#define COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_SERVICE_ISOLATED_WEB_APP_BROWSER_CONTEXT_SERVICE_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace web_app {

// This factory acts as a base class for keyed services that should only exist
// for browser contexts that support isolated web apps. If a browser context
// does not support isolated web apps, it will not create the keyed service.
class IsolatedWebAppBrowserContextServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  IsolatedWebAppBrowserContextServiceFactory(
      const IsolatedWebAppBrowserContextServiceFactory&) = delete;
  IsolatedWebAppBrowserContextServiceFactory& operator=(
      const IsolatedWebAppBrowserContextServiceFactory&) = delete;

 protected:
  explicit IsolatedWebAppBrowserContextServiceFactory(const char* name);
  ~IsolatedWebAppBrowserContextServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  // Selects the given context to proper context to use based whether it
  // supports isolated web apps.
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const final;
};

}  // namespace web_app

#endif  // COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_SERVICE_ISOLATED_WEB_APP_BROWSER_CONTEXT_SERVICE_FACTORY_H_
