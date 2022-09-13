// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_SERVICE_WORKER_MANAGER_H_
#define EXTENSIONS_BROWSER_SERVICE_WORKER_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"

namespace content {
class BrowserContext;
}

namespace extensions {

// A helper class to manage extension service workers. Handles suspending
// them when the extension is unloaded and removing them when the extension is
// uninstalled.
class ServiceWorkerManager : public ExtensionRegistryObserver {
 public:
  explicit ServiceWorkerManager(content::BrowserContext* browser_context);

  ServiceWorkerManager(const ServiceWorkerManager&) = delete;
  ServiceWorkerManager& operator=(const ServiceWorkerManager&) = delete;

  ~ServiceWorkerManager() override;

 private:
  // ExtensionRegistryObserver:
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const Extension* extension,
                              extensions::UninstallReason reason) override;

  raw_ptr<content::BrowserContext> browser_context_;

  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      registry_observation_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_SERVICE_WORKER_MANAGER_H_
