// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/core_browser_context_keyed_service_factories.h"

#include "components/guest_view/buildflags/buildflags.h"
#include "extensions/browser/api/web_request/web_request_event_router_factory.h"
#include "extensions/browser/delayed_install_manager_factory.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_navigation_registry.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_prefs_helper_factory.h"
#include "extensions/browser/extension_protocols.h"
#include "extensions/browser/extension_registrar_factory.h"
#include "extensions/browser/image_loader_factory.h"
#include "extensions/browser/message_tracker.h"
#include "extensions/browser/pending_extension_manager_factory.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/browser/process_manager_factory.h"
#include "extensions/browser/renderer_startup_helper.h"
#include "extensions/browser/service_worker/service_worker_keepalive.h"
#include "extensions/browser/service_worker/service_worker_task_queue_factory.h"
#include "extensions/browser/updater/update_service_factory.h"
#include "extensions/browser/user_script_world_configuration_manager.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_GUEST_VIEW)
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_stream_manager.h"
#endif

#if BUILDFLAG(ENABLE_PLATFORM_APPS)
#include "extensions/browser/app_window/app_window_geometry_cache.h"
#include "extensions/browser/app_window/app_window_registry.h"
#endif

namespace extensions {

void EnsureCoreBrowserContextKeyedServiceFactoriesBuilt() {
#if BUILDFLAG(ENABLE_PLATFORM_APPS)
  AppWindowGeometryCache::Factory::GetInstance();
  AppWindowRegistry::Factory::GetInstance();
#endif
  DelayedInstallManagerFactory::GetInstance();
  EnsureExtensionURLLoaderFactoryShutdownNotifierFactoryBuilt();
  EventRouterFactory::GetInstance();
  ExtensionActionManager::GetFactory();
  ExtensionFunction::EnsureShutdownNotifierFactoryBuilt();
  ExtensionPrefsFactory::GetInstance();
  ExtensionNavigationRegistry::GetFactoryInstance();
  ExtensionPrefsHelperFactory::GetInstance();
  ExtensionRegistrarFactory::GetInstance();
  ImageLoaderFactory::GetInstance();
  MessageTracker::GetFactory();
#if BUILDFLAG(ENABLE_GUEST_VIEW)
  MimeHandlerStreamManager::EnsureFactoryBuilt();
#endif
  PendingExtensionManagerFactory::GetInstance();
  PermissionsManager::GetFactory();
  ProcessManagerFactory::GetInstance();
  RendererStartupHelperFactory::GetInstance();
  ServiceWorkerKeepalive::EnsureShutdownNotifierFactoryBuilt();
  ServiceWorkerTaskQueueFactory::GetInstance();
  UpdateServiceFactory::GetInstance();
  UserScriptWorldConfigurationManager::GetFactory();
  WebRequestEventRouterFactory::GetInstance();
}

}  // namespace extensions
