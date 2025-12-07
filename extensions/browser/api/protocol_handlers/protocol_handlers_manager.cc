// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/protocol_handlers/protocol_handlers_manager.h"

#include "base/check_is_test.h"
#include "base/lazy_instance.h"
#include "base/one_shot_event.h"
#include "components/custom_handlers/protocol_handler.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/manifest_handlers/protocol_handler_info.h"
#include "third_party/blink/public/common/security/protocol_handler_security_level.h"

using content::BrowserThread;

namespace extensions {

namespace {

static void RegisterHandlersIfNeeded(
    const ExtensionId& id,
    const ProtocolHandlersInfo& info,
    custom_handlers::ProtocolHandlerRegistry& registry) {
  for (const auto& handler_info : info) {
    custom_handlers::ProtocolHandler handler =
        custom_handlers::ProtocolHandler::CreateExtensionProtocolHandler(
            handler_info.protocol, handler_info.url, id);

    DCHECK(handler.IsValid());

    // This function is called whenever the extension is loaded (including
    // every startup), so the handler is most likely already registered. In
    // that case, the registration will be (safely) silently ignored.
    if (registry.SilentlyHandleRegisterHandlerRequest(handler)) {
      continue;
    }

    // TODO(crbug.com/40482153#comment71): As defined in the design document,
    // the runtime permissions approach for extensions must be followed here.
    // Hence, we need a new "registration status" that mark the custom handler
    // as "pending" and ask the user only when used.
    registry.OnAcceptRegisterProtocolHandler(handler);
  }
}

static void UnregisterHandlersIfNeeded(
    const ExtensionId& id,
    const ProtocolHandlersInfo& info,
    custom_handlers::ProtocolHandlerRegistry& registry) {
  for (const auto& handler_info : info) {
    custom_handlers::ProtocolHandler handler =
        custom_handlers::ProtocolHandler::CreateExtensionProtocolHandler(
            handler_info.protocol, handler_info.url, id);

    DCHECK(handler.IsValid());

    registry.RemoveHandler(handler);
  }
}

}  // namespace

ProtocolHandlersManager::ProtocolHandlersManager(
    content::BrowserContext* context)
    : browser_context_(context) {
  DCHECK(browser_context_);
  ExtensionRegistry::Get(browser_context_)->AddObserver(this);
  ExtensionSystem::Get(browser_context_)
      ->ready()
      .Post(
          FROM_HERE,
          base::BindOnce(&ProtocolHandlersManager::ProtocolHandlersSanityCheck,
                         weak_ptr_factory_.GetWeakPtr()));
}

ProtocolHandlersManager::~ProtocolHandlersManager() = default;

void ProtocolHandlersManager::Shutdown() {
  ExtensionRegistry::Get(browser_context_)->RemoveObserver(this);
}

// static
BrowserContextKeyedAPIFactory<ProtocolHandlersManager>*
ProtocolHandlersManager::GetFactoryInstance() {
  static base::NoDestructor<
      BrowserContextKeyedAPIFactory<ProtocolHandlersManager>>
      instance;
  return instance.get();
}

void ProtocolHandlersManager::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  const ProtocolHandlersInfo* info =
      ProtocolHandlers::GetProtocolHandlers(*extension);
  if (!info) {
    return;
  }

  auto* registry = ExtensionsBrowserClient::Get()->GetProtocolHandlerRegistry(
      browser_context);
  // Can be null for tests using dummy profiles.
  if (!registry) {
    CHECK_IS_TEST();
    return;
  }

  RegisterHandlersIfNeeded(extension->id(), *info, *registry);
}

void ProtocolHandlersManager::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  const ProtocolHandlersInfo* info =
      ProtocolHandlers::GetProtocolHandlers(*extension);
  if (!info) {
    return;
  }

  auto* registry = ExtensionsBrowserClient::Get()->GetProtocolHandlerRegistry(
      browser_context);
  // Can be null for tests using dummy profiles.
  if (!registry) {
    CHECK_IS_TEST();
    return;
  }

  UnregisterHandlersIfNeeded(extension->id(), *info, *registry);
}

void ProtocolHandlersManager::ProtocolHandlersSanityCheck() {
  const auto* ext_registry = ExtensionRegistry::Get(browser_context_);
  ExtensionIdSet enabled_ids = ext_registry->enabled_extensions().GetIDs();
  auto* ph_registry =
      ExtensionsBrowserClient::Get()->GetProtocolHandlerRegistry(
          browser_context_);
  // Can be null for tests using dummy profiles.
  if (!ph_registry) {
    CHECK_IS_TEST();
    return;
  }
  for (const auto& handler : ph_registry->GetExtensionProtocolHandlers()) {
    DCHECK(handler.extension_id());
    if (!enabled_ids.contains(*handler.extension_id())) {
      ph_registry->RemoveHandler(handler);
    }
  }
}

}  // namespace extensions
