// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_TEST_EXTENSION_REGISTRY_OBSERVER_H_
#define EXTENSIONS_BROWSER_TEST_EXTENSION_REGISTRY_OBSERVER_H_

#include <memory>
#include <string>

#include "base/scoped_observation.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension_id.h"

namespace extensions {

// A helper class that listen for ExtensionRegistry notifications.
class TestExtensionRegistryObserver : public ExtensionRegistryObserver {
 public:
  // If |extension_id| is provided, listens only to events relating to that
  // extension. Otherwise, listens to all events.
  explicit TestExtensionRegistryObserver(ExtensionRegistry* registry);
  TestExtensionRegistryObserver(ExtensionRegistry* registry,
                                const ExtensionId& extension_id);

  TestExtensionRegistryObserver(const TestExtensionRegistryObserver&) = delete;
  TestExtensionRegistryObserver& operator=(
      const TestExtensionRegistryObserver&) = delete;

  ~TestExtensionRegistryObserver() override;

  // Waits for the notification, and returns the extension that caused it.
  scoped_refptr<const Extension> WaitForExtensionWillBeInstalled();
  scoped_refptr<const Extension> WaitForExtensionInstalled();
  scoped_refptr<const Extension> WaitForExtensionUninstalled();
  scoped_refptr<const Extension> WaitForExtensionUninstallationDenied();
  scoped_refptr<const Extension> WaitForExtensionLoaded();
  scoped_refptr<const Extension> WaitForExtensionReady();
  scoped_refptr<const Extension> WaitForExtensionUnloaded();

 private:
  class Waiter;

  // ExtensionRegistryObserver.
  void OnExtensionWillBeInstalled(content::BrowserContext* browser_context,
                                  const Extension* extension,
                                  bool is_update,
                                  const std::string& old_name) override;
  void OnExtensionInstalled(content::BrowserContext* browser_context,
                            const Extension* extension,
                            bool is_update) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const Extension* extension,
                              extensions::UninstallReason reason) override;
  void OnExtensionUninstallationDenied(content::BrowserContext* browser_context,
                                       const Extension* extension) override;
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionReady(content::BrowserContext* browser_context,
                        const Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  scoped_refptr<const Extension> Wait(std::unique_ptr<Waiter>* waiter);

  std::unique_ptr<Waiter> will_be_installed_waiter_;
  std::unique_ptr<Waiter> installed_waiter_;
  std::unique_ptr<Waiter> uninstalled_waiter_;
  std::unique_ptr<Waiter> uninstallation_denied_waiter_;
  std::unique_ptr<Waiter> loaded_waiter_;
  std::unique_ptr<Waiter> ready_waiter_;
  std::unique_ptr<Waiter> unloaded_waiter_;

  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observation_{this};

  ExtensionId extension_id_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_TEST_EXTENSION_REGISTRY_OBSERVER_H_
