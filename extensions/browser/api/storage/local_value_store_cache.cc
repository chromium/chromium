// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/storage/local_value_store_cache.h"

#include <stddef.h>

#include <limits>
#include <utility>

#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/storage/backend_task_runner.h"
#include "extensions/browser/api/storage/weak_unlimited_settings_storage.h"
#include "extensions/browser/value_store/value_store_factory.h"
#include "extensions/common/api/storage.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permissions_data.h"

using content::BrowserThread;

namespace extensions {

namespace {

// Returns the quota limit for local storage, taken from the schema in
// extensions/common/api/storage.json.
SettingsStorageQuotaEnforcer::Limits GetLocalQuotaLimits() {
  SettingsStorageQuotaEnforcer::Limits limits = {
      static_cast<size_t>(api::storage::local::QUOTA_BYTES),
      std::numeric_limits<size_t>::max(), std::numeric_limits<size_t>::max()};
  return limits;
}

}  // namespace

LocalValueStoreCache::LocalValueStoreCache(
    scoped_refptr<ValueStoreFactory> factory)
    : storage_factory_(std::move(factory)), quota_(GetLocalQuotaLimits()) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

LocalValueStoreCache::~LocalValueStoreCache() {
  DCHECK(IsOnBackendSequence());
}

void LocalValueStoreCache::RunWithValueStoreForExtension(
    const StorageCallback& callback,
    scoped_refptr<const Extension> extension) {
  DCHECK(IsOnBackendSequence());

  ValueStore* storage = GetStorage(extension.get());

  // A neat way to implement unlimited storage; if the extension has the
  // unlimited storage permission, force through all calls to Set().
  if (extension->permissions_data()->HasAPIPermission(
          APIPermission::kUnlimitedStorage)) {
    WeakUnlimitedSettingsStorage unlimited_storage(storage);
    callback.Run(&unlimited_storage);
  } else {
    callback.Run(storage);
  }
}

void LocalValueStoreCache::DeleteStorageSoon(const std::string& extension_id) {
  DCHECK(IsOnBackendSequence());
  storage_map_.erase(extension_id);
  storage_factory_->DeleteSettings(settings_namespace::LOCAL,
                                   ValueStoreFactory::ModelType::APP,
                                   extension_id);
  storage_factory_->DeleteSettings(settings_namespace::LOCAL,
                                   ValueStoreFactory::ModelType::EXTENSION,
                                   extension_id);
}

ValueStore* LocalValueStoreCache::GetStorage(const Extension* extension) {
  auto iter = storage_map_.find(extension->id());
  if (iter != storage_map_.end())
    return iter->second.get();

  ValueStoreFactory::ModelType model_type =
      extension->is_app() ? ValueStoreFactory::ModelType::APP
                          : ValueStoreFactory::ModelType::EXTENSION;
  std::unique_ptr<ValueStore> store = storage_factory_->CreateSettingsStore(
      settings_namespace::LOCAL, model_type, extension->id());
  std::unique_ptr<SettingsStorageQuotaEnforcer> storage(
      new SettingsStorageQuotaEnforcer(quota_, std::move(store)));
  DCHECK(storage.get());

  ValueStore* storage_ptr = storage.get();
  storage_map_[extension->id()] = std::move(storage);
  return storage_ptr;
}

}  // namespace extensions
