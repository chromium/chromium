// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/value_store/leveldb_scoped_database.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace {

// Note: Changing the delimiter will change the database schema.
const char kKeyDelimiter = ':';  // delimits scope and key
const char kCannotSerialize[] = "Cannot serialize value to JSON";
const char kInvalidJson[] = "Invalid JSON";
const char kInvalidScope[] = "Invalid scope";

}  // namespace

bool LeveldbScopedDatabase::SplitKey(const std::string& full_key,
                                     std::string* scope,
                                     std::string* key) {
  size_t pos = full_key.find(kKeyDelimiter);
  if (pos == std::string::npos)
    return false;
  if (pos == 0)
    return false;
  *scope = full_key.substr(0, pos);
  *key = full_key.substr(pos + 1);
  return true;
}

bool LeveldbScopedDatabase::CreateKey(const std::string& scope,
                                      const std::string& key,
                                      std::string* scoped_key) {
  if (scope.empty() || scope.find(kKeyDelimiter) != std::string::npos)
    return false;
  *scoped_key = scope + kKeyDelimiter + key;
  return true;
}

LeveldbScopedDatabase::LeveldbScopedDatabase(const std::string& uma_client_name,
                                             const base::FilePath& path)
    : LazyLevelDb(uma_client_name, path) {}

LeveldbScopedDatabase::~LeveldbScopedDatabase() {}

ValueStore::Status LeveldbScopedDatabase::Read(
    const std::string& scope,
    const std::string& key,
    base::Optional<base::Value>* value) {
  ValueStore::Status status = EnsureDbIsOpen();
  if (!status.ok())
    return status;
  std::string scoped_key;
  if (!CreateKey(scope, key, &scoped_key))
    return ValueStore::Status(ValueStore::OTHER_ERROR, kInvalidScope);

  return LazyLevelDb::Read(scoped_key, value);
}

ValueStore::Status LeveldbScopedDatabase::Read(const std::string& scope,
                                               base::DictionaryValue* values) {
  ValueStore::Status status = EnsureDbIsOpen();
  if (!status.ok())
    return status;

  std::string prefix;
  if (!CreateKey(scope, "", &prefix))
    return ValueStore::Status(ValueStore::OTHER_ERROR, kInvalidScope);

  std::unique_ptr<leveldb::Iterator> it(db()->NewIterator(read_options()));

  std::unique_ptr<base::DictionaryValue> settings(new base::DictionaryValue());

  for (it->Seek(prefix); it->Valid() && it->key().starts_with(prefix);
       it->Next()) {
    leveldb::Slice descoped_key(it->key());
    descoped_key.remove_prefix(prefix.size());
    base::Optional<base::Value> value = base::JSONReader::Read(
        base::StringPiece(it->value().data(), it->value().size()));
    if (!value) {
      return ValueStore::Status(ValueStore::CORRUPTION,
                                LazyLevelDb::Delete(it->key().ToString()).ok()
                                    ? ValueStore::VALUE_RESTORE_DELETE_SUCCESS
                                    : ValueStore::VALUE_RESTORE_DELETE_FAILURE,
                                kInvalidJson);
    }
    values->SetKey(descoped_key.ToString(), std::move(*value));
  }

  return status;
}

ValueStore::Status LeveldbScopedDatabase::Write(const std::string& scope,
                                                const std::string& key,
                                                const base::Value& value) {
  ValueStore::Status status = EnsureDbIsOpen();
  if (!status.ok())
    return status;

  leveldb::WriteBatch batch;
  status = AddToWriteBatch(&batch, scope, key, value);
  if (!status.ok())
    return status;
  return ToValueStoreError(db()->Write(write_options(), &batch));
}

ValueStore::Status LeveldbScopedDatabase::Write(
    const std::string& scope,
    const base::DictionaryValue& values) {
  ValueStore::Status status = EnsureDbIsOpen();
  if (!status.ok())
    return status;

  leveldb::WriteBatch batch;
  for (base::DictionaryValue::Iterator it(values); !it.IsAtEnd();
       it.Advance()) {
    status = AddToWriteBatch(&batch, scope, it.key(), it.value());
    if (!status.ok())
      return status;
  }

  return ToValueStoreError(db()->Write(write_options(), &batch));
}

ValueStore::Status LeveldbScopedDatabase::DeleteValues(
    const std::string& scope,
    const std::vector<std::string>& keys) {
  ValueStore::Status status = EnsureDbIsOpen();
  if (!status.ok())
    return status;

  leveldb::WriteBatch batch;
  std::string scoped_key;
  for (const auto& key : keys) {
    if (!CreateKey(scope, key, &scoped_key))
      return ValueStore::Status(ValueStore::OTHER_ERROR, kInvalidScope);
    batch.Delete(scoped_key);
  }

  return ToValueStoreError(db()->Write(write_options(), &batch));
}

ValueStore::Status LeveldbScopedDatabase::AddToWriteBatch(
    leveldb::WriteBatch* batch,
    const std::string& scope,
    const std::string& key,
    const base::Value& value) {
  std::string scoped_key;
  if (!CreateKey(scope, key, &scoped_key))
    return ValueStore::Status(ValueStore::OTHER_ERROR, kInvalidScope);

  std::string value_as_json;
  if (!base::JSONWriter::Write(value, &value_as_json))
    return ValueStore::Status(ValueStore::OTHER_ERROR, kCannotSerialize);

  batch->Put(scoped_key, value_as_json);
  return ValueStore::Status();
}
