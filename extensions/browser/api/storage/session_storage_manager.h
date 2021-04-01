// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_STORAGE_SESSION_STORAGE_MANAGER_H_
#define EXTENSIONS_BROWSER_API_STORAGE_SESSION_STORAGE_MANAGER_H_

#include <map>
#include <string>

#include "base/supports_user_data.h"
#include "base/values.h"
#include "extensions/common/extension_id.h"

namespace extensions {

// SessionStorageManager manages the content stored in memory by
// chrome.storage.session.
class SessionStorageManager : public base::SupportsUserData::Data {
 public:
  explicit SessionStorageManager(size_t quota_bytes_per_extension);
  ~SessionStorageManager() override;
  SessionStorageManager(const SessionStorageManager& other) = delete;
  SessionStorageManager& operator=(SessionStorageManager& other) = delete;

  // Returns the value for the given `extension_id` and `key`, or null if none
  // exists.
  const base::Value* Get(const ExtensionId& extension_id,
                         const std::string& key) const;

  // Stores multiple values of an extension id.
  bool Set(const ExtensionId& extension_id,
           std::map<std::string, base::Value> values);

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

    // Returns the value for the given `key`, or null if none exists.
    const base::Value* Get(const std::string& key) const;

    // Stores the input values in the values map.
    bool Set(std::map<std::string, base::Value> input_values);

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
