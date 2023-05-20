// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/extras/sqlite/sqlite_persistent_shared_dictionary_store.h"

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/pickle.h"
#include "base/task/sequenced_task_runner.h"
#include "net/base/network_isolation_key.h"
#include "net/extras/shared_dictionary/shared_dictionary_storage_isolation_key.h"
#include "net/extras/sqlite/sqlite_persistent_store_backend_base.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace net {

namespace {

constexpr char kHistogramTag[] = "SharedDictionary";

constexpr char kTableName[] = "dictionaries";

// The key for storing the total dictionary size in MetaTable. It is utilized
// when determining whether cache eviction needs to be performed. We store it as
// metadata because calculating the total size is an expensive operation.
constexpr char kTotalDictSizeKey[] = "total_dict_size";

const int kCurrentVersionNumber = 1;
const int kCompatibleVersionNumber = 1;

bool CreateV1Schema(sql::Database* db, sql::MetaTable* meta_table) {
  CHECK(!db->DoesTableExist(kTableName));

  static constexpr char kCreateTableQuery[] =
      // clang-format off
      "CREATE TABLE dictionaries("
          "id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,"
          "frame_origin TEXT NOT NULL,"
          "top_frame_site TEXT NOT NULL,"
          "host TEXT NOT NULL,"
          "match TEXT NOT NULL,"
          "url TEXT NOT NULL,"
          "res_time INTEGER NOT NULL,"
          "exp_time INTEGER NOT NULL,"
          "last_used_time INTEGER NOT NULL,"
          "size INTEGER NOT NULL,"
          "sha256 BLOB NOT NULL,"
          "token_high INTEGER NOT NULL,"
          "token_low INTEGER NOT NULL)";
  // clang-format on

  static constexpr char kCreateUniqueIndexQuery[] =
      // clang-format off
      "CREATE UNIQUE INDEX unique_index ON dictionaries("
          "frame_origin,"
          "top_frame_site,"
          "host,"
          "match)";
  // clang-format on

  // This index is used for GetDictionaries().
  static constexpr char kCreateIsolationIndexQuery[] =
      // clang-format off
      "CREATE INDEX isolation_index ON dictionaries("
          "frame_origin,"
          "top_frame_site)";
  // clang-format on

  // This index will be used when implementing garbage collection logic of
  // SharedDictionaryDiskCache.
  static constexpr char kCreateTokenIndexQuery[] =
      // clang-format off
      "CREATE INDEX token_index ON dictionaries("
          "token_high, token_low)";
  // clang-format on

  // This index will be used when implementing clearing expired dictionary
  // logic.
  static constexpr char kCreateExpirationTimeIndexQuery[] =
      // clang-format off
      "CREATE INDEX exp_time_index ON dictionaries("
          "exp_time)";
  // clang-format on

  // This index will be used when implementing clearing dictionary logic which
  // will be called from BrowsingDataRemover.
  static constexpr char kCreateLastUsedTimeIndexQuery[] =
      // clang-format off
      "CREATE INDEX last_used_time_index ON dictionaries("
          "last_used_time)";
  // clang-format on

  if (!db->Execute(kCreateTableQuery) ||
      !db->Execute(kCreateUniqueIndexQuery) ||
      !db->Execute(kCreateIsolationIndexQuery) ||
      !db->Execute(kCreateTokenIndexQuery) ||
      !db->Execute(kCreateExpirationTimeIndexQuery) ||
      !db->Execute(kCreateLastUsedTimeIndexQuery) ||
      !meta_table->SetValue(kTotalDictSizeKey, 0)) {
    return false;
  }
  return true;
}

absl::optional<SHA256HashValue> ToSHA256HashValue(
    base::span<const uint8_t> sha256_bytes) {
  SHA256HashValue sha256_hash;
  if (sha256_bytes.size() != sizeof(sha256_hash.data)) {
    return absl::nullopt;
  }
  memcpy(sha256_hash.data, sha256_bytes.data(), sha256_bytes.size());
  return sha256_hash;
}

absl::optional<base::UnguessableToken> ToUnguessableToken(int64_t token_high,
                                                          int64_t token_low) {
  // There is no `sql::Statement::ColumnUint64()` method. So we cast to
  // uint64_t.
  return base::UnguessableToken::Deserialize(static_cast<uint64_t>(token_high),
                                             static_cast<uint64_t>(token_low));
}

template <typename ResultType>
base::OnceCallback<void(ResultType)> WrapCallbackWithWeakPtrCheck(
    base::WeakPtr<SQLitePersistentSharedDictionaryStore> weak_ptr,
    base::OnceCallback<void(ResultType)> callback) {
  return base::BindOnce(
      [](base::WeakPtr<SQLitePersistentSharedDictionaryStore> weak_ptr,
         base::OnceCallback<void(ResultType)> callback, ResultType result) {
        if (!weak_ptr) {
          return;
        }
        std::move(callback).Run(std::move(result));
      },
      std::move(weak_ptr), std::move(callback));
}

}  // namespace

class SQLitePersistentSharedDictionaryStore::Backend
    : public SQLitePersistentStoreBackendBase {
 public:
  Backend(
      const base::FilePath& path,
      const scoped_refptr<base::SequencedTaskRunner>& client_task_runner,
      const scoped_refptr<base::SequencedTaskRunner>& background_task_runner)
      : SQLitePersistentStoreBackendBase(path,
                                         kHistogramTag,
                                         kCurrentVersionNumber,
                                         kCompatibleVersionNumber,
                                         background_task_runner,
                                         client_task_runner,
                                         /*enable_exclusive_access=*/false) {}

  Backend(const Backend&) = delete;
  Backend& operator=(const Backend&) = delete;

#define DEFINE_CROSS_SEQUENCE_CALL_METHOD(Name)                              \
  template <typename ResultType, typename... Args>                           \
  void Name(base::OnceCallback<void(ResultType)> callback, Args&&... args) { \
    CHECK(client_task_runner()->RunsTasksInCurrentSequence());               \
    PostBackgroundTask(                                                      \
        FROM_HERE,                                                           \
        base::BindOnce(                                                      \
            [](scoped_refptr<Backend> backend,                               \
               base::OnceCallback<void(ResultType)> callback,                \
               Args&&... args) {                                             \
              backend->PostClientTask(                                       \
                  FROM_HERE,                                                 \
                  base::BindOnce(                                            \
                      std::move(callback),                                   \
                      backend->Name##Impl(std::forward<Args>(args)...)));    \
            },                                                               \
            scoped_refptr<Backend>(this), std::move(callback),               \
            std::forward<Args>(args)...));                                   \
  }

  // The following methods call *Impl() method in the background task runner,
  // and call the passed `callback` with the result in the client task runner.
  DEFINE_CROSS_SEQUENCE_CALL_METHOD(GetTotalDictionarySize)
  DEFINE_CROSS_SEQUENCE_CALL_METHOD(RegisterDictionary)
  DEFINE_CROSS_SEQUENCE_CALL_METHOD(GetDictionaries)
  DEFINE_CROSS_SEQUENCE_CALL_METHOD(GetAllDictionaries)
  DEFINE_CROSS_SEQUENCE_CALL_METHOD(ClearAllDictionaries)
#undef DEFINE_CROSS_SEQUENCE_CALL_METHOD

 private:
  ~Backend() override = default;

  // Gets the total dictionary size in MetaTable.
  base::expected<uint64_t, Error> GetTotalDictionarySizeImpl();

  RegisterDictionaryResultOrError RegisterDictionaryImpl(
      const SharedDictionaryStorageIsolationKey& isolation_key,
      const SharedDictionaryInfo& dictionary_info);
  DictionaryListOrError GetDictionariesImpl(
      const SharedDictionaryStorageIsolationKey& isolation_key);
  DictionaryMapOrError GetAllDictionariesImpl();
  Error ClearAllDictionariesImpl();

  // If a matching dictionary exists, populates 'size_out' and
  // 'disk_cache_key_out' with the dictionary's respective values and returns
  // true. Otherwise returns false.
  bool GetExistingDictionarySizeAndDiskCacheKeyToken(
      const SharedDictionaryStorageIsolationKey& isolation_key,
      const url::SchemeHostPort& host,
      const std::string& match,
      int64_t* size_out,
      absl::optional<base::UnguessableToken>* disk_cache_key_out);

  // Updates the total dictionary size in MetaTable by `size_delta` and returns
  // the updated total dictionary size.
  Error UpdateTotalDictionarySizeInMetaTable(
      int64_t size_delta,
      uint64_t* total_dictionary_size_out);

  // SQLitePersistentStoreBackendBase implementation
  bool CreateDatabaseSchema() override;
  absl::optional<int> DoMigrateDatabaseSchema() override;
  void DoCommit() override;
};

bool SQLitePersistentSharedDictionaryStore::Backend::CreateDatabaseSchema() {
  if (!db()->DoesTableExist(kTableName) &&
      !CreateV1Schema(db(), meta_table())) {
    return false;
  }
  return true;
}

absl::optional<int>
SQLitePersistentSharedDictionaryStore::Backend::DoMigrateDatabaseSchema() {
  int cur_version = meta_table()->GetVersionNumber();
  if (cur_version != kCurrentVersionNumber) {
    return absl::nullopt;
  }

  // Future database upgrade statements go here.

  return absl::make_optional(cur_version);
}

void SQLitePersistentSharedDictionaryStore::Backend::DoCommit() {
  // Currently we don't batch queries because RegisterDictionary() is not
  // expected to be called so frequently.
  // TODO(crbug.com/1413922): Consider using batching when we implement a method
  // for updating `last_used_time`.
}

base::expected<uint64_t, SQLitePersistentSharedDictionaryStore::Error>
SQLitePersistentSharedDictionaryStore::Backend::GetTotalDictionarySizeImpl() {
  CHECK(background_task_runner()->RunsTasksInCurrentSequence());
  if (!InitializeDatabase()) {
    return base::unexpected(Error::kFailedToInitializeDatabase);
  }

  int64_t unsigned_total_dictionary_size = 0;
  if (!meta_table()->GetValue(kTotalDictSizeKey,
                              &unsigned_total_dictionary_size)) {
    return base::unexpected(Error::kFailedToGetTotalDictSize);
  }

  // There is no `sql::Statement::ColumnUint64()` method. So we cast to
  // uint64_t.
  return base::ok(static_cast<uint64_t>(unsigned_total_dictionary_size));
}

SQLitePersistentSharedDictionaryStore::RegisterDictionaryResultOrError
SQLitePersistentSharedDictionaryStore::Backend::RegisterDictionaryImpl(
    const SharedDictionaryStorageIsolationKey& isolation_key,
    const SharedDictionaryInfo& dictionary_info) {
  CHECK(background_task_runner()->RunsTasksInCurrentSequence());
  if (!InitializeDatabase()) {
    return base::unexpected(Error::kFailedToInitializeDatabase);
  }

  sql::Transaction transaction(db());
  if (!transaction.Begin()) {
    return base::unexpected(Error::kFailedToBeginTransaction);
  }

  int64_t size_of_removed_dict = 0;
  absl::optional<base::UnguessableToken> disk_cache_key_token_of_removed_dict;
  int64_t size_delta = dictionary_info.size();
  if (GetExistingDictionarySizeAndDiskCacheKeyToken(
          isolation_key, url::SchemeHostPort(dictionary_info.url()),
          dictionary_info.match(), &size_of_removed_dict,
          &disk_cache_key_token_of_removed_dict)) {
    size_delta -= size_of_removed_dict;
  }

  static constexpr char kQuery[] =
      // clang-format off
      "INSERT OR REPLACE INTO dictionaries("
          "frame_origin,"
          "top_frame_site,"
          "host,"
          "match,"
          "url,"
          "res_time,"
          "exp_time,"
          "last_used_time,"
          "size,"
          "sha256,"
          "token_high,"
          "token_low) "
          "VALUES(?,?,?,?,?,?,?,?,?,?,?,?)";
  // clang-format on

  if (!db()->IsSQLValid(kQuery)) {
    return base::unexpected(Error::kInvalidSql);
  }

  sql::Statement statement(db()->GetCachedStatement(SQL_FROM_HERE, kQuery));
  statement.BindString(0, isolation_key.frame_origin().Serialize());
  statement.BindString(1, isolation_key.top_frame_site().Serialize());
  statement.BindString(2,
                       url::SchemeHostPort(dictionary_info.url()).Serialize());
  statement.BindString(3, dictionary_info.match());
  statement.BindString(4, dictionary_info.url().spec());
  statement.BindTime(5, dictionary_info.response_time());
  statement.BindTime(6, dictionary_info.GetExpirationTime());
  statement.BindTime(7, dictionary_info.last_used_time());
  statement.BindInt64(8, dictionary_info.size());
  statement.BindBlob(9, base::make_span(dictionary_info.hash().data));
  // There is no `sql::Statement::BindUint64()` method. So we cast to int64_t.
  int64_t token_high = static_cast<int64_t>(
      dictionary_info.disk_cache_key_token().GetHighForSerialization());
  int64_t token_low = static_cast<int64_t>(
      dictionary_info.disk_cache_key_token().GetLowForSerialization());
  statement.BindInt64(10, token_high);
  statement.BindInt64(11, token_low);

  if (!statement.Run()) {
    return base::unexpected(Error::kFailedToExecuteSql);
  }
  int64_t id = db()->GetLastInsertRowId();

  uint64_t total_dictionary_size = 0;
  Error error =
      UpdateTotalDictionarySizeInMetaTable(size_delta, &total_dictionary_size);
  if (error != Error::kOk) {
    return base::unexpected(error);
  }

  if (!transaction.Commit()) {
    return base::unexpected(Error::kFailedToCommitTransaction);
  }
  return base::ok(
      RegisterDictionaryResult{.primary_key_in_database = id,
                               .disk_cache_key_token_to_be_removed =
                                   disk_cache_key_token_of_removed_dict,
                               .total_dictionary_size = total_dictionary_size});
}

SQLitePersistentSharedDictionaryStore::DictionaryListOrError
SQLitePersistentSharedDictionaryStore::Backend::GetDictionariesImpl(
    const SharedDictionaryStorageIsolationKey& isolation_key) {
  CHECK(background_task_runner()->RunsTasksInCurrentSequence());
  std::vector<SharedDictionaryInfo> result;

  if (!InitializeDatabase()) {
    return base::unexpected(Error::kFailedToInitializeDatabase);
  }

  static constexpr char kQuery[] =
      // clang-format off
      "SELECT "
          "id,"
          "match,"
          "url,"
          "res_time,"
          "exp_time,"
          "last_used_time,"
          "size,"
          "sha256,"
          "token_high,"
          "token_low FROM dictionaries "
          "WHERE frame_origin=? AND top_frame_site=? "
          "ORDER BY id";
  // clang-format on

  if (!db()->IsSQLValid(kQuery)) {
    return base::unexpected(Error::kInvalidSql);
  }

  sql::Statement statement(db()->GetCachedStatement(SQL_FROM_HERE, kQuery));
  statement.BindString(0, isolation_key.frame_origin().Serialize());
  statement.BindString(1, isolation_key.top_frame_site().Serialize());

  while (statement.Step()) {
    const int64_t primary_key_in_database = statement.ColumnInt64(0);
    const std::string match = statement.ColumnString(1);
    const std::string url_string = statement.ColumnString(2);
    const base::Time response_time = statement.ColumnTime(3);
    const base::Time expiration_time = statement.ColumnTime(4);
    const base::Time last_used_time = statement.ColumnTime(5);
    const size_t size = statement.ColumnInt64(6);

    absl::optional<SHA256HashValue> sha256_hash =
        ToSHA256HashValue(statement.ColumnBlob(7));
    if (!sha256_hash) {
      LOG(WARNING) << "Invalid hash";
      continue;
    }
    absl::optional<base::UnguessableToken> disk_cache_key_token =
        ToUnguessableToken(statement.ColumnInt64(8), statement.ColumnInt64(9));
    if (!disk_cache_key_token) {
      LOG(WARNING) << "Invalid token";
      continue;
    }
    result.emplace_back(GURL(url_string), response_time,
                        expiration_time - response_time, match, last_used_time,
                        size, *sha256_hash, *disk_cache_key_token,
                        primary_key_in_database);
  }
  return base::ok(std::move(result));
}

SQLitePersistentSharedDictionaryStore::DictionaryMapOrError
SQLitePersistentSharedDictionaryStore::Backend::GetAllDictionariesImpl() {
  CHECK(background_task_runner()->RunsTasksInCurrentSequence());
  if (!InitializeDatabase()) {
    return base::unexpected(Error::kFailedToInitializeDatabase);
  }

  static constexpr char kQuery[] =
      // clang-format off
      "SELECT "
          "id,"
          "frame_origin,"
          "top_frame_site,"
          "match,"
          "url,"
          "res_time,"
          "exp_time,"
          "last_used_time,"
          "size,"
          "sha256,"
          "token_high,"
          "token_low FROM dictionaries "
          "ORDER BY id";
  // clang-format on

  if (!db()->IsSQLValid(kQuery)) {
    return base::unexpected(Error::kInvalidSql);
  }

  std::map<SharedDictionaryStorageIsolationKey,
           std::vector<SharedDictionaryInfo>>
      result;
  sql::Statement statement(db()->GetCachedStatement(SQL_FROM_HERE, kQuery));

  while (statement.Step()) {
    const int64_t primary_key_in_database = statement.ColumnInt64(0);
    const std::string frame_origin_string = statement.ColumnString(1);
    const std::string top_frame_site_string = statement.ColumnString(2);
    const std::string match = statement.ColumnString(3);
    const std::string url_string = statement.ColumnString(4);
    const base::Time response_time = statement.ColumnTime(5);
    const base::Time expiration_time = statement.ColumnTime(6);
    const base::Time last_used_time = statement.ColumnTime(7);
    const size_t size = statement.ColumnInt64(8);

    absl::optional<SHA256HashValue> sha256_hash =
        ToSHA256HashValue(statement.ColumnBlob(9));
    if (!sha256_hash) {
      LOG(WARNING) << "Invalid hash";
      continue;
    }

    absl::optional<base::UnguessableToken> disk_cache_key_token =
        ToUnguessableToken(statement.ColumnInt64(10),
                           statement.ColumnInt64(11));
    if (!disk_cache_key_token) {
      LOG(WARNING) << "Invalid token";
      continue;
    }

    url::Origin frame_origin = url::Origin::Create(GURL(frame_origin_string));
    net::SchemefulSite top_frame_site =
        net::SchemefulSite(GURL(top_frame_site_string));

    result[SharedDictionaryStorageIsolationKey(frame_origin, top_frame_site)]
        .emplace_back(GURL(url_string), response_time,
                      expiration_time - response_time, match, last_used_time,
                      size, *sha256_hash, *disk_cache_key_token,
                      primary_key_in_database);
  }
  return base::ok(std::move(result));
}

SQLitePersistentSharedDictionaryStore::Error
SQLitePersistentSharedDictionaryStore::Backend::ClearAllDictionariesImpl() {
  CHECK(background_task_runner()->RunsTasksInCurrentSequence());

  if (!InitializeDatabase()) {
    return Error::kFailedToInitializeDatabase;
  }

  sql::Transaction transaction(db());
  if (!transaction.Begin()) {
    return Error::kFailedToBeginTransaction;
  }

  static constexpr char kQuery[] = "DELETE FROM dictionaries";
  if (!db()->IsSQLValid(kQuery)) {
    return Error::kInvalidSql;
  }
  sql::Statement statement(db()->GetCachedStatement(SQL_FROM_HERE, kQuery));
  if (!statement.Run()) {
    return Error::kFailedToExecuteSql;
  }

  if (!meta_table()->SetValue(kTotalDictSizeKey, 0)) {
    return Error::kFailedToSetTotalDictSize;
  }

  if (!transaction.Commit()) {
    return Error::kFailedToCommitTransaction;
  }
  return Error::kOk;
}

bool SQLitePersistentSharedDictionaryStore::Backend::
    GetExistingDictionarySizeAndDiskCacheKeyToken(
        const SharedDictionaryStorageIsolationKey& isolation_key,
        const url::SchemeHostPort& host,
        const std::string& match,
        int64_t* size_out,
        absl::optional<base::UnguessableToken>* disk_cache_key_out) {
  CHECK(background_task_runner()->RunsTasksInCurrentSequence());

  static constexpr char kQuery[] =
      // clang-format off
      "SELECT "
          "size,"
          "token_high,"
          "token_low FROM dictionaries "
          "WHERE frame_origin=? AND top_frame_site=? AND host=? AND match=? "
          "ORDER BY id";
  // clang-format on

  if (!db()->IsSQLValid(kQuery)) {
    return false;
  }
  sql::Statement statement(db()->GetCachedStatement(SQL_FROM_HERE, kQuery));
  statement.BindString(0, isolation_key.frame_origin().Serialize());
  statement.BindString(1, isolation_key.top_frame_site().Serialize());
  statement.BindString(2, host.Serialize());
  statement.BindString(3, match);

  if (statement.Step()) {
    *size_out = statement.ColumnInt64(0);
    *disk_cache_key_out =
        ToUnguessableToken(statement.ColumnInt64(1), statement.ColumnInt64(2));
    return true;
  }
  return false;
}

SQLitePersistentSharedDictionaryStore::Error
SQLitePersistentSharedDictionaryStore::Backend::
    UpdateTotalDictionarySizeInMetaTable(int64_t size_delta,
                                         uint64_t* total_dictionary_size_out) {
  CHECK(background_task_runner()->RunsTasksInCurrentSequence());
  base::expected<uint64_t, Error> total_dictionary_size_or_error =
      GetTotalDictionarySizeImpl();
  if (!total_dictionary_size_or_error.has_value()) {
    return total_dictionary_size_or_error.error();
  }

  base::CheckedNumeric<uint64_t> checked_total_dictionary_size =
      total_dictionary_size_or_error.value();
  checked_total_dictionary_size += size_delta;
  if (!checked_total_dictionary_size.IsValid()) {
    LOG(ERROR) << "Invalid total_dict_size detected.";
    return Error::kInvalidTotalDictSize;
  }
  *total_dictionary_size_out = checked_total_dictionary_size.ValueOrDie();
  if (!meta_table()->SetValue(kTotalDictSizeKey, *total_dictionary_size_out)) {
    return Error::kFailedToSetTotalDictSize;
  }
  return Error::kOk;
}

SQLitePersistentSharedDictionaryStore::SQLitePersistentSharedDictionaryStore(
    const base::FilePath& path,
    const scoped_refptr<base::SequencedTaskRunner>& client_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& background_task_runner)
    : backend_(base::MakeRefCounted<Backend>(path,
                                             client_task_runner,
                                             background_task_runner)) {}

SQLitePersistentSharedDictionaryStore::
    ~SQLitePersistentSharedDictionaryStore() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_->Close();
}

void SQLitePersistentSharedDictionaryStore::GetTotalDictionarySize(
    base::OnceCallback<void(base::expected<uint64_t, Error>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_->GetTotalDictionarySize(
      WrapCallbackWithWeakPtrCheck(GetWeakPtr(), std::move(callback)));
}

void SQLitePersistentSharedDictionaryStore::RegisterDictionary(
    const SharedDictionaryStorageIsolationKey& isolation_key,
    SharedDictionaryInfo dictionary_info,
    base::OnceCallback<void(RegisterDictionaryResultOrError)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_->RegisterDictionary(
      WrapCallbackWithWeakPtrCheck(GetWeakPtr(), std::move(callback)),
      isolation_key, std::move(dictionary_info));
}

void SQLitePersistentSharedDictionaryStore::GetDictionaries(
    const SharedDictionaryStorageIsolationKey& isolation_key,
    base::OnceCallback<void(DictionaryListOrError)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_->GetDictionaries(
      WrapCallbackWithWeakPtrCheck(GetWeakPtr(), std::move(callback)),
      isolation_key);
}

void SQLitePersistentSharedDictionaryStore::GetAllDictionaries(
    base::OnceCallback<void(DictionaryMapOrError)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_->GetAllDictionaries(
      WrapCallbackWithWeakPtrCheck(GetWeakPtr(), std::move(callback)));
}

void SQLitePersistentSharedDictionaryStore::ClearAllDictionaries(
    base::OnceCallback<void(Error)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_->ClearAllDictionaries(
      WrapCallbackWithWeakPtrCheck(GetWeakPtr(), std::move(callback)));
}

base::WeakPtr<SQLitePersistentSharedDictionaryStore>
SQLitePersistentSharedDictionaryStore::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_factory_.GetWeakPtr();
}

}  // namespace net
