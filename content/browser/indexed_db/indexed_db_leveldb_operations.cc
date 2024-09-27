// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_leveldb_operations.h"

#include <string_view>

#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/no_destructor.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/services/storage/indexed_db/scopes/leveldb_scopes.h"
#include "components/services/storage/indexed_db/scopes/varint_coding.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_database.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_iterator.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_transaction.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/cpp/constants.h"
#include "content/browser/indexed_db/file_path_util.h"
#include "content/browser/indexed_db/indexed_db_data_format_version.h"
#include "content/browser/indexed_db/indexed_db_data_loss_info.h"
#include "content/browser/indexed_db/indexed_db_reporting.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

using blink::IndexedDBKeyPath;

namespace content::indexed_db {

namespace {

class LDBComparator : public leveldb::Comparator {
 public:
  LDBComparator() = default;
  ~LDBComparator() override = default;
  int Compare(const leveldb::Slice& a, const leveldb::Slice& b) const override {
    return ::content::indexed_db::Compare(leveldb_env::MakeStringView(a),
                                          leveldb_env::MakeStringView(b),
                                          /*index_keys=*/false);
  }
  const char* Name() const override { return "idb_cmp1"; }
  void FindShortestSeparator(std::string* start,
                             const leveldb::Slice& limit) const override {}
  void FindShortSuccessor(std::string* key) const override {}
};

}  // namespace

base::FilePath ComputeCorruptionFileName(
    const storage::BucketLocator& bucket_locator) {
  return GetLevelDBFileName(bucket_locator)
      .Append(FILE_PATH_LITERAL("corruption_info.json"));
}

std::string ReadCorruptionInfo(const base::FilePath& path_base,
                               const storage::BucketLocator& bucket_locator) {
  const base::FilePath info_path =
      path_base.Append(ComputeCorruptionFileName(bucket_locator));
  std::string message;
  if (IsPathTooLong(info_path)) {
    return message;
  }

  const int64_t kMaxJsonLength = 4096;

  base::File::Info file_info;
  if (!base::GetFileInfo(info_path, &file_info)) {
    return message;
  }
  if (!file_info.size || file_info.size > kMaxJsonLength) {
    base::DeleteFile(info_path);
    return message;
  }

  base::File file(info_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (file.IsValid()) {
    std::string input_js(file_info.size, '\0');
    if (file_info.size ==
        UNSAFE_TODO(file.Read(0, std::data(input_js), file_info.size))) {
      std::optional<base::Value> val = base::JSONReader::Read(input_js);
      if (val && val->is_dict()) {
        std::string* s = val->GetDict().FindString("message");
        if (s) {
          message = *s;
        }
      }
    }
    file.Close();
  }

  base::DeleteFile(info_path);

  return message;
}

Status InternalInconsistencyStatus() {
  return Status::Corruption("Internal inconsistency");
}

Status InvalidDBKeyStatus() {
  return Status::InvalidArgument("Invalid database key ID");
}

Status IOErrorStatus() {
  return Status::IOError("IO Error");
}

Status PutBool(TransactionalLevelDBTransaction* transaction,
               std::string_view key,
               bool value) {
  std::string buffer;
  EncodeBool(value, &buffer);
  return Status(transaction->Put(key, &buffer));
}

template <typename DBOrTransaction>
Status GetVarInt(DBOrTransaction* db,
                 std::string_view key,
                 int64_t* found_int,
                 bool* found) {
  std::string result;
  Status s(db->Get(key, &result, found));
  if (!s.ok())
    return s;
  if (!*found)
    return Status::OK();
  std::string_view slice(result);
  if (DecodeVarInt(&slice, found_int) && slice.empty())
    return s;
  return InternalInconsistencyStatus();
}
template Status GetVarInt<TransactionalLevelDBTransaction>(
    TransactionalLevelDBTransaction* txn,
    std::string_view key,
    int64_t* found_int,
    bool* found);
template Status GetVarInt<TransactionalLevelDBDatabase>(
    TransactionalLevelDBDatabase* db,
    std::string_view key,
    int64_t* found_int,
    bool* found);

template <typename TransactionOrWriteBatch>
Status PutVarInt(TransactionOrWriteBatch* transaction_or_write_batch,
                 std::string_view key,
                 int64_t value) {
  std::string buffer;
  EncodeVarInt(value, &buffer);
  return PutValue(transaction_or_write_batch, key, &buffer);
}
template Status PutVarInt<TransactionalLevelDBTransaction>(
    TransactionalLevelDBTransaction* transaction,
    std::string_view key,
    int64_t value);
template Status PutVarInt<LevelDBDirectTransaction>(
    LevelDBDirectTransaction* transaction,
    std::string_view key,
    int64_t value);
template Status PutVarInt<LevelDBWriteBatch>(LevelDBWriteBatch* transaction,
                                             std::string_view key,
                                             int64_t value);

template <typename DBOrTransaction>
Status GetString(DBOrTransaction* db,
                 std::string_view key,
                 std::u16string* found_string,
                 bool* found) {
  std::string result;
  *found = false;
  Status s(db->Get(key, &result, found));
  if (!s.ok())
    return s;
  if (!*found)
    return Status::OK();
  std::string_view slice(result);
  if (DecodeString(&slice, found_string) && slice.empty())
    return s;
  return InternalInconsistencyStatus();
}

template Status GetString<TransactionalLevelDBTransaction>(
    TransactionalLevelDBTransaction* txn,
    std::string_view key,
    std::u16string* found_string,
    bool* found);
template Status GetString<TransactionalLevelDBDatabase>(
    TransactionalLevelDBDatabase* db,
    std::string_view key,
    std::u16string* found_string,
    bool* found);

Status PutString(TransactionalLevelDBTransaction* transaction,
                 std::string_view key,
                 const std::u16string& value) {
  std::string buffer;
  EncodeString(value, &buffer);
  return Status(transaction->Put(key, &buffer));
}

Status PutIDBKeyPath(TransactionalLevelDBTransaction* transaction,
                     std::string_view key,
                     const IndexedDBKeyPath& value) {
  std::string buffer;
  EncodeIDBKeyPath(value, &buffer);
  return Status(transaction->Put(key, &buffer));
}

template <typename DBOrTransaction>
Status GetMaxObjectStoreId(DBOrTransaction* db,
                           int64_t database_id,
                           int64_t* max_object_store_id) {
  const std::string max_object_store_id_key = DatabaseMetaDataKey::Encode(
      database_id, DatabaseMetaDataKey::MAX_OBJECT_STORE_ID);
  *max_object_store_id = -1;
  bool found = false;
  Status s = GetInt(db, max_object_store_id_key, max_object_store_id, &found);
  if (!s.ok())
    return s;
  if (!found)
    *max_object_store_id = 0;

  DCHECK_GE(*max_object_store_id, 0);
  return s;
}

template Status GetMaxObjectStoreId<TransactionalLevelDBTransaction>(
    TransactionalLevelDBTransaction* db,
    int64_t database_id,
    int64_t* max_object_store_id);
template Status GetMaxObjectStoreId<TransactionalLevelDBDatabase>(
    TransactionalLevelDBDatabase* db,
    int64_t database_id,
    int64_t* max_object_store_id);

Status SetMaxObjectStoreId(TransactionalLevelDBTransaction* transaction,
                           int64_t database_id,
                           int64_t object_store_id) {
  const std::string max_object_store_id_key = DatabaseMetaDataKey::Encode(
      database_id, DatabaseMetaDataKey::MAX_OBJECT_STORE_ID);
  int64_t max_object_store_id = -1;
  bool found = false;
  Status s = GetInt(transaction, max_object_store_id_key, &max_object_store_id,
                    &found);
  if (!s.ok())
    return s;
  if (!found)
    max_object_store_id = 0;

  DCHECK_GE(max_object_store_id, 0);
  if (!s.ok()) {
    INTERNAL_READ_ERROR(SET_MAX_OBJECT_STORE_ID);
    return s;
  }

  if (object_store_id <= max_object_store_id) {
    INTERNAL_CONSISTENCY_ERROR(SET_MAX_OBJECT_STORE_ID);
    return InternalInconsistencyStatus();
  }
  return PutInt(transaction, max_object_store_id_key, object_store_id);
}

Status GetNewVersionNumber(TransactionalLevelDBTransaction* transaction,
                           int64_t database_id,
                           int64_t object_store_id,
                           int64_t* new_version_number) {
  const std::string last_version_key = ObjectStoreMetaDataKey::Encode(
      database_id, object_store_id, ObjectStoreMetaDataKey::LAST_VERSION);

  *new_version_number = -1;
  int64_t last_version = -1;
  bool found = false;
  Status s = GetInt(transaction, last_version_key, &last_version, &found);
  if (!s.ok()) {
    INTERNAL_READ_ERROR(GET_NEW_VERSION_NUMBER);
    return s;
  }
  if (!found)
    last_version = 0;

  DCHECK_GE(last_version, 0);

  int64_t version = last_version + 1;
  s = PutInt(transaction, last_version_key, version);
  if (!s.ok()) {
    INTERNAL_READ_ERROR(GET_NEW_VERSION_NUMBER);
    return s;
  }

  // TODO(jsbell): Think about how we want to handle the overflow scenario.
  DCHECK(version > last_version);

  *new_version_number = version;
  return s;
}

Status SetMaxIndexId(TransactionalLevelDBTransaction* transaction,
                     int64_t database_id,
                     int64_t object_store_id,
                     int64_t index_id) {
  int64_t max_index_id = -1;
  const std::string max_index_id_key = ObjectStoreMetaDataKey::Encode(
      database_id, object_store_id, ObjectStoreMetaDataKey::MAX_INDEX_ID);
  bool found = false;
  Status s = GetInt(transaction, max_index_id_key, &max_index_id, &found);
  if (!s.ok()) {
    INTERNAL_READ_ERROR(SET_MAX_INDEX_ID);
    return s;
  }
  if (!found)
    max_index_id = kMinimumIndexId;

  if (index_id <= max_index_id) {
    INTERNAL_CONSISTENCY_ERROR(SET_MAX_INDEX_ID);
    return InternalInconsistencyStatus();
  }

  return PutInt(transaction, max_index_id_key, index_id);
}

Status VersionExists(TransactionalLevelDBTransaction* transaction,
                     int64_t database_id,
                     int64_t object_store_id,
                     int64_t version,
                     const std::string& encoded_primary_key,
                     bool* exists) {
  const std::string key =
      ExistsEntryKey::Encode(database_id, object_store_id, encoded_primary_key);
  std::string data;

  Status s(transaction->Get(key, &data, exists));
  if (!s.ok()) {
    INTERNAL_READ_ERROR(VERSION_EXISTS);
    return s;
  }
  if (!*exists)
    return s;

  std::string_view slice(data);
  int64_t decoded;
  if (!DecodeInt(&slice, &decoded) || !slice.empty())
    return InternalInconsistencyStatus();
  *exists = (decoded == version);
  return s;
}

template Status GetNewDatabaseId<LevelDBDirectTransaction>(
    LevelDBDirectTransaction* transaction,
    int64_t* new_id);

template Status GetNewDatabaseId<TransactionalLevelDBTransaction>(
    TransactionalLevelDBTransaction* transaction,
    int64_t* new_id);

template <typename Transaction>
Status GetNewDatabaseId(Transaction* transaction, int64_t* new_id) {
  *new_id = -1;
  int64_t max_database_id = -1;
  bool found = false;
  Status s =
      GetInt(transaction, MaxDatabaseIdKey::Encode(), &max_database_id, &found);
  if (!s.ok()) {
    INTERNAL_READ_ERROR(GET_NEW_DATABASE_ID);
    return s;
  }
  if (!found)
    max_database_id = 0;

  DCHECK_GE(max_database_id, 0);

  int64_t database_id = max_database_id + 1;
  s = PutInt(transaction, MaxDatabaseIdKey::Encode(), database_id);
  if (!s.ok()) {
    INTERNAL_READ_ERROR(GET_NEW_DATABASE_ID);
    return s;
  }
  *new_id = database_id;
  return Status::OK();
}

bool CheckObjectStoreAndMetaDataType(const TransactionalLevelDBIterator* it,
                                     const std::string& stop_key,
                                     int64_t object_store_id,
                                     int64_t meta_data_type) {
  if (!it->IsValid() || CompareKeys(it->Key(), stop_key) >= 0)
    return false;

  std::string_view slice(it->Key());
  ObjectStoreMetaDataKey meta_data_key;
  bool ok =
      ObjectStoreMetaDataKey::Decode(&slice, &meta_data_key) && slice.empty();
  DCHECK(ok);
  if (meta_data_key.ObjectStoreId() != object_store_id)
    return false;
  if (meta_data_key.MetaDataType() != meta_data_type)
    return false;
  return ok;
}

bool CheckIndexAndMetaDataKey(const TransactionalLevelDBIterator* it,
                              const std::string& stop_key,
                              int64_t index_id,
                              unsigned char meta_data_type) {
  if (!it->IsValid() || CompareKeys(it->Key(), stop_key) >= 0)
    return false;

  std::string_view slice(it->Key());
  IndexMetaDataKey meta_data_key;
  bool ok = IndexMetaDataKey::Decode(&slice, &meta_data_key);
  DCHECK(ok);
  if (meta_data_key.IndexId() != index_id)
    return false;
  if (meta_data_key.meta_data_type() != meta_data_type)
    return false;
  return true;
}

bool FindGreatestKeyLessThanOrEqual(
    TransactionalLevelDBTransaction* transaction,
    const std::string& target,
    std::string* found_key,
    Status* s) {
  leveldb::Status status_out;
  std::unique_ptr<TransactionalLevelDBIterator> it =
      transaction->CreateIterator(status_out);
  *s = status_out;
  if (!s->ok()) {
    INTERNAL_WRITE_ERROR(CREATE_ITERATOR);
    return false;
  }

  *s = it->Seek(target);
  if (!s->ok())
    return false;

  if (!it->IsValid()) {
    *s = it->SeekToLast();
    if (!s->ok() || !it->IsValid())
      return false;
  }

  while (CompareIndexKeys(it->Key(), target) > 0) {
    *s = it->Prev();
    if (!s->ok() || !it->IsValid())
      return false;
  }

  do {
    *found_key = std::string(it->Key());

    // There can be several index keys that compare equal. We want the last one.
    *s = it->Next();
  } while (s->ok() && it->IsValid() && !CompareIndexKeys(it->Key(), target));

  return true;
}

bool GetBlobNumberGeneratorCurrentNumber(
    LevelDBDirectTransaction* leveldb_transaction,
    int64_t database_id,
    int64_t* blob_number_generator_current_number) {
  const std::string key_gen_key = DatabaseMetaDataKey::Encode(
      database_id, DatabaseMetaDataKey::BLOB_KEY_GENERATOR_CURRENT_NUMBER);

  // Default to initial number if not found.
  int64_t cur_number = DatabaseMetaDataKey::kBlobNumberGeneratorInitialNumber;
  std::string data;

  bool found = false;
  bool ok = leveldb_transaction->Get(key_gen_key, &data, &found).ok();
  if (!ok) {
    INTERNAL_READ_ERROR(GET_BLOB_KEY_GENERATOR_CURRENT_NUMBER);
    return false;
  }
  if (found) {
    std::string_view slice(data);
    if (!DecodeVarInt(&slice, &cur_number) || !slice.empty() ||
        !DatabaseMetaDataKey::IsValidBlobNumber(cur_number)) {
      INTERNAL_READ_ERROR(GET_BLOB_KEY_GENERATOR_CURRENT_NUMBER);
      return false;
    }
  }
  *blob_number_generator_current_number = cur_number;
  return true;
}

bool UpdateBlobNumberGeneratorCurrentNumber(
    LevelDBDirectTransaction* leveldb_transaction,
    int64_t database_id,
    int64_t blob_number_generator_current_number) {
#if DCHECK_IS_ON()
  int64_t old_number;
  if (!GetBlobNumberGeneratorCurrentNumber(leveldb_transaction, database_id,
                                           &old_number))
    return false;
  DCHECK_LT(old_number, blob_number_generator_current_number);
#endif
  DCHECK(DatabaseMetaDataKey::IsValidBlobNumber(
      blob_number_generator_current_number));
  const std::string key = DatabaseMetaDataKey::Encode(
      database_id, DatabaseMetaDataKey::BLOB_KEY_GENERATOR_CURRENT_NUMBER);

  Status s =
      PutVarInt(leveldb_transaction, key, blob_number_generator_current_number);
  return s.ok();
}

Status GetEarliestSweepTime(TransactionalLevelDBDatabase* db,
                            base::Time* earliest_sweep) {
  const std::string earliest_sweep_time_key = EarliestSweepKey::Encode();
  *earliest_sweep = base::Time();
  bool found = false;
  int64_t time_micros = 0;
  Status s = GetInt(db, earliest_sweep_time_key, &time_micros, &found);
  if (!s.ok())
    return s;
  if (!found)
    time_micros = 0;

  DCHECK_GE(time_micros, 0);
  *earliest_sweep += base::Microseconds(time_micros);

  return s;
}

template Status SetEarliestSweepTime<TransactionalLevelDBTransaction>(
    TransactionalLevelDBTransaction* db,
    base::Time earliest_sweep);
template Status SetEarliestSweepTime<LevelDBDirectTransaction>(
    LevelDBDirectTransaction* db,
    base::Time earliest_sweep);

template <typename Transaction>
Status SetEarliestSweepTime(Transaction* txn, base::Time earliest_sweep) {
  const std::string earliest_sweep_time_key = EarliestSweepKey::Encode();
  int64_t time_micros = (earliest_sweep - base::Time()).InMicroseconds();
  return PutInt(txn, earliest_sweep_time_key, time_micros);
}

Status GetEarliestCompactionTime(TransactionalLevelDBDatabase* db,
                                 base::Time* earliest_compaction) {
  const std::string earliest_compaction_time_key =
      EarliestCompactionKey::Encode();
  *earliest_compaction = base::Time();
  bool found = false;
  int64_t time_micros = 0;
  Status s = GetInt(db, earliest_compaction_time_key, &time_micros, &found);
  if (!s.ok())
    return s;
  if (!found)
    time_micros = 0;

  DCHECK_GE(time_micros, 0);
  *earliest_compaction += base::Microseconds(time_micros);

  return s;
}

template Status SetEarliestCompactionTime<TransactionalLevelDBTransaction>(
    TransactionalLevelDBTransaction* db,
    base::Time earliest_compaction);
template Status SetEarliestCompactionTime<LevelDBDirectTransaction>(
    LevelDBDirectTransaction* db,
    base::Time earliest_compaction);

template <typename Transaction>
Status SetEarliestCompactionTime(Transaction* txn,
                                 base::Time earliest_compaction) {
  const std::string earliest_compaction_time_key =
      EarliestCompactionKey::Encode();
  int64_t time_micros = (earliest_compaction - base::Time()).InMicroseconds();
  return PutInt(txn, earliest_compaction_time_key, time_micros);
}

const leveldb::Comparator* GetDefaultLevelDBComparator() {
  static const base::NoDestructor<LDBComparator> ldb_comparator;
  return ldb_comparator.get();
}

}  // namespace content::indexed_db
