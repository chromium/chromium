// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/core_browser_context_keyed_service_factories.h"

#include "extensions/browser/app_window/app_window_geometry_cache.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_message_filter.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_protocols.h"
#include "extensions/browser/extension_service_worker_message_filter.h"
#include "extensions/browser/process_manager_factory.h"
#include "extensions/browser/renderer_startup_helper.h"
#include "extensions/browser/updater/update_service_factory.h"

namespace extensions {

void EnsureCoreBrowserContextKeyedServiceFactoriesBuilt() {
  AppWindowGeometryCache::Factory::GetInstance();
  AppWindowRegistry::Factory::GetInstance();
  EnsureExtensionURLLoaderFactoryShutdownNotifierFactoryBuilt();
  EventRouterFactory::GetInstance();
  ExtensionFunction::EnsureShutdownNotifierFactoryBuilt();
  ExtensionMessageFilter::EnsureShutdownNotifierFactoryBuilt();
  ExtensionServiceWorkerMessageFilter::EnsureShutdownNotifierFactoryBuilt();
  ExtensionPrefsFactory::GetInstance();
  ProcessManagerFactory::GetInstance();
  RendererStartupHelperFactory::GetInstance();
  UpdateServiceFactory::GetInstance();
}

}  // namespace extensions
