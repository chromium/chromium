// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_STORAGE_SETTINGS_STORAGE_QUOTA_ENFORCER_H_
#define EXTENSIONS_BROWSER_API_STORAGE_SETTINGS_STORAGE_QUOTA_ENFORCER_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "components/value_store/value_store.h"

namespace extensions {

// Enforces total quota and a per-setting quota in bytes, and a maximum number
// of setting keys, for a delegate storage area.
class SettingsStorageQuotaEnforcer : public value_store::ValueStore {
 public:
  struct Limits {
    // The total quota in bytes.
    size_t quota_bytes;

    // The quota for each individual item in bytes.
    size_t quota_bytes_per_item;

    // The maximum number of items allowed.
    size_t max_items;
  };

  SettingsStorageQuotaEnforcer(
      const Limits& limits,
      std::unique_ptr<value_store::ValueStore> delegate);

  SettingsStorageQuotaEnforcer(const SettingsStorageQuotaEnforcer&) = delete;
  SettingsStorageQuotaEnforcer& operator=(const SettingsStorageQuotaEnforcer&) =
      delete;

  ~SettingsStorageQuotaEnforcer() override;

  // ValueStore implementation.
  size_t GetBytesInUse(const std::string& key) override;
  size_t GetBytesInUse(const std::vector<std::string>& keys) override;
  size_t GetBytesInUse() override;
  ReadResult Get(const std::string& key) override;
  ReadResult Get(const std::vector<std::string>& keys) override;
  ReadResult Get() override;
  WriteResult Set(WriteOptions options,
                  const std::string& key,
                  const base::Value& value) override;
  WriteResult Set(WriteOptions options,
                  const base::Value::Dict& values) override;
  WriteResult Remove(const std::string& key) override;
  WriteResult Remove(const std::vector<std::string>& keys) override;
  WriteResult Clear() override;

  value_store::ValueStore* get_delegate_for_test() { return delegate_.get(); }

 private:
  template <class T>
  T HandleResult(T result);

  // Calculate the current usage for the database if not already calculated.
  void LazyCalculateUsage();

  // Frees the allocation of a setting in a record of total and per-setting
  // usage.
  void Free(const std::string& key);

  // Limits configuration.
  const Limits limits_;

  // The delegate storage area.
  std::unique_ptr<value_store::ValueStore> const delegate_;

  // Total bytes in used by |delegate_|. Includes both key lengths and
  // JSON-encoded values.
  size_t used_total_;

  // Have the total bytes used been calculated?
  bool usage_calculated_;

  // Map of item key to its size, including the key itself.
  std::map<std::string, size_t> used_per_setting_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_STORAGE_SETTINGS_STORAGE_QUOTA_ENFORCER_H_
