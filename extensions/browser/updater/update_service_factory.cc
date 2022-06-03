// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/updater/update_service_factory.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/update_client/update_client.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/process_manager_factory.h"
#include "extensions/browser/updater/update_service.h"

namespace extensions {

// static
UpdateService* UpdateServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<UpdateService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
UpdateServiceFactory* UpdateServiceFactory::GetInstance() {
  return base::Singleton<UpdateServiceFactory>::get();
}

UpdateServiceFactory::UpdateServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "UpdateService",
          BrowserContextDependencyManager::GetInstance()) {}

UpdateServiceFactory::~UpdateServiceFactory() = default;

KeyedService* UpdateServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new UpdateService(
      context, ExtensionsBrowserClient::Get()->CreateUpdateClient(context));
}

}  // namespace extensions
