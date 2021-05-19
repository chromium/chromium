// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_STORAGE_SESSION_STORAGE_MANAGER_H_
#define EXTENSIONS_BROWSER_API_STORAGE_SESSION_STORAGE_MANAGER_H_

#include <map>
#include <string>
#include <vector>

#include "base/supports_user_data.h"
#include "base/values.h"
#include "extensions/common/extension_id.h"

namespace extensions {

// SessionStorageManager manages the content stored in memory by
// chrome.storage.session.
// This class is optimized to reduce the number of possible copies of
// base::Values; this is because these values could potentially be rather
// large in size. This results in some slightly unwieldy function
// signatures.
class SessionStorageManager : public base::SupportsUserData::Data {
 public:
  struct ValueChange {
    ValueChange(std::string key,
                absl::optional<base::Value> old_value,
                base::Value* new_value);
    ~ValueChange();
    ValueChange(const ValueChange& other) = delete;
    ValueChange& operator=(const ValueChange& other) = delete;
    ValueChange(ValueChange&& other);

    std::string key;

    absl::optional<base::Value> old_value;

    // Owned by the SessionStorageManager. Caller cannot rely on it after any
    // subsequent calls to SessionStorageManager methods.
    const base::Value* new_value;
  };

  explicit SessionStorageManager(size_t quota_bytes_per_extension);
  ~SessionStorageManager() override;
  SessionStorageManager(const SessionStorageManager& other) = delete;
  SessionStorageManager& operator=(SessionStorageManager& other) = delete;

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

  // Stores multiple values of an extension id.
  bool Set(const ExtensionId& extension_id,
           std::map<std::string, base::Value> values,
           std::vector<ValueChange>& changes);

  // Removes multiple keys for the given `extension_id`.
  void Remove(const ExtensionId& extension_id,
              const std::vector<std::string>& keys,
              std::vector<ValueChange>& changes);

  // Removes a key for the given `extension_id`.
  void Remove(const ExtensionId& extension_id,
              const std::string& key,
              std::vector<ValueChange>& changes);

  // Clears the storage of the given `extension_id`.
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
    // if a change occurs.
    bool Set(std::map<std::string, base::Value> input_values,
             std::vector<ValueChange>& changes);

    // Removes multiple keys from the storage.
    void Remove(const std::vector<std::string>& keys,
                std::vector<ValueChange>& changes);

    // Clears the storage.
    void Clear(std::vector<ValueChange>& changes);

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

  // Map of extension id to its storage.
  std::map<ExtensionId, std::unique_ptr<ExtensionStorage>> extensions_storage_;

  // The total quota for each extension in bytes.
  const size_t quota_bytes_per_extension_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_STORAGE_SESSION_STORAGE_MANAGER_H_
