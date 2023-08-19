// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/printer_provider/printer_provider_api_factory.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/api/printer_provider/printer_provider_api.h"
#include "extensions/browser/api/printer_provider/printer_provider_internal_api.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extensions_browser_client.h"

namespace {

static base::LazyInstance<
    extensions::PrinterProviderAPIFactory>::DestructorAtExit g_api_factory =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace extensions {

// static
PrinterProviderAPIFactory* PrinterProviderAPIFactory::GetInstance() {
  return g_api_factory.Pointer();
}

PrinterProviderAPI* PrinterProviderAPIFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<PrinterProviderAPI*>(
      GetServiceForBrowserContext(context, true));
}

PrinterProviderAPIFactory::PrinterProviderAPIFactory()
    : BrowserContextKeyedServiceFactory(
          "PrinterProviderAPI",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  DependsOn(PrinterProviderInternalAPI::GetFactoryInstance());
  DependsOn(ExtensionRegistryFactory::GetInstance());
}

PrinterProviderAPIFactory::~PrinterProviderAPIFactory() {
}

KeyedService* PrinterProviderAPIFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return PrinterProviderAPI::Create(context);
}

content::BrowserContext* PrinterProviderAPIFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return ExtensionsBrowserClient::Get()->GetContextRedirectedToOriginal(
      context, /*force_guest_profile=*/true);
}

}  // namespace extensions
