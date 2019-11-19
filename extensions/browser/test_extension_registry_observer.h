// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_TEST_EXTENSION_REGISTRY_OBSERVER_H_
#define EXTENSIONS_BROWSER_TEST_EXTENSION_REGISTRY_OBSERVER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"

namespace extensions {

// A helper class that listen for ExtensionRegistry notifications.
class TestExtensionRegistryObserver : public ExtensionRegistryObserver {
 public:
  // If |extension_id| is provided, listens only to events relating to that
  // extension. Otherwise, listens to all events.
  explicit TestExtensionRegistryObserver(ExtensionRegistry* registry);
  TestExtensionRegistryObserver(ExtensionRegistry* registry,
                                const std::string& extension_id);

  ~TestExtensionRegistryObserver() override;

  // Waits for the notification, and returns the extension that caused it.
  // TODO(lazyboy): Return scoped_refptr<const Extension> from all of these
  // methods for consistency.
  const Extension* WaitForExtensionWillBeInstalled();
  const Extension* WaitForExtensionInstalled();
  scoped_refptr<const Extension> WaitForExtensionUninstalled();
  const Extension* WaitForExtensionLoaded();
  const Extension* WaitForExtensionReady();
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
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionReady(content::BrowserContext* browser_context,
                        const Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  const Extension* Wait(std::unique_ptr<Waiter>* waiter);

  std::unique_ptr<Waiter> will_be_installed_waiter_;
  std::unique_ptr<Waiter> installed_waiter_;
  std::unique_ptr<Waiter> uninstalled_waiter_;
  std::unique_ptr<Waiter> loaded_waiter_;
  std::unique_ptr<Waiter> ready_waiter_;
  std::unique_ptr<Waiter> unloaded_waiter_;

  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_{this};

  std::string extension_id_;

  DISALLOW_COPY_AND_ASSIGN(TestExtensionRegistryObserver);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_TEST_EXTENSION_REGISTRY_OBSERVER_H_
