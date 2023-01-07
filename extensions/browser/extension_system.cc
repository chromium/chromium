// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_system.h"

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"

namespace extensions {

ExtensionSystem::ExtensionSystem() {
}

ExtensionSystem::~ExtensionSystem() {
}

// static
ExtensionSystem* ExtensionSystem::Get(content::BrowserContext* context) {
  return ExtensionsBrowserClient::Get()
      ->GetExtensionSystemFactory()
      ->GetForBrowserContext(context);
}

}  // namespace extensions
