// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/vpn_provider/vpn_service_factory.h"

#include "base/memory/singleton.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/network/network_handler.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/api/vpn_provider/vpn_service.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_browser_client.h"

namespace chromeos {

// static
VpnService* VpnServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<VpnService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
VpnServiceFactory* VpnServiceFactory::GetInstance() {
  return base::Singleton<VpnServiceFactory>::get();
}

VpnServiceFactory::VpnServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "VpnService",
          BrowserContextDependencyManager::GetInstance()) {}

VpnServiceFactory::~VpnServiceFactory() = default;

bool VpnServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool VpnServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

KeyedService* VpnServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  std::string context_user_hash =
      extensions::ExtensionsBrowserClient::Get()->GetUserIdHashFromContext(
          context);

  if (!LoginState::IsInitialized() ||
      context_user_hash != LoginState::Get()->primary_user_hash()) {
    return nullptr;
  }

  return new VpnService(
      context, context_user_hash, extensions::ExtensionRegistry::Get(context),
      extensions::EventRouter::Get(context),
      DBusThreadManager::Get()->GetShillThirdPartyVpnDriverClient(),
      NetworkHandler::Get()->network_configuration_handler(),
      NetworkHandler::Get()->network_profile_handler(),
      NetworkHandler::Get()->network_state_handler());
}

}  // namespace chromeos
