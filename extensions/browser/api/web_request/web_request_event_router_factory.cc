// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_request/web_request_event_router_factory.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/api/web_request/extension_web_request_event_router.h"
#include "extensions/browser/api/web_request/permission_helper.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/process_map_factory.h"

using content::BrowserContext;

namespace extensions {

// static
WebRequestEventRouter* WebRequestEventRouterFactory::GetForBrowserContext(
    BrowserContext* context) {
  return static_cast<WebRequestEventRouter*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
WebRequestEventRouterFactory* WebRequestEventRouterFactory::GetInstance() {
  return base::Singleton<WebRequestEventRouterFactory>::get();
}

WebRequestEventRouterFactory::WebRequestEventRouterFactory()
    : BrowserContextKeyedServiceFactory(
          "WebRequestEventRouter",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(EventRouterFactory::GetInstance());
  DependsOn(ExtensionRegistryFactory::GetInstance());
  DependsOn(PermissionHelper::GetFactoryInstance());
  DependsOn(ProcessMapFactory::GetInstance());
}

WebRequestEventRouterFactory::~WebRequestEventRouterFactory() = default;

KeyedService* WebRequestEventRouterFactory::BuildServiceInstanceFor(
    BrowserContext* context) const {
  return new WebRequestEventRouter(context);
}

BrowserContext* WebRequestEventRouterFactory::GetBrowserContextToUse(
    BrowserContext* context) const {
  // WebRequestAPI shares an instance between regular and incognito profiles,
  // so this must do the same.
  return ExtensionsBrowserClient::Get()->GetContextRedirectedToOriginal(
      context, /*force_guest_profile=*/true);
}

bool WebRequestEventRouterFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace extensions
