// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_PROTOCOL_HANDLERS_PROTOCOL_HANDLERS_MANAGER_H_
#define EXTENSIONS_BROWSER_API_PROTOCOL_HANDLERS_PROTOCOL_HANDLERS_MANAGER_H_

#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_registry_observer.h"

namespace extensions {

class ProtocolHandlersManager : public BrowserContextKeyedAPI,
                                public ExtensionRegistryObserver {
 public:
  explicit ProtocolHandlersManager(content::BrowserContext* context);

  ProtocolHandlersManager(const ProtocolHandlersManager&) = delete;
  ProtocolHandlersManager& operator=(const ProtocolHandlersManager&) = delete;

  ~ProtocolHandlersManager() override;

  // BrowserContextKeyedAPI support:
  static BrowserContextKeyedAPIFactory<ProtocolHandlersManager>*
  GetFactoryInstance();
  void Shutdown() override;

 private:
  friend class BrowserContextKeyedAPIFactory<ProtocolHandlersManager>;

  // BrowserContextKeyedAPI support:
  static const char* service_name() { return "ProtocolHandlersManager"; }

  // ExtensionRegistryObserver implementation.
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  // Ensure all the handlers are associated to enabled extensions.
  void ProtocolHandlersSanityCheck();

  const raw_ptr<content::BrowserContext> browser_context_;
  // Use this factory to generate weak pointers bound to the UI thread.
  base::WeakPtrFactory<ProtocolHandlersManager> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_PROTOCOL_HANDLERS_PROTOCOL_HANDLERS_MANAGER_H_
