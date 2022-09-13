// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_keep_alive_requester.h"

#include "apps/app_lifetime_monitor_factory.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "extensions/browser/extension_prefs.h"

namespace extensions {

ShellKeepAliveRequester::ShellKeepAliveRequester(
    content::BrowserContext* browser_context) {
  extension_registry_observation_.Observe(
      ExtensionRegistry::Get(browser_context));
  app_lifetime_monitor_observation_.Observe(
      apps::AppLifetimeMonitorFactory::GetForBrowserContext(browser_context));
}

ShellKeepAliveRequester::~ShellKeepAliveRequester() = default;

void ShellKeepAliveRequester::StartTrackingReload(const Extension* extension) {
  if (!extension->is_platform_app())
    return;

  // The app will be reloaded, closing its windows. Add a keep-alive to wait for
  // the app to unload and reload.
  app_reloading_keep_alives_[extension->id()] =
      std::make_unique<ScopedKeepAlive>(KeepAliveOrigin::APP_CONTROLLER,
                                        KeepAliveRestartOption::ENABLED);
}

void ShellKeepAliveRequester::StopTrackingReload(
    const ExtensionId& old_extension_id) {
  // No longer waiting for reload to complete.
  app_reloading_keep_alives_.erase(old_extension_id);
}

void ShellKeepAliveRequester::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  if (!extension->is_platform_app())
    return;

  // Add a keep-alive to wait for the app to launch its first app window, as
  // otherwise the Aura desktop controller may exit. The assumption is that all
  // apps will create a visible window. If the app doesn't, this keep-alive will
  // still be erased once the app's background page eventually stops.
  app_launching_keep_alives_[extension->id()] =
      std::make_unique<ScopedKeepAlive>(KeepAliveOrigin::APP_CONTROLLER,
                                        KeepAliveRestartOption::ENABLED);
}

void ShellKeepAliveRequester::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  // There may already be a keep-alive waiting for the app to launch a window.
  // Remove that; another will be created if the extension successfully loads.
  app_launching_keep_alives_.erase(extension->id());
}

void ShellKeepAliveRequester::OnAppActivated(content::BrowserContext* context,
                                             const std::string& app_id) {
  // The app has launched its first window. The desktop controller will keep
  // running until all app windows close.
  app_launching_keep_alives_.erase(app_id);
}

void ShellKeepAliveRequester::OnAppStop(content::BrowserContext* context,
                                        const std::string& app_id) {
  // The app will still have a keep-alive if it never showed a window.
  app_launching_keep_alives_.erase(app_id);
}

}  // namespace extensions
