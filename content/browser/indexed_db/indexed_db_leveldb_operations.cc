// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_leveldb_operations.h"

#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/values.h"
#include "components/services/storage/indexed_db/scopes/leveldb_scopes.h"
#include "components/services/storage/indexed_db/scopes/varint_coding.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_database.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_iterator.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_transaction.h"
#include "content/browser/indexed_db/indexed_db_data_format_version.h"
#include "content/browser/indexed_db/indexed_db_data_loss_info.h"
#include "content/browser/indexed_db/indexed_db_leveldb_env.h"
#include "content/browser/indexed_db/indexed_db_reporting.h"
#include "content/browser/indexed_db/indexed_db_tracing.h"
#include "storage/common/database/database_identifier.h"
#include "third_party/leveldatabase/env_chromium.h"

using base::StringPiece;
using blink::IndexedDBKeyPath;
using leveldb::Status;

namespace content {
namespace indexed_db {
namespace {
class LDBComparator : public leveldb::Comparator {
 public:
  LDBComparator() = default;
  ~LDBComparator() override = default;
  int Compare(const leveldb::Slice& a, const leveldb::Slice& b) const override {
    return content::Compare(leveldb_env::MakeStringPiece(a),
                            leveldb_env::MakeStringPiece(b),
                            false /*index_keys*/);
  }
  const char* Name() const override { return "idb_cmp1"; }
  void FindShortestSeparator(std::string* start,
                             const leveldb::Slice& limit) const override {}
  void FindShortSuccessor(std::string* key) const override {}
};
}  // namespace

const base::FilePath::CharType kBlobExtension[] = FILE_PATH_LITERAL(".blob");
const base::FilePath::CharType kIndexedDBExtension[] =
    FILE_PATH_LITERAL(".indexeddb");
const base::FilePath::CharType kLevelDBExtension[] =
    FILE_PATH_LITERAL(".leveldb");

// static
base::FilePath GetBlobStoreFileName(const url::Origin& origin) {
  std::string origin_id = storage::GetIdentifierFromOrigin(origin);
  return base::FilePath()
      .AppendASCII(origin_id)
      .AddExtension(kIndexedDBExtension)
      .AddExtension(kBlobExtension);
}

// static
base::FilePath GetLevelDBFileName(const url::Origin& origin) {
  std::string origin_id = storage::GetIdentifierFromOrigin(origin);
  return base::FilePath()
      .AppendASCII(origin_id)
      .AddExtension(kIndexedDBExtension)
      .AddExtension(kLevelDBExtension);
}

base::FilePath ComputeCorruptionFileName(const url::Origin& origin) {
  return GetLevelDBFileName(origin).Append(
      FILE_PATH_LITERAL("corruption_info.json"));
}

bool IsPathTooLong(const base::FilePath& leveldb_dir) {
  int limit = base::GetMaximumPathComponentLength(leveldb_dir.DirName());
  if (limit == -1) {
    DLOG(WARNING) << "GetMaximumPathComponentLength returned -1";
// In limited testing, ChromeOS returns 143, other OSes 255.
#if defined(OS_CHROMEOS)
    limit = 143;
#else
    limit = 255;
#endif
  }
  size_t component_length = leveldb_dir.BaseName().value().length();
  if (component_length > static_cast<uint32_t>(limit)) {
    DLOG(WARNING) << "Path component length (" << component_length
                  << ") exceeds maximum (" << limit
                  << ") allowed by this filesystem.";
    const int min = 140;
    const int max = 300;
    const int num_buckets = 12;
    base::UmaHistogramCustomCounts(
        "WebCore.IndexedDB.BackingStore.OverlyLargeOriginLength",
        component_length, min, max, num_buckets);
    return true;
  }
  return false;
}

std::string ReadCorruptionInfo(const base::FilePath& path_base,
                               const url::Origin& origin) {
  const base::FilePath info_path =
      path_base.Append(indexed_db::ComputeCorruptionFileName(origin));
  std::string message;
  if (IsPathTooLong(info_path))
    return message;

  const int64_t kMaxJsonLength = 4096;
  int64_t file_size = 0;
  if (!base::GetFileSize(info_path, &file_size))
    return message;
  if (!file_size || file_size > kMaxJsonLength) {
    base::DeleteFile(info_path, false);
    return message;
  }

  base::File file(info_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (file.IsValid()) {
    std::string input_js(file_size, '\0');
    if (file_size == file.Read(0, base::data(input_js), file_size)) {
      base::JSONReader reader;
      std::unique_ptr<base::DictionaryValue> val(
          base::DictionaryValue::From(reader.ReadToValueDeprecated(input_js)));
      if (val)
        val->GetString("message", &message);
    }
    file.Close();
  }

  base::DeleteFile(info_path, false);

  return message;
}

leveldb::Status InternalInconsistencyStatus() {
  return leveldb::Status::Corruption("Internal inconsistency");
}

leveldb::Status InvalidDBKeyStatus() {
  return leveldb::Status::InvalidArgument("Invalid database key ID");
}

leveldb::Status IOErrorStatus() {
  return leveldb::Status::IOError("IO Error");
}

leveldb::Status PutBool(TransactionalLevelDBTransaction* transaction,
                        const StringPiece& key,
                        bool value) {
  std::string buffer;
  EncodeBool(value, &buffer);
  return transaction->Put(key, &buffer);
}

template <typename DBOrTransaction>
Status GetVarInt(DBOrTransaction* db,
                 const StringPiece& key,
                 int64_t* found_int,
                 bool* found) {
  std::string result;
  Status s = db->Get(key, &result, found);
  if (!s.ok())
    return s;
  if (!*found)
    return Status::OK();
  StringPiece slice(result);
  if (DecodeVarInt(&slice, found_int) && slice.empty())
    return s;
  return InternalInconsistencyStatus();
}
template Status GetVarInt<TransactionalLevelDBTransaction>(
    TransactionalLevelDBTransaction* txn,
    const StringPiece& key,
    int64_t* found_int,
    bool* found);
template Status GetVarInt<TransactionalLevelDBDatabase>(
    TransactionalLevelDBDatabase* db,
    const StringPiece& key,
    int64_t* found_int,
    bool* found);

template <typename TransactionOrWriteBatch>
leveldb::Status PutVarInt(TransactionOrWriteBatch* transaction_or_write_batch,
                          const StringPiece& key,
                          int64_t value) {
  std::string buffer;
  EncodeVarInt(value, &buffer);
  return PutValue(transaction_or_write_batch, key, &buffer);
}
template leveldb::Status PutVarInt<TransactionalLevelDBTransaction>(
    TransactionalLevelDBTransaction* transaction,
    const StringPiece& key,
    int64_t value);
template leveldb::Status PutVarInt<LevelDBDirectTransaction>(
    LevelDBDirectTransaction* transaction,
    const StringPiece& key,
    int64_t value);
template leveldb::Status PutVarInt<LevelDBWriteBatch>(
    LevelDBWriteBatch* transaction,
    const StringPiece& key,
    int64_t value);

template <typename DBOrTransaction>
Status GetString(DBOrTransaction* db,
                 const StringPiece& key,
                 base::string16* found_string,
                 bool* found) {
  std::string result;
  *found = false;
  Status s = db->Get(key, &result, found);
  if (!s.ok())
    return s;
  if (!*found)
    return Status::OK();
  StringPiece slice(result);
  if (DecodeString(&slice, found_string) && slice.empty())
    return s;
  return InternalInconsistencyStatus();
}

template Status GetString<TransactionalLevelDBTransaction>(
    TransactionalLevelDBTransaction* txn,
    const StringPiece& key,
    base::string16* found_string,
    bool* found);
template Status GetString<TransactionalLevelDBDatabase>(
    TransactionalLevelDBDatabase* db,
    const StringPiece& key,
    base::string16* found_string,
    bool* found);

leveldb::Status PutString(TransactionalLevelDBTransaction* transaction,
                          const StringPiece& key,
                          const base::string16& value) {
  std::string buffer;
  EncodeString(value, &buffer);
  return transaction->Put(key, &buffer);
}

leveldb::Status PutIDBKeyPath(TransactionalLevelDBTransaction* transaction,
                              const StringPiece& key,
                              const IndexedDBKeyPath& value) {
  std::string buffer;
  EncodeIDBKeyPath(value, &buffer);
  return transaction->Put(key, &buffer);
}

template <typename DBOrTransaction>
Status GetMaxObjectStoreId(DBOrTransaction* db,
                           int64_t database_id,
                           int64_t* max_object_store_id) {
  const std::string max_object_store_id_key = DatabaseMetaDataKey::Encode(
      database_id, DatabaseMetaDataKey::MAX_OBJECT_STORE_ID);
  *max_object_store_id = -1;
  bool found = false;
  Status s = indexed_db::GetInt(db, max_object_store_id_key,
                                max_object_store_id, &found);
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
    INTERNAL_READ_ERROR_UNTESTED(SET_MAX_OBJECT_STORE_ID);
    return s;
  }

  if (object_store_id <= max_object_store_id) {
    INTERNAL_CONSISTENCY_ERROR(SET_MAX_OBJECT_STORE_ID);
    return indexed_db::InternalInconsistencyStatus();
  }
  return indexed_db::PutInt(transaction, max_object_store_id_key,
                            object_store_id);
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
    INTERNAL_READ_ERROR_UNTESTED(GET_NEW_VERSION_NUMBER);
    return s;
  }
  if (!found)
    last_version = 0;

  DCHECK_GE(last_version, 0);

  int64_t version = last_version + 1;
  s = PutInt(transaction, last_version_key, version);
  if (!s.ok()) {
    INTERNAL_READ_ERROR_UNTESTED(GET_NEW_VERSION_NUMBER);
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
    INTERNAL_READ_ERROR_UNTESTED(SET_MAX_INDEX_ID);
    return s;
  }
  if (!found)
    max_index_id = kMinimumIndexId;

  if (index_id <= max_index_id) {
    INTERNAL_CONSISTENCY_ERROR_UNTESTED(SET_MAX_INDEX_ID);
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

  Status s = transaction->Get(key, &data, exists);
  if (!s.ok()) {
    INTERNAL_READ_ERROR_UNTESTED(VERSION_EXISTS);
    return s;
  }
  if (!*exists)
    return s;

  StringPiece slice(data);
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
  Status s = indexed_db::GetInt(transaction, MaxDatabaseIdKey::Encode(),
                                &max_database_id, &found);
  if (!s.ok()) {
    INTERNAL_READ_ERROR_UNTESTED(GET_NEW_DATABASE_ID);
    return s;
  }
  if (!found)
    max_database_id = 0;

  DCHECK_GE(max_database_id, 0);

  int64_t database_id = max_database_id + 1;
  s = indexed_db::PutInt(transaction, MaxDatabaseIdKey::Encode(), database_id);
  if (!s.ok()) {
    INTERNAL_READ_ERROR_UNTESTED(GET_NEW_DATABASE_ID);
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

  StringPiece slice(it->Key());
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

  StringPiece slice(it->Key());
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
  std::unique_ptr<TransactionalLevelDBIterator> it =
      transaction->CreateIterator();
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
    *found_key = it->Key().as_string();

    // There can be several index keys that compare equal. We want the last one.
    *s = it->Next();
  } while (s->ok() && it->IsValid() && !CompareIndexKeys(it->Key(), target));

  return true;
}

bool GetBlobKeyGeneratorCurrentNumber(
    LevelDBDirectTransaction* leveldb_transaction,
    int64_t database_id,
    int64_t* blob_key_generator_current_number) {
  const std::string key_gen_key = DatabaseMetaDataKey::Encode(
      database_id, DatabaseMetaDataKey::BLOB_KEY_GENERATOR_CURRENT_NUMBER);

  // Default to initial number if not found.
  int64_t cur_number = DatabaseMetaDataKey::kBlobKeyGeneratorInitialNumber;
  std::string data;

  bool found = false;
  bool ok = leveldb_transaction->Get(key_gen_key, &data, &found).ok();
  if (!ok) {
    INTERNAL_READ_ERROR_UNTESTED(GET_BLOB_KEY_GENERATOR_CURRENT_NUMBER);
    return false;
  }
  if (found) {
    StringPiece slice(data);
    if (!DecodeVarInt(&slice, &cur_number) || !slice.empty() ||
        !DatabaseMetaDataKey::IsValidBlobKey(cur_number)) {
      INTERNAL_READ_ERROR_UNTESTED(GET_BLOB_KEY_GENERATOR_CURRENT_NUMBER);
      return false;
    }
  }
  *blob_key_generator_current_number = cur_number;
  return true;
}

bool UpdateBlobKeyGeneratorCurrentNumber(
    LevelDBDirectTransaction* leveldb_transaction,
    int64_t database_id,
    int64_t blob_key_generator_current_number) {
#if DCHECK_IS_ON()
  int64_t old_number;
  if (!GetBlobKeyGeneratorCurrentNumber(leveldb_transaction, database_id,
                                        &old_number))
    return false;
  DCHECK_LT(old_number, blob_key_generator_current_number);
#endif
  DCHECK(
      DatabaseMetaDataKey::IsValidBlobKey(blob_key_generator_current_number));
  const std::string key = DatabaseMetaDataKey::Encode(
      database_id, DatabaseMetaDataKey::BLOB_KEY_GENERATOR_CURRENT_NUMBER);

  leveldb::Status s =
      PutVarInt(leveldb_transaction, key, blob_key_generator_current_number);
  return s.ok();
}

Status GetEarliestSweepTime(TransactionalLevelDBDatabase* db,
                            base::Time* earliest_sweep) {
  const std::string earliest_sweep_time_key = EarliestSweepKey::Encode();
  *earliest_sweep = base::Time();
  bool found = false;
  int64_t time_micros = 0;
  Status s =
      indexed_db::GetInt(db, earliest_sweep_time_key, &time_micros, &found);
  if (!s.ok())
    return s;
  if (!found)
    time_micros = 0;

  DCHECK_GE(time_micros, 0);
  *earliest_sweep += base::TimeDelta::FromMicroseconds(time_micros);

  return s;
}

template leveldb::Status SetEarliestSweepTime<TransactionalLevelDBTransaction>(
    TransactionalLevelDBTransaction* db,
    base::Time earliest_sweep);
template leveldb::Status SetEarliestSweepTime<LevelDBDirectTransaction>(
    LevelDBDirectTransaction* db,
    base::Time earliest_sweep);

template <typename Transaction>
leveldb::Status SetEarliestSweepTime(Transaction* txn,
                                     base::Time earliest_sweep) {
  const std::string earliest_sweep_time_key = EarliestSweepKey::Encode();
  int64_t time_micros = (earliest_sweep - base::Time()).InMicroseconds();
  return indexed_db::PutInt(txn, earliest_sweep_time_key, time_micros);
}

const leveldb::Comparator* GetDefaultLevelDBComparator() {
  static const base::NoDestructor<LDBComparator> ldb_comparator;
  return ldb_comparator.get();
}

}  // namespace indexed_db
}  // namespace content
