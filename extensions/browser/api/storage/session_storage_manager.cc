// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/storage/session_storage_manager.h"

#include "base/trace_event/memory_usage_estimator.h"
#include "extensions/common/api/storage.h"

using base::trace_event::EstimateMemoryUsage;

namespace extensions {

SessionStorageManager::SessionValue::SessionValue(base::Value value,
                                                  size_t size)
    : value(std::move(value)), size(size) {}

SessionStorageManager::ExtensionStorage::ExtensionStorage(size_t quota_bytes)
    : quota_bytes_(quota_bytes) {}

SessionStorageManager::ExtensionStorage::~ExtensionStorage() = default;

size_t SessionStorageManager::ExtensionStorage::CalculateUsage(
    std::map<std::string, base::Value> input_values,
    std::map<std::string, std::unique_ptr<SessionValue>>& session_values)
    const {
  size_t updated_used_total = used_total_;

  for (auto& input_it : input_values) {
    size_t input_size = EstimateMemoryUsage(input_it.first) +
                        EstimateMemoryUsage(input_it.second);
    updated_used_total += input_size;

    // Remove session value size of existent key from total used bytes.
    auto existent_value_it = values_.find(input_it.first);
    if (existent_value_it != values_.end()) {
      // `updated_used_total` is guaranteed to be at least as large as any
      // individual value that's already in the map.
      DCHECK_GE(updated_used_total, existent_value_it->second->size);
      updated_used_total -= existent_value_it->second->size;
    }

    // Add input to the session values map.
    session_values.emplace(
        std::move(input_it.first),
        std::make_unique<SessionValue>(std::move(input_it.second), input_size));
  }

  if (updated_used_total >= quota_bytes_) {
    session_values.clear();
    return quota_bytes_;
  }

  return updated_used_total;
}

const base::Value* SessionStorageManager::ExtensionStorage::Get(
    const std::string& key) const {
  auto value_it = values_.find(key);
  if (value_it == values_.end())
    return nullptr;

  return &value_it->second->value;
}

bool SessionStorageManager::ExtensionStorage::Set(
    std::map<std::string, base::Value> input_values) {
  std::map<std::string, std::unique_ptr<SessionValue>> session_values;
  size_t updated_used_total =
      CalculateUsage(std::move(input_values), session_values);
  if (updated_used_total == quota_bytes_)
    return false;

  // Insert values in storage map and update total bytes.
  for (auto& session_value : session_values) {
    values_[std::move(session_value.first)] = std::move(session_value.second);
  }
  used_total_ = updated_used_total;
  return true;
}

SessionStorageManager::SessionStorageManager(size_t quota_bytes_per_extension)
    : quota_bytes_per_extension_(quota_bytes_per_extension) {}

SessionStorageManager::~SessionStorageManager() = default;

const base::Value* SessionStorageManager::Get(const ExtensionId& extension_id,
                                              const std::string& key) const {
  auto storage_it = extensions_storage_.find(extension_id);
  if (storage_it == extensions_storage_.end())
    return nullptr;

  return storage_it->second->Get(key);
}

bool SessionStorageManager::Set(
    const ExtensionId& extension_id,
    std::map<std::string, base::Value> input_values) {
  auto& storage = extensions_storage_[extension_id];

  // Initialize the extension storage, if it doesn't already exist.
  if (!storage)
    storage = std::make_unique<ExtensionStorage>(quota_bytes_per_extension_);

  return storage->Set(std::move(input_values));
}

}  // namespace extensions
