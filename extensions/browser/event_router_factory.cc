// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/event_router_factory.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extensions_browser_client.h"

using content::BrowserContext;

namespace extensions {

// static
EventRouter* EventRouterFactory::GetForBrowserContext(BrowserContext* context) {
  return static_cast<EventRouter*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
EventRouterFactory* EventRouterFactory::GetInstance() {
  return base::Singleton<EventRouterFactory>::get();
}

EventRouterFactory::EventRouterFactory()
    : BrowserContextKeyedServiceFactory(
          "EventRouter",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ExtensionRegistryFactory::GetInstance());
  DependsOn(ExtensionPrefsFactory::GetInstance());
}

EventRouterFactory::~EventRouterFactory() {
}

std::unique_ptr<KeyedService>
EventRouterFactory::BuildServiceInstanceForBrowserContext(
    BrowserContext* context) const {
  return std::make_unique<EventRouter>(context, ExtensionPrefs::Get(context));
}

BrowserContext* EventRouterFactory::GetBrowserContextToUse(
    BrowserContext* context) const {
  // Redirected in incognito.
  return ExtensionsBrowserClient::Get()->GetContextRedirectedToOriginal(
      context, /*force_guest_profile=*/true);
}

bool EventRouterFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace extensions
