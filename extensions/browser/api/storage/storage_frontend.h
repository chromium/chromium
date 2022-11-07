// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_STORAGE_STORAGE_FRONTEND_H_
#define EXTENSIONS_BROWSER_API_STORAGE_STORAGE_FRONTEND_H_

#include <map>
#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "extensions/browser/api/storage/settings_namespace.h"
#include "extensions/browser/api/storage/settings_observer.h"
#include "extensions/browser/api/storage/value_store_cache.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"

namespace content {
class BrowserContext;
}

namespace value_store {
class ValueStoreFactory;
}

namespace extensions {

// The component of the Storage API which runs on the UI thread.
class StorageFrontend : public BrowserContextKeyedAPI {
 public:
  // Returns the current instance for |context|.
  static StorageFrontend* Get(content::BrowserContext* context);

  // Creates with a specific |storage_factory|.
  static std::unique_ptr<StorageFrontend> CreateForTesting(
      scoped_refptr<value_store::ValueStoreFactory> storage_factory,
      content::BrowserContext* context);

  StorageFrontend(const StorageFrontend&) = delete;
  StorageFrontend& operator=(const StorageFrontend&) = delete;

  // Public so tests can create and delete their own instances.
  ~StorageFrontend() override;

  // Returns the value store cache for |settings_namespace|.
  ValueStoreCache* GetValueStoreCache(
      settings_namespace::Namespace settings_namespace) const;

  // Returns true if |settings_namespace| is a valid namespace.
  bool IsStorageEnabled(settings_namespace::Namespace settings_namespace) const;

  // Runs |callback| with the storage area of the given |settings_namespace|
  // for the |extension|.
  void RunWithStorage(scoped_refptr<const Extension> extension,
                      settings_namespace::Namespace settings_namespace,
                      ValueStoreCache::StorageCallback callback);

  // Deletes the settings for the given |extension_id| and synchronously invokes
  // |done_callback| once the settings are deleted.
  void DeleteStorageSoon(const std::string& extension_id,
                         base::OnceClosure done_callback);

  // Gets the Settings change callback.
  SettingsChangedCallback GetObserver();

  void DisableStorageForTesting(
      settings_namespace::Namespace settings_namespace);

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<StorageFrontend>* GetFactoryInstance();
  static const char* service_name();
  static const bool kServiceRedirectedInIncognito = true;
  static const bool kServiceIsNULLWhileTesting = true;

 private:
  friend class BrowserContextKeyedAPIFactory<StorageFrontend>;

  typedef std::map<settings_namespace::Namespace, ValueStoreCache*> CacheMap;

  // Constructor for normal BrowserContextKeyedAPI usage.
  explicit StorageFrontend(content::BrowserContext* context);

  // Constructor for tests.
  StorageFrontend(scoped_refptr<value_store::ValueStoreFactory> storage_factory,
                  content::BrowserContext* context);

  void Init(scoped_refptr<value_store::ValueStoreFactory> storage_factory);

  void OnSettingsChanged(const std::string& extension_id,
                         StorageAreaNamespace storage_area,
                         base::Value changes);

  // The (non-incognito) browser context this Frontend belongs to.
  const raw_ptr<content::BrowserContext> browser_context_;

  // Maps a known namespace to its corresponding ValueStoreCache. The caches
  // are owned by this object.
  CacheMap caches_;

  base::WeakPtrFactory<StorageFrontend> weak_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_STORAGE_STORAGE_FRONTEND_H_
