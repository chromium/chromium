// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/value_store/testing_value_store.h"

#include <memory>
#include <utility>

#include "base/notreached.h"

namespace {

const char kGenericErrorMessage[] = "TestingValueStore configured to error";

// Having this utility function allows ValueStore::Status to not have a copy
// constructor.
ValueStore::Status CreateStatusCopy(const ValueStore::Status& status) {
  return ValueStore::Status(status.code, status.restore_status, status.message);
}

}  // namespace

TestingValueStore::TestingValueStore() : read_count_(0), write_count_(0) {}

TestingValueStore::~TestingValueStore() {}

void TestingValueStore::set_status_code(StatusCode status_code) {
  status_ = ValueStore::Status(status_code, kGenericErrorMessage);
}

size_t TestingValueStore::GetBytesInUse(const std::string& key) {
  // Let SettingsStorageQuotaEnforcer implement this.
  NOTREACHED() << "Not implemented";
  return 0;
}

size_t TestingValueStore::GetBytesInUse(
    const std::vector<std::string>& keys) {
  // Let SettingsStorageQuotaEnforcer implement this.
  NOTREACHED() << "Not implemented";
  return 0;
}

size_t TestingValueStore::GetBytesInUse() {
  // Let SettingsStorageQuotaEnforcer implement this.
  NOTREACHED() << "Not implemented";
  return 0;
}

ValueStore::ReadResult TestingValueStore::Get(const std::string& key) {
  return Get(std::vector<std::string>(1, key));
}

ValueStore::ReadResult TestingValueStore::Get(
    const std::vector<std::string>& keys) {
  read_count_++;
  if (!status_.ok())
    return ReadResult(CreateStatusCopy(status_));

  auto settings = std::make_unique<base::DictionaryValue>();
  for (auto it = keys.cbegin(); it != keys.cend(); ++it) {
    base::Value* value = NULL;
    if (storage_.GetWithoutPathExpansion(*it, &value)) {
      settings->SetKey(*it, value->Clone());
    }
  }
  return ReadResult(std::move(settings), CreateStatusCopy(status_));
}

ValueStore::ReadResult TestingValueStore::Get() {
  read_count_++;
  if (!status_.ok())
    return ReadResult(CreateStatusCopy(status_));
  return ReadResult(storage_.CreateDeepCopy(), CreateStatusCopy(status_));
}

ValueStore::WriteResult TestingValueStore::Set(
    WriteOptions options, const std::string& key, const base::Value& value) {
  base::DictionaryValue settings;
  settings.SetKey(key, value.Clone());
  return Set(options, settings);
}

ValueStore::WriteResult TestingValueStore::Set(
    WriteOptions options, const base::DictionaryValue& settings) {
  write_count_++;
  if (!status_.ok())
    return WriteResult(CreateStatusCopy(status_));

  ValueStoreChangeList changes;
  for (base::DictionaryValue::Iterator it(settings);
       !it.IsAtEnd(); it.Advance()) {
    base::Value* old_value = NULL;
    if (!storage_.GetWithoutPathExpansion(it.key(), &old_value) ||
        *old_value != it.value()) {
      changes.emplace_back(it.key(),
                           old_value
                               ? absl::optional<base::Value>(old_value->Clone())
                               : absl::nullopt,
                           it.value().Clone());
      storage_.SetKey(it.key(), it.value().Clone());
    }
  }
  return WriteResult(std::move(changes), CreateStatusCopy(status_));
}

ValueStore::WriteResult TestingValueStore::Remove(const std::string& key) {
  return Remove(std::vector<std::string>(1, key));
}

ValueStore::WriteResult TestingValueStore::Remove(
    const std::vector<std::string>& keys) {
  write_count_++;
  if (!status_.ok())
    return WriteResult(CreateStatusCopy(status_));

  ValueStoreChangeList changes;
  for (auto it = keys.cbegin(); it != keys.cend(); ++it) {
    std::unique_ptr<base::Value> old_value;
    if (storage_.RemoveWithoutPathExpansion(*it, &old_value)) {
      changes.emplace_back(*it, std::move(*old_value), absl::nullopt);
    }
  }
  return WriteResult(std::move(changes), CreateStatusCopy(status_));
}

ValueStore::WriteResult TestingValueStore::Clear() {
  std::vector<std::string> keys;
  for (base::DictionaryValue::Iterator it(storage_);
       !it.IsAtEnd(); it.Advance()) {
    keys.push_back(it.key());
  }
  return Remove(keys);
}
