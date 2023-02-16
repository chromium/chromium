// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/storage/settings_storage_quota_enforcer.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "extensions/common/extension_api.h"

using value_store::ValueStore;

namespace extensions {

namespace {

// Resources there are a quota for.
enum Resource {
  QUOTA_BYTES,
  QUOTA_BYTES_PER_ITEM,
  MAX_ITEMS
};

// Allocates a setting in a record of total and per-setting usage.
void Allocate(
    const std::string& key,
    const base::Value& value,
    size_t* used_total,
    std::map<std::string, size_t>* used_per_setting) {
  // Calculate the setting size based on its JSON serialization size.
  // TODO(kalman): Does this work with different encodings?
  // TODO(kalman): This is duplicating work that the leveldb delegate
  // implementation is about to do, and it would be nice to avoid this.
  std::string value_as_json;
  base::JSONWriter::Write(value, &value_as_json);
  size_t new_size = key.size() + value_as_json.size();
  size_t existing_size = (*used_per_setting)[key];

  *used_total += (new_size - existing_size);
  (*used_per_setting)[key] = new_size;
}

ValueStore::Status QuotaExceededError(Resource resource) {
  const char* name = nullptr;
  switch (resource) {
    case QUOTA_BYTES:
      name = "QUOTA_BYTES";
      break;
    case QUOTA_BYTES_PER_ITEM:
      name = "QUOTA_BYTES_PER_ITEM";
      break;
    case MAX_ITEMS:
      name = "MAX_ITEMS";
      break;
  }
  CHECK(name);
  return ValueStore::Status(ValueStore::QUOTA_EXCEEDED,
                            base::StringPrintf("%s quota exceeded", name));
}

}  // namespace

SettingsStorageQuotaEnforcer::SettingsStorageQuotaEnforcer(
    const Limits& limits,
    std::unique_ptr<ValueStore> delegate)
    : limits_(limits),
      delegate_(std::move(delegate)),
      used_total_(0),
      usage_calculated_(false) {}

SettingsStorageQuotaEnforcer::~SettingsStorageQuotaEnforcer() = default;

size_t SettingsStorageQuotaEnforcer::GetBytesInUse(const std::string& key) {
  LazyCalculateUsage();
  auto maybe_used = used_per_setting_.find(key);
  return maybe_used == used_per_setting_.end() ? 0u : maybe_used->second;
}

size_t SettingsStorageQuotaEnforcer::GetBytesInUse(
    const std::vector<std::string>& keys) {
  size_t used = 0;
  for (const std::string& key : keys)
    used += GetBytesInUse(key);
  return used;
}

size_t SettingsStorageQuotaEnforcer::GetBytesInUse() {
  // All ValueStore implementations rely on GetBytesInUse being
  // implemented here.
  LazyCalculateUsage();
  return used_total_;
}

ValueStore::ReadResult SettingsStorageQuotaEnforcer::Get(
    const std::string& key) {
  return HandleResult(delegate_->Get(key));
}

ValueStore::ReadResult SettingsStorageQuotaEnforcer::Get(
    const std::vector<std::string>& keys) {
  return HandleResult(delegate_->Get(keys));
}

ValueStore::ReadResult SettingsStorageQuotaEnforcer::Get() {
  return HandleResult(delegate_->Get());
}

ValueStore::WriteResult SettingsStorageQuotaEnforcer::Set(
    WriteOptions options, const std::string& key, const base::Value& value) {
  LazyCalculateUsage();
  size_t new_used_total = used_total_;
  std::map<std::string, size_t> new_used_per_setting = used_per_setting_;
  Allocate(key, value, &new_used_total, &new_used_per_setting);

  if (!(options & IGNORE_QUOTA)) {
    if (new_used_total > limits_.quota_bytes)
      return WriteResult(QuotaExceededError(QUOTA_BYTES));
    if (new_used_per_setting[key] > limits_.quota_bytes_per_item)
      return WriteResult(QuotaExceededError(QUOTA_BYTES_PER_ITEM));
    if (new_used_per_setting.size() > limits_.max_items)
      return WriteResult(QuotaExceededError(MAX_ITEMS));
  }

  WriteResult result = HandleResult(delegate_->Set(options, key, value));
  if (!result.status().ok())
    return result;

  if (usage_calculated_) {
    used_total_ = new_used_total;
    used_per_setting_.swap(new_used_per_setting);
  }
  return result;
}

ValueStore::WriteResult SettingsStorageQuotaEnforcer::Set(
    WriteOptions options,
    const base::Value::Dict& values) {
  LazyCalculateUsage();
  size_t new_used_total = used_total_;
  std::map<std::string, size_t> new_used_per_setting = used_per_setting_;
  for (const auto [key, value] : values) {
    Allocate(key, value, &new_used_total, &new_used_per_setting);

    if (!(options & IGNORE_QUOTA) &&
        new_used_per_setting[key] > limits_.quota_bytes_per_item) {
      return WriteResult(QuotaExceededError(QUOTA_BYTES_PER_ITEM));
    }
  }

  if (!(options & IGNORE_QUOTA)) {
    if (new_used_total > limits_.quota_bytes)
      return WriteResult(QuotaExceededError(QUOTA_BYTES));
    if (new_used_per_setting.size() > limits_.max_items)
      return WriteResult(QuotaExceededError(MAX_ITEMS));
  }

  WriteResult result = HandleResult(delegate_->Set(options, values));
  if (!result.status().ok())
    return result;

  if (usage_calculated_) {
    used_total_ = new_used_total;
    used_per_setting_ = new_used_per_setting;
  }

  return result;
}

ValueStore::WriteResult SettingsStorageQuotaEnforcer::Remove(
    const std::string& key) {
  LazyCalculateUsage();
  WriteResult result = HandleResult(delegate_->Remove(key));
  if (!result.status().ok())
    return result;
  Free(key);

  return result;
}

ValueStore::WriteResult SettingsStorageQuotaEnforcer::Remove(
    const std::vector<std::string>& keys) {
  WriteResult result = HandleResult(delegate_->Remove(keys));
  if (!result.status().ok())
    return result;

  for (const std::string& key : keys)
    Free(key);

  return result;
}

ValueStore::WriteResult SettingsStorageQuotaEnforcer::Clear() {
  LazyCalculateUsage();
  WriteResult result = HandleResult(delegate_->Clear());
  if (!result.status().ok())
    return result;

  used_per_setting_.clear();
  used_total_ = 0u;

  return result;
}

template <class T>
T SettingsStorageQuotaEnforcer::HandleResult(T result) {
  if (result.status().restore_status != RESTORE_NONE) {
    // Restoration means that an unknown amount, possibly all, of the data was
    // lost from the database. Reset our counters - they will be lazily
    // recalculated if/when needed.
    used_per_setting_.clear();
    used_total_ = 0u;
    usage_calculated_ = false;
  }
  return result;
}

void SettingsStorageQuotaEnforcer::LazyCalculateUsage() {
  if (usage_calculated_)
    return;

  DCHECK_EQ(0u, used_total_);
  DCHECK(used_per_setting_.empty());

  ReadResult maybe_settings = HandleResult(delegate_->Get());
  if (!maybe_settings.status().ok()) {
    LOG(WARNING) << "Failed to get settings for quota:"
                 << maybe_settings.status().message;
    return;
  }

  for (const auto [key, value] : maybe_settings.settings()) {
    Allocate(key, value, &used_total_, &used_per_setting_);
  }

  usage_calculated_ = true;
}

void SettingsStorageQuotaEnforcer::Free(const std::string& key) {
  if (!usage_calculated_)
    return;
  auto it = used_per_setting_.find(key);
  if (it == used_per_setting_.end())
    return;
  DCHECK_GE(used_total_, it->second);
  used_total_ -= it->second;
  used_per_setting_.erase(it);
}

}  // namespace extensions
