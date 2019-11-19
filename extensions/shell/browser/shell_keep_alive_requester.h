// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_KEEP_ALIVE_REQUESTER_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_KEEP_ALIVE_REQUESTER_H_

#include "apps/app_lifetime_monitor.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension_id.h"

class ScopedKeepAlive;

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {

// Maintains keep-alives while apps are loading or launching.
//
// Motivation: When an app reloads, its app windows close. If these were the
// last windows open, the application may exit. Keep-alives can be used to
// ensure the application doesn't exit until the app has had a chance to load
// and launch.
class ShellKeepAliveRequester : public ExtensionRegistryObserver,
                                public apps::AppLifetimeMonitor::Observer {
 public:
  explicit ShellKeepAliveRequester(content::BrowserContext* browser_context);
  ~ShellKeepAliveRequester() override;

  // Owner should call this before starting to reload an app, so that a
  // keep-alive may be created when the app is closed.
  // Should be followed by a call to StopTrackingReload().
  void StartTrackingReload(const Extension* extension);

  // Owner should call this after reloading an app succeeds or fails, so the
  // keep-alive can be cleaned up.
  void StopTrackingReload(const ExtensionId& old_extension_id);

  // ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  // apps::AppLifetimeMonitor::Observer:
  void OnAppActivated(content::BrowserContext* context,
                      const std::string& app_id) override;
  void OnAppStop(content::BrowserContext* context,
                 const std::string& app_id) override;

 private:
  // Keep-alives saved while waiting for apps to finish reloading or launch
  // their first app window, since the DesktopController might have no windows
  // and would otherwise exit.
  base::flat_map<ExtensionId, std::unique_ptr<ScopedKeepAlive>>
      app_launching_keep_alives_;
  base::flat_map<ExtensionId, std::unique_ptr<ScopedKeepAlive>>
      app_reloading_keep_alives_;

  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_{this};
  ScopedObserver<apps::AppLifetimeMonitor, apps::AppLifetimeMonitor::Observer>
      app_lifetime_monitor_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(ShellKeepAliveRequester);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_KEEP_ALIVE_REQUESTER_H_
