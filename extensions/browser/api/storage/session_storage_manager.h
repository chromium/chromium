// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_STORAGE_SESSION_STORAGE_MANAGER_H_
#define EXTENSIONS_BROWSER_API_STORAGE_SESSION_STORAGE_MANAGER_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension_id.h"

class BrowserContextKeyedServiceFactory;

namespace extensions {

// SessionStorageManager manages the content stored in memory by
// chrome.storage.session.
// This class is optimized to reduce the number of possible copies of
// base::Values; this is because these values could potentially be rather
// large in size. This results in some slightly unwieldy function
// signatures.
class SessionStorageManager : public KeyedService,
                              public ExtensionRegistryObserver {
 public:
  struct ValueChange {
    ValueChange(std::string key,
                std::optional<base::Value> old_value,
                base::Value* new_value);
    ~ValueChange();
    ValueChange(const ValueChange& other) = delete;
    ValueChange& operator=(const ValueChange& other) = delete;
    ValueChange(ValueChange&& other);

    std::string key;

    std::optional<base::Value> old_value;

    // Owned by the SessionStorageManager. Caller cannot rely on it after any
    // subsequent calls to SessionStorageManager methods.
    raw_ptr<const base::Value, DanglingUntriaged> new_value;
  };

  SessionStorageManager(size_t quota_bytes_per_extension,
                        content::BrowserContext* browser_context);
  ~SessionStorageManager() override;
  SessionStorageManager(const SessionStorageManager& other) = delete;
  SessionStorageManager& operator=(SessionStorageManager& other) = delete;

  // Retrieves the SessionStorageManager for a given `browser_context`.
  static SessionStorageManager* GetForBrowserContext(
      content::BrowserContext* browser_context);

  // Retrieves the factory instance for the SessionStorageManager.
  static BrowserContextKeyedServiceFactory* GetFactory();

  // Returns the value for the given `extension_id` and `key`, or null if none
  // exists.
  const base::Value* Get(const ExtensionId& extension_id,
                         const std::string& key) const;

  // Returns a map with keys and values found in storage for the given
  // `extension_id`.
  std::map<std::string, const base::Value*> Get(
      const ExtensionId& extension_id,
      const std::vector<std::string>& keys) const;

  // Returns a map with all keys and values found in storage for the given
  // `extension_id`.
  std::map<std::string, const base::Value*> GetAll(
      const ExtensionId& extension_id) const;

  // Stores multiple `values` for an `extension_id`. If storing the values
  // succeeds, returns true and populates `changes` with the inserted values. If
  // storing the values fails (e.g. due to going over quota), returns false and
  // leaves `changes` untouched, storing an error in `error`.
  bool Set(const ExtensionId& extension_id,
           std::map<std::string, base::Value> values,
           std::vector<ValueChange>& changes,
           std::string* error);

  // Removes multiple `keys` for an `extension_id`. Populates `changes` with the
  // removed values.
  void Remove(const ExtensionId& extension_id,
              const std::vector<std::string>& keys,
              std::vector<ValueChange>& changes);

  // Removes a `key` for an `extension_id`. Populates `changes` with the removed
  // value.
  void Remove(const ExtensionId& extension_id,
              const std::string& key,
              std::vector<ValueChange>& changes);

  // Clears all keys for an `extension_id`.
  void Clear(const ExtensionId& extension_id);

  // Clears all keys for an `extension_id`. Populates `changes` with the cleared
  // values.
  void Clear(const ExtensionId& extension_id,
             std::vector<ValueChange>& changes);

  // Gets the total amount of bytes being used by multiple keys and values of
  // the given `extension_id`.
  size_t GetBytesInUse(const ExtensionId& extension_id,
                       const std::vector<std::string>& keys) const;

  // Gets the total amount of bytes being used by a key for the given
  // `extension_id`.
  size_t GetBytesInUse(const ExtensionId& extension_id,
                       const std::string& key) const;

  // Gets the total amount of bytes being used by the given `extension_id`.
  size_t GetTotalBytesInUse(const ExtensionId& extension_id) const;

 private:
  struct SessionValue {
    SessionValue(base::Value value, size_t size);

    base::Value value;

    // Total bytes in use by value and key that points to this object.
    size_t size;
  };

  class ExtensionStorage {
   public:
    explicit ExtensionStorage(size_t quota_bytes);
    ~ExtensionStorage();

    // Returns a map with keys and values found in storage.
    std::map<std::string, const base::Value*> Get(
        const std::vector<std::string>& keys) const;

    // Returns a map with all keys and values found in storage.
    std::map<std::string, const base::Value*> GetAll() const;

    // Stores the input values in the values map, and updates the changes list
    // if a change occurs. If storing fails, returns false and populates
    // `error`.
    bool Set(std::map<std::string, base::Value> input_values,
             std::vector<ValueChange>& changes,
             std::string* error);

    // Removes multiple keys from the storage.
    void Remove(const std::vector<std::string>& keys,
                std::vector<ValueChange>& changes);

    // Clears the storage.
    void Clear(std::vector<ValueChange>& changes);
    void Clear();

    // Gets the total amount of bytes being used by multiple keys and values.
    size_t GetBytesInUse(const std::vector<std::string>& keys) const;

    // Gets the total amount of bytes stored.
    size_t GetTotalBytesInUse() const;

   private:
    // Returns the updated usage for the input values and adds them as session
    // values if there is available space, or returns the max quota bytes.
    size_t CalculateUsage(std::map<std::string, base::Value> input_values,
                          std::map<std::string, std::unique_ptr<SessionValue>>&
                              session_values) const;

    // The total quota in bytes.
    size_t quota_bytes_;

    // Total bytes stored in session by the extension. Includes both keys and
    // values.
    size_t used_total_ = 0;

    // Map of value key to its session value.
    std::map<std::string, std::unique_ptr<SessionValue>> values_;
  };

  // ExtensionRegistryObserver:
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observation_{this};

  // Map of extension id to its storage.
  std::map<ExtensionId, std::unique_ptr<ExtensionStorage>> extensions_storage_;

  // The total quota for each extension in bytes.
  const size_t quota_bytes_per_extension_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_STORAGE_SESSION_STORAGE_MANAGER_H_
