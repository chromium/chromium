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
#include "components/value_store/value_store.h"
#include "extensions/browser/api/storage/session_storage_manager.h"
#include "extensions/browser/api/storage/settings_namespace.h"
#include "extensions/browser/api/storage/settings_observer.h"
#include "extensions/browser/api/storage/value_store_cache.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/common/api/storage.h"
#include "extensions/common/extension_id.h"

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
  struct ResultStatus {
    ResultStatus();
    ResultStatus(const ResultStatus&);
    ~ResultStatus();

    bool success = true;
    std::optional<std::string> error;
  };

  struct GetKeysResult {
    GetKeysResult();
    GetKeysResult(const GetKeysResult&) = delete;
    GetKeysResult(GetKeysResult&& other);
    ~GetKeysResult();

    ResultStatus status;
    std::optional<base::Value::List> data;
  };

  struct GetResult {
    GetResult();
    GetResult(const GetResult&) = delete;
    GetResult(GetResult&& other);
    ~GetResult();

    ResultStatus status;
    std::optional<base::Value::Dict> data;
  };

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
  void DeleteStorageSoon(const ExtensionId& extension_id,
                         base::OnceClosure done_callback);

  // For a given `extension` and `storage_area`, retrieves a map of key value
  // pairs from storage and fires `callback` with the result. If `keys` is
  // specified, only the specified keys are retrieved. Otherwise, all data is
  // returned.
  void GetValues(scoped_refptr<const Extension> extension,
                 StorageAreaNamespace storage_area,
                 std::optional<std::vector<std::string>> keys,
                 base::OnceCallback<void(GetResult)> callback);

  // For a given `extension` and `storage_area`, retrieves a list of keys and
  // fires `callback` with the result.
  void GetKeys(scoped_refptr<const Extension> extension,
               StorageAreaNamespace storage_area,
               base::OnceCallback<void(GetKeysResult)> callback);

  // For a given `extension` and `storage_area`, determines the number of bytes
  // in use and fires `callback` with the result. If `keys` is specified, the
  // result is based only on keys contained within the vector. Otherwise, all
  // keys are included.
  void GetBytesInUse(scoped_refptr<const Extension> extension,
                     StorageAreaNamespace storage_area,
                     std::optional<std::vector<std::string>> keys,
                     base::OnceCallback<void(size_t)> callback);

  // For a given `extension` and `storage_area`, sets the values specified by
  // `values` in storage and fires `callback`.
  void Set(scoped_refptr<const Extension> extension,
           StorageAreaNamespace storage_area,
           base::Value::Dict values,
           base::OnceCallback<void(ResultStatus)> callback);

  // For a given `extension` and `storage_area`, removes the items specified by
  // `keys` from storage and fires `callback`.
  void Remove(scoped_refptr<const Extension> extension,
              StorageAreaNamespace storage_area,
              const std::vector<std::string>& keys,
              base::OnceCallback<void(ResultStatus)> callback);

  // For a given `extension` and `storage_area`, clears the storage and fires
  // `callback`.
  void Clear(scoped_refptr<const Extension> extension,
             StorageAreaNamespace storage_area,
             base::OnceCallback<void(ResultStatus)> callback);

  // Gets the Settings change callback.
  SettingsChangedCallback GetObserver();

  void SetCacheForTesting(settings_namespace::Namespace settings_namespace,
                          std::unique_ptr<ValueStoreCache> cache);

  void DisableStorageForTesting(
      settings_namespace::Namespace settings_namespace);

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<StorageFrontend>* GetFactoryInstance();
  static const char* service_name();
  static const bool kServiceRedirectedInIncognito = true;
  static const bool kServiceIsNULLWhileTesting = true;

 private:
  friend class BrowserContextKeyedAPIFactory<StorageFrontend>;

  typedef std::map<settings_namespace::Namespace,
                   raw_ptr<ValueStoreCache, CtnExperimental>>
      CacheMap;

  // Constructor for normal BrowserContextKeyedAPI usage.
  explicit StorageFrontend(content::BrowserContext* context);

  // Constructor for tests.
  StorageFrontend(scoped_refptr<value_store::ValueStoreFactory> storage_factory,
                  content::BrowserContext* context);

  void Init(scoped_refptr<value_store::ValueStoreFactory> storage_factory);

  // Should be called on the UI thread after a read has been performed in
  // `storage_area`. Fires `callback` with the keys from `get_result`.
  void OnReadKeysFinished(base::OnceCallback<void(GetKeysResult)> callback,
                          GetResult get_result);

  // Should be called on the UI thread after a read has been performed in
  // `storage_area`. Fires `callback` with the `result` from the read
  // operation.
  void OnReadFinished(const ExtensionId& extension_id,
                      StorageAreaNamespace storage_area,
                      base::OnceCallback<void(GetResult)> callback,
                      value_store::ValueStore::ReadResult result);

  // Should be called on the UI thread after a write has been performed in
  // `storage_area`. Fires events if any values were changed and then runs
  // `callback` with the `result` from the write operation.
  void OnWriteFinished(const ExtensionId& extension_id,
                       StorageAreaNamespace storage_area,
                       base::OnceCallback<void(ResultStatus)> callback,
                       value_store::ValueStore::WriteResult result);

  // Called when storage with `storage_area` for `extension_id` is updated with
  // `changes`. Must include `session_access_level` iff `storage_area` is
  // session (other storage areas don't support access levels, see
  // crbug.com/1508463).
  void OnSettingsChanged(
      const ExtensionId& extension_id,
      StorageAreaNamespace storage_area,
      std::optional<api::storage::AccessLevel> session_access_level,
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
