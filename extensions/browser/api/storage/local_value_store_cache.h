// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_STORAGE_LOCAL_VALUE_STORE_CACHE_H_
#define EXTENSIONS_BROWSER_API_STORAGE_LOCAL_VALUE_STORE_CACHE_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/memory/scoped_refptr.h"
#include "extensions/browser/api/storage/settings_storage_quota_enforcer.h"
#include "extensions/browser/api/storage/value_store_cache.h"
#include "extensions/common/extension_id.h"

namespace value_store {
class ValueStoreFactory;
}

namespace extensions {

// ValueStoreCache for the LOCAL namespace. It owns a backend for apps and
// another for extensions. Each backend takes care of persistence.
class LocalValueStoreCache : public ValueStoreCache {
 public:
  explicit LocalValueStoreCache(
      scoped_refptr<value_store::ValueStoreFactory> factory);

  LocalValueStoreCache(const LocalValueStoreCache&) = delete;
  LocalValueStoreCache& operator=(const LocalValueStoreCache&) = delete;

  ~LocalValueStoreCache() override;

  // ValueStoreCache implementation:
  void RunWithValueStoreForExtension(
      StorageCallback callback,
      scoped_refptr<const Extension> extension) override;
  void DeleteStorageSoon(const ExtensionId& extension_id) override;

 private:
  using StorageMap =
      std::map<std::string, std::unique_ptr<value_store::ValueStore>>;

  value_store::ValueStore* GetStorage(const Extension* extension);

  // The Factory to use for creating new ValueStores.
  const scoped_refptr<value_store::ValueStoreFactory> storage_factory_;

  // Quota limits (see SettingsStorageQuotaEnforcer).
  const SettingsStorageQuotaEnforcer::Limits quota_;

  // The collection of ValueStores for local storage.
  StorageMap storage_map_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_STORAGE_LOCAL_VALUE_STORE_CACHE_H_
