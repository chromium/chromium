// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/isolated_web_apps/service/isolated_web_app_browser_context_service_factory.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/isolated_web_apps_policy.h"

namespace web_app {

IsolatedWebAppBrowserContextServiceFactory::
    IsolatedWebAppBrowserContextServiceFactory(const char* name)
    : BrowserContextKeyedServiceFactory(
          name,
          BrowserContextDependencyManager::GetInstance()) {}

IsolatedWebAppBrowserContextServiceFactory::
    ~IsolatedWebAppBrowserContextServiceFactory() = default;

content::BrowserContext*
IsolatedWebAppBrowserContextServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return content::AreIsolatedWebAppsEnabled(context) ? context : nullptr;
}

}  // namespace web_app
