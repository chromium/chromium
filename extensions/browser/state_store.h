// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_STATE_STORE_H_
#define EXTENSIONS_BROWSER_STATE_STORE_H_

#include <set>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/value_store/value_store_frontend.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class ValueStoreFactory;

// A storage area for per-extension state that needs to be persisted to disk.
class StateStore : public base::SupportsWeakPtr<StateStore>,
                   public ExtensionRegistryObserver {
 public:
  typedef ValueStoreFrontend::ReadCallback ReadCallback;

  class TestObserver {
   public:
    virtual ~TestObserver() {}
    virtual void WillSetExtensionValue(const std::string& extension_id,
                                       const std::string& key) = 0;
  };

  // If |deferred_load| is true, we will defer the database loading until the
  // application is less busy on startup.
  StateStore(content::BrowserContext* context,
             const scoped_refptr<ValueStoreFactory>& store_factory,
             ValueStoreFrontend::BackendType backend_type,
             bool deferred_load);
  // This variant is useful for testing (using a mock ValueStore).
  StateStore(content::BrowserContext* context,
             std::unique_ptr<ValueStore> store);
  ~StateStore() override;

  // Register a key for removal upon extension install/uninstall. We remove
  // for install to reset state when an extension upgrades.
  void RegisterKey(const std::string& key);

  // Get the value associated with the given extension and key, and pass
  // it to |callback| asynchronously.
  void GetExtensionValue(const std::string& extension_id,
                         const std::string& key,
                         ReadCallback callback);

  // Sets a value for a given extension and key.
  void SetExtensionValue(const std::string& extension_id,
                         const std::string& key,
                         std::unique_ptr<base::Value> value);

  // Removes a value for a given extension and key.
  void RemoveExtensionValue(const std::string& extension_id,
                            const std::string& key);

  // Return whether or not the StateStore has initialized itself.
  bool IsInitialized() const;

  void AddObserver(TestObserver* observer);
  void RemoveObserver(TestObserver* observer);

  // Flushes the state store (finishes any pending reads and writes). Should
  // only be used for testing. Invokes |flushed_callback| upon completion.
  void FlushForTesting(base::OnceClosure flushed_callback);

 private:
  class DelayedTaskQueue;

  void Init();

  // Removes all keys registered for the given extension.
  void RemoveKeysForExtension(const std::string& extension_id);

  // ExtensionRegistryObserver implementation.
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const Extension* extension,
                              extensions::UninstallReason reason) override;
  void OnExtensionWillBeInstalled(content::BrowserContext* browser_context,
                                  const Extension* extension,
                                  bool is_update,
                                  const std::string& old_name) override;

  // The store that holds our key/values.
  std::unique_ptr<ValueStoreFrontend> store_;

  // List of all known keys. They will be cleared for each extension when it is
  // (un)installed.
  std::set<std::string> registered_keys_;

  // Keeps track of tasks we have delayed while starting up.
  std::unique_ptr<DelayedTaskQueue> task_queue_;

  base::ObserverList<TestObserver>::Unchecked observers_;

  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observation_{this};

  DISALLOW_COPY_AND_ASSIGN(StateStore);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_STATE_STORE_H_
