// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/networking_private/networking_private_event_router_factory.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/api/networking_private/networking_private_delegate_factory.h"
#include "extensions/browser/api/networking_private/networking_private_event_router.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"

namespace extensions {

// static
NetworkingPrivateEventRouter*
NetworkingPrivateEventRouterFactory::GetForProfile(
    content::BrowserContext* context) {
  return static_cast<NetworkingPrivateEventRouter*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
NetworkingPrivateEventRouterFactory*
NetworkingPrivateEventRouterFactory::GetInstance() {
  return base::Singleton<NetworkingPrivateEventRouterFactory>::get();
}

NetworkingPrivateEventRouterFactory::NetworkingPrivateEventRouterFactory()
    : BrowserContextKeyedServiceFactory(
          "NetworkingPrivateEventRouter",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  DependsOn(NetworkingPrivateDelegateFactory::GetInstance());
}

std::unique_ptr<KeyedService>
NetworkingPrivateEventRouterFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return NetworkingPrivateEventRouter::Create(context);
}

content::BrowserContext*
NetworkingPrivateEventRouterFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return ExtensionsBrowserClient::Get()->GetContextRedirectedToOriginal(
      context, /*force_guest_profile=*/true);
}

bool NetworkingPrivateEventRouterFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

bool NetworkingPrivateEventRouterFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace extensions
