// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/extras/sqlite/sqlite_persistent_shared_dictionary_store.h"

#include "base/containers/span.h"
#include "base/debug/dump_without_crashing.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_functions.h"
#include "base/pickle.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected_macros.h"
#include "net/base/network_isolation_key.h"
#include "net/extras/sqlite/sqlite_persistent_store_backend_base.h"
#include "net/shared_dictionary/shared_dictionary_isolation_key.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace net {

namespace {

constexpr char kHistogramTag[] = "SharedDictionary";

constexpr char kHistogramPrefix[] = "Net.SharedDictionaryStore.";

constexpr char kTableName[] = "dictionaries";

// The key for storing the total dictionary size in MetaTable. It is utilized
// when determining whether cache eviction needs to be performed. We store it as
// metadata because calculating the total size is an expensive operation.
constexpr char kTotalDictSizeKey[] = "total_dict_size";

const int kCurrentVersionNumber = 3;
const int kCompatibleVersionNumber = 3;

bool CreateV3Schema(sql::Database* db, sql::MetaTable* meta_table) {
  CHECK(!db->DoesTableExist(kTableName));

  static constexpr char kCreateTableQuery[] =
      // clang-format off
      "CREATE TABLE dictionaries("
          "primary_key INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,"
          "frame_origin TEXT NOT NULL,"
          "top_frame_site TEXT NOT NULL,"
          "host TEXT NOT NULL,"
          "match TEXT NOT NULL,"
          "match_dest TEXT NOT NULL,"
          "id TEXT NOT NULL,"
          "url TEXT NOT NULL,"
          "last_fetch_time INTEGER NOT NULL,"
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
          "match,"
          "match_dest)";
  // clang-format on

  // This index is used for the size and count limitation per top_frame_site.
  static constexpr char kCreateTopFrameSiteIndexQuery[] =
      // clang-format off
      "CREATE INDEX top_frame_site_index ON dictionaries("
          "top_frame_site)";
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

  sql::Transaction transaction(db);
  if (!transaction.Begin() || !db->Execute(kCreateTableQuery) ||
      !db->Execute(kCreateUniqueIndexQuery) ||
      !db->Execute(kCreateTopFrameSiteIndexQuery) ||
      !db->Execute(kCreateIsolationIndexQuery) ||
      !db->Execute(kCreateTokenIndexQuery) ||
      !db->Execute(kCreateExpirationTimeIndexQuery) ||
      !db->Execute(kCreateLastUsedTimeIndexQuery) ||
      !meta_table->SetValue(kTotalDictSizeKey, 0) || !transaction.Commit()) {
    return false;
  }
  return true;
}

std::optional<SHA256HashValue> ToSHA256HashValue(
    base::span<const uint8_t> sha256_bytes) {
  SHA256HashValue sha256_hash;
  if (sha256_bytes.size() != sizeof(sha256_hash.data)) {
    return std::nullopt;
  }
  memcpy(sha256_hash.data, sha256_bytes.data(), sha256_bytes.size());
  return sha256_hash;
}

std::optional<base::UnguessableToken> ToUnguessableToken(int64_t token_high,
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

void RecordErrorHistogram(const char* method_name,
                          SQLitePersistentSharedDictionaryStore::Error error) {
  base::UmaHistogramEnumeration(
      base::StrCat({kHistogramPrefix, method_name, ".Error"}), error);
}

template <typename ResultType>
void RecordErrorHistogram(
    const char* method_name,
    base::expected<ResultType, SQLitePersistentSharedDictionaryStore::Error>
        result) {
  RecordErrorHistogram(method_name,
                       result.has_value()
                           ? SQLitePersistentSharedDictionaryStore::Error::kOk
                           : result.error());
}

}  // namespace

SQLitePersistentSharedDictionaryStore::RegisterDictionaryResult::
    RegisterDictionaryResult(
        int64_t primary_key_in_database,
        std::optional<base::UnguessableToken> replaced_disk_cache_key_token,
        std::set<base::UnguessableToken> evicted_disk_cache_key_tokens,
        uint64_t total_dictionary_size,
        uint64_t total_dictionary_count)
    : primary_key_in_database_(primary_key_in_database),
      replaced_disk_cache_key_token_(std::move(replaced_disk_cache_key_token)),
      evicted_disk_cache_key_tokens_(std::move(evicted_disk_cache_key_tokens)),
      total_dictionary_size_(total_dictionary_size),
      total_dictionary_count_(total_dictionary_count) {}

SQLitePersistentSharedDictionaryStore::RegisterDictionaryResult::
    ~RegisterDictionaryResult() = default;

SQLitePersistentSharedDictionaryStore::RegisterDictionaryResult::
    RegisterDictionaryResult(const RegisterDictionaryResult& other) = default;

SQLitePersistentSharedDictionaryStore::RegisterDictionaryResult::
    RegisterDictionaryResult(RegisterDictionaryResult&& other) = default;

SQLitePersistentSharedDictionaryStore::RegisterDictionaryResult&
SQLitePersistentSharedDictionaryStore::RegisterDictionaryResult::operator=(
    const RegisterDictionaryResult& other) = default;

SQLitePersistentSharedDictionaryStore::RegisterDictionaryResult&
SQLitePersistentSharedDictionaryStore::RegisterDictionaryResult::operator=(
    RegisterDictionaryResult&& other) = default;

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

#define DEFINE_CROSS_SEQUENCE_CALL_METHOD(Name)                               \
  template <typename ResultType, typename... Args>                            \
  void Name(base::OnceCallback<void(ResultType)> callback, Args&&... args) {  \
    CHECK(client_task_runner()->RunsTasksInCurrentSequence());                \
    PostBackgroundTask(                                                       \
        FROM_HERE,                                                            \
        base::BindOnce(                                                       \
            [](scoped_refptr<Backend> backend,                                \
               base::OnceCallback<void(ResultType)> callback,                 \
               Args&&... args) {                                              \
              auto result = backend->Name##Impl(std::forward<Args>(args)...); \
              RecordErrorHistogram(#Name, result);                            \
              backend->PostClientTask(                                        \
                  FROM_HERE,                                                  \
                  base::BindOnce(std::move(callback), std::move(result)));    \
            },                                                                \
            scoped_refptr<Backend>(this), std::move(callback),                \
            std::forward<Args>(args)...));                                    \
  }

  // The following methods call *Impl() method in the background task runner,
  // and call the passed `callback` with the result in the client task runner.
  DEFINE_CROSS_SEQUENCE_CALL_METHOD(GetTotalDictionarySize)
  DEFINE_CROSS_SEQUENCE_CALL_METHOD(RegisterDictionary)
  DEFINE_CROSS_SEQUENCE_CALL_METHOD(GetDictionaries)
  DEFINE_CROSS_SEQUENCE_CALL_METHOD(GetAllDictionaries)
  DEFINE_CROSS_SEQUENCE_CALL_METHOD(GetUsageInfo)
  DEFINE_CROSS_SEQUENCE_CALL_METHOD(GetOriginsBetween)
  DEFINE_CROSS_SEQUENCE_CALL_METHOD(ClearAllDictionaries)
  DEFINE_CROSS_SEQUENCE_CALL_METHOD(ClearDictionaries)
  DEFINE_CROSS_SEQUENCE_CALL_METHOD(ClearDictionariesForIsolationKey)
  DEFINE_CROSS_SEQUENCE_CALL_METHOD(DeleteExpiredDictionaries)
  DEFINE_CROSS_SEQUENCE_CALL_METHOD(ProcessEviction)
  DEFINE_CROSS_SEQUENCE_CALL_METHOD(GetAllDiskCacheKeyTokens)
  DEFINE_CROSS_SEQUENCE_CALL_METHOD(DeleteDictionariesByDiskCacheKeyTokens)
  DEFINE_CROSS_SEQUENCE_CALL_METHOD(UpdateDictionaryLastFetchTime)
#undef DEFINE_CROSS_SEQUENCE_CALL_METHOD

  void UpdateDictionaryLastUsedTime(int64_t primary_key_in_database,
                                    base::Time last_used_time);

 private:
  ~Backend() override = default;

  // Gets the total dictionary size in MetaTable.
  SizeOrError GetTotalDictionarySizeImpl();

  RegisterDictionaryResultOrError RegisterDictionaryImpl(
      const SharedDictionaryIsolationKey& isolation_key,
      const SharedDictionaryInfo& dictionary_info,
      uint64_t max_size_per_site,
      uint64_t max_count_per_site);
  DictionaryListOrError GetDictionariesImpl(
      const SharedDictionaryIsolationKey& isolation_key);
  DictionaryMapOrError GetAllDictionariesImpl();
  UsageInfoOrError GetUsageInfoImpl();
  OriginListOrError GetOriginsBetweenImpl(const base::Time start_time,
                                          const base::Time end_time);
  UnguessableTokenSetOrError ClearAllDictionariesImpl();
  UnguessableTokenSetOrError ClearDictionariesImpl(
      base::Time start_time,
      base::Time end_time,
      base::RepeatingCallback<bool(const GURL&)> url_matcher);
  UnguessableTokenSetOrError ClearDictionariesForIsolationKeyImpl(
      const SharedDictionaryIsolationKey& isolation_key);
  UnguessableTokenSetOrError DeleteExpiredDictionariesImpl(base::Time now);
  UnguessableTokenSetOrError ProcessEvictionImpl(uint64_t cache_max_size,
                                                 uint64_t size_low_watermark,
                                                 uint64_t cache_max_count,
                                                 uint64_t count_low_watermark);
  UnguessableTokenSetOrError GetAllDiskCacheKeyTokensImpl();
  Error DeleteDictionariesByDiskCacheKeyTokensImpl(
      const std::set<base::UnguessableToken>& disk_cache_key_tokens);
  Error UpdateDictionaryLastFetchTimeImpl(const int64_t primary_key_in_database,
                                          const base::Time last_fetch_time);

  // If a matching dictionary exists, populates 'size_out' and
  // 'disk_cache_key_out' with the dictionary's respective values and returns
  // true. Otherwise returns false.
  bool GetExistingDictionarySizeAndDiskCacheKeyToken(
      const SharedDictionaryIsolationKey& isolation_key,
      const url::SchemeHostPort& host,
      const std::string& match,
      const std::string& match_dest,
      int64_t* size_out,
      std::optional<base::UnguessableToken>* disk_cache_key_out);

  // Updates the total dictionary size in MetaTable by `size_delta` and returns
  // the updated total dictionary size.
  Error UpdateTotalDictionarySizeInMetaTable(
      int64_t size_delta,
      uint64_t* total_dictionary_size_out);

  // Gets the total dictionary count.
  SizeOrError GetTotalDictionaryCount();

  // SQLitePersistentStoreBackendBase implementation
  bool CreateDatabaseSchema() override;
  std::optional<int> DoMigrateDatabaseSchema() override;
  void DoCommit() override;

  // Commits the last used time update.
  Error CommitDictionaryLastUsedTimeUpdate(int64_t primary_key_in_database,
                                           base::Time last_used_time);

  // Selects dictionaries which `res_time` is between `start_time` and
  // `end_time`. And fills their primary keys and tokens and total size.
  Error SelectMatchingDictionaries(
      base::Time start_time,
      base::Time end_time,
      std::vector<int64_t>* primary_keys_out,
      std::vector<base::UnguessableToken>* tokens_out,
      int64_t* total_size_out);
  // Selects dictionaries which `res_time` is between `start_time` and
  // `end_time`, and which `frame_origin` or `top_frame_site` or `host` matches
  // with `url_matcher`. And fills their primary keys and tokens and total size.
  Error SelectMatchingDictionariesWithUrlMatcher(
      base::Time start_time,
      base::Time end_time,
      base::RepeatingCallback<bool(const GURL&)> url_matcher,
      std::vector<int64_t>* primary_keys_out,
      std::vector<base::UnguessableToken>* tokens_out,
      int64_t* total_size_out);
  // Selects dictionaries in order of `last_used_time` if the total size of all
  // dictionaries exceeds `cache_max_size` or the total dictionary count exceeds
  // `cache_max_count` until the total size reaches `size_low_watermark` and the
  // total count reaches `count_low_watermark`, and fills their primary keys and
  // tokens and total size. If `cache_max_size` is zero, the size limitation is
  // ignored.
  Error SelectEvictionCandidates(
      uint64_t cache_max_size,
      uint64_t size_low_watermark,
      uint64_t cache_max_count,
      uint64_t count_low_watermark,
      std::vector<int64_t>* primary_keys_out,
      std::vector<base::UnguessableToken>* tokens_out,
      int64_t* total_size_after_eviction_out);
  // Deletes a dictionary with `primary_key`.
  Error DeleteDictionaryByPrimaryKey(int64_t primary_key);
  // Deletes a dictionary with `disk_cache_key_token` and returns the deleted
  // dictionarie's size.
  SizeOrError DeleteDictionaryByDiskCacheToken(
      const base::UnguessableToken& disk_cache_key_token);

  Error MaybeEvictDictionariesForPerSiteLimit(
      const SchemefulSite& top_frame_site,
      uint64_t max_size_per_site,
      uint64_t max_count_per_site,
      std::vector<base::UnguessableToken>* evicted_disk_cache_key_tokens,
      uint64_t* total_dictionary_size_out);
  SizeOrError GetDictionaryCountPerSite(const SchemefulSite& top_frame_site);
  SizeOrError GetDictionarySizePerSite(const SchemefulSite& top_frame_site);
  Error SelectCandidatesForPerSiteEviction(
      const SchemefulSite& top_frame_site,
      uint64_t max_size_per_site,
      uint64_t max_count_per_site,
      std::vector<int64_t>* primary_keys_out,
      std::vector<base::UnguessableToken>* tokens_out,
      int64_t* total_candidate_dictionary_size_out);

  // Total number of pending last used time update operations (may not match the
  // size of `pending_last_used_time_updates_`, due to operation coalescing).
  size_t num_pending_ GUARDED_BY(lock_) = 0;
  std::map<int64_t, base::Time> pending_last_used_time_updates_
      GUARDED_BY(lock_);
  // Protects `num_pending_`, and `pending_last_used_time_updates_`.
  mutable base::Lock lock_;
};

bool SQLitePersistentSharedDictionaryStore::Backend::CreateDatabaseSchema() {
  if (!db()->DoesTableExist(kTableName) &&
      !CreateV3Schema(db(), meta_table())) {
    return false;
  }
  return true;
}

std::optional<int>
SQLitePersistentSharedDictionaryStore::Backend::DoMigrateDatabaseSchema() {
  int cur_version = meta_table()->GetVersionNumber();
  if (cur_version == 1 || cur_version == 2) {
    sql::Transaction transaction(db());
    if (!transaction.Begin() ||
        !db()->Execute("DROP TABLE IF EXISTS dictionaries") ||
        !meta_table()->DeleteKey(kTotalDictSizeKey)) {
      return std::nullopt;
    }
    // The version 1 is used during the Origin Trial period (M119-M122).
    // The version 2 is used during the Origin Trial period (M123-M124).
    // We don't need to migrate the data from version 1 and 2.
    cur_version = 3;
    if (!meta_table()->SetVersionNumber(cur_version) ||
        !meta_table()->SetCompatibleVersionNumber(
            std::min(cur_version, kCompatibleVersionNumber)) ||
        !transaction.Commit()) {
      return std::nullopt;
    }
  }

  // Future database upgrade statements go here.

  return std::make_optional(cur_version);
}

void SQLitePersistentSharedDictionaryStore::Backend::DoCommit() {
  std::map<int64_t, base::Time> pending_last_used_time_updates;
  {
    base::AutoLock locked(lock_);
    pending_last_used_time_updates_.swap(pending_last_used_time_updates);
    num_pending_ = 0;
  }
  if (!db() || pending_last_used_time_updates.empty()) {
    return;
  }

  sql::Transaction transaction(db());
  if (!transaction.Begin()) {
    return;
  }
  for (const auto& it : pending_last_used_time_updates) {
    if (CommitDictionaryLastUsedTimeUpdate(it.first, it.second) != Error::kOk) {
      return;
    }
  }
  transaction.Commit();
}

SQLitePersistentSharedDictionaryStore::Error
SQLitePersistentSharedDictionaryStore::Backend::
    CommitDictionaryLastUsedTimeUpdate(int64_t primary_key_in_database,
                                       base::Time last_used_time) {
  CHECK(background_task_runner()->RunsTasksInCurrentSequence());
  if (!InitializeDatabase()) {
    return Error::kFailedToInitializeDatabase;
  }
  static constexpr char kQuery[] =
      "UPDATE dictionaries SET last_used_time=? WHERE primary_key=?";

  if (!db()->IsSQLValid(kQuery)) {
    return Error::kInvalidSql;
  }
  sql::Statement statement(db()->GetCachedStatement(SQL_FROM_HERE, kQuery));
  statement.BindTime(0, last_used_time);
  statement.BindInt64(1, primary_key_in_database);
  if (!statement.Run()) {
    return Error::kFailedToExecuteSql;
  }
  return Error::kOk;
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
    const SharedDictionaryIsolationKey& isolation_key,
    const SharedDictionaryInfo& dictionary_info,
    uint64_t max_size_per_site,
    uint64_t max_count_per_site) {
  CHECK(background_task_runner()->RunsTasksInCurrentSequence());
  CHECK_NE(0u, max_count_per_site);
  if (max_size_per_site != 0 && dictionary_info.size() > max_size_per_site) {
    return base::unexpected(Error::kTooBigDictionary);
  }

  if (!InitializeDatabase()) {
    return base::unexpected(Error::kFailedToInitializeDatabase);
  }

  // Commit `pending_last_used_time_updates_`.
  DoCommit();

  sql::Transaction transaction(db());
  if (!transaction.Begin()) {
    return base::unexpected(Error::kFailedToBeginTransaction);
  }

  int64_t size_of_removed_dict = 0;
  std::optional<base::UnguessableToken> replaced_disk_cache_key_token;
  int64_t size_delta = dictionary_info.size();
  if (GetExistingDictionarySizeAndDiskCacheKeyToken(
          isolation_key, url::SchemeHostPort(dictionary_info.url()),
          dictionary_info.match(), dictionary_info.match_dest_string(),
          &size_of_removed_dict, &replaced_disk_cache_key_token)) {
    size_delta -= size_of_removed_dict;
  }

  static constexpr char kQuery[] =
      // clang-format off
      "INSERT OR REPLACE INTO dictionaries("
          "frame_origin,"
          "top_frame_site,"
          "host,"
          "match,"
          "match_dest,"
          "id,"
          "url,"
          "last_fetch_time,"
          "res_time,"
          "exp_time,"
          "last_used_time,"
          "size,"
          "sha256,"
          "token_high,"
          "token_low) "
          "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)";
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
  statement.BindString(4, dictionary_info.match_dest_string());
  statement.BindString(5, dictionary_info.id());
  statement.BindString(6, dictionary_info.url().spec());
  statement.BindTime(7, dictionary_info.last_fetch_time());
  statement.BindTime(8, dictionary_info.response_time());
  statement.BindTime(9, dictionary_info.GetExpirationTime());
  statement.BindTime(10, dictionary_info.last_used_time());
  statement.BindInt64(11, dictionary_info.size());
  statement.BindBlob(12, base::make_span(dictionary_info.hash().data));
  // There is no `sql::Statement::BindUint64()` method. So we cast to int64_t.
  int64_t token_high = static_cast<int64_t>(
      dictionary_info.disk_cache_key_token().GetHighForSerialization());
  int64_t token_low = static_cast<int64_t>(
      dictionary_info.disk_cache_key_token().GetLowForSerialization());
  statement.BindInt64(13, token_high);
  statement.BindInt64(14, token_low);

  if (!statement.Run()) {
    return base::unexpected(Error::kFailedToExecuteSql);
  }
  int64_t primary_key = db()->GetLastInsertRowId();

  uint64_t total_dictionary_size = 0;
  Error error =
      UpdateTotalDictionarySizeInMetaTable(size_delta, &total_dictionary_size);
  if (error != Error::kOk) {
    return base::unexpected(error);
  }

  std::vector<base::UnguessableToken> evicted_disk_cache_key_tokens;
  error = MaybeEvictDictionariesForPerSiteLimit(
      isolation_key.top_frame_site(), max_size_per_site, max_count_per_site,
      &evicted_disk_cache_key_tokens, &total_dictionary_size);
  if (error != Error::kOk) {
    return base::unexpected(error);
  }

  ASSIGN_OR_RETURN(uint64_t total_dictionary_count, GetTotalDictionaryCount());

  if (!transaction.Commit()) {
    return base::unexpected(Error::kFailedToCommitTransaction);
  }
  return base::ok(RegisterDictionaryResult{
      primary_key, replaced_disk_cache_key_token,
      std::set<base::UnguessableToken>(evicted_disk_cache_key_tokens.begin(),
                                       evicted_disk_cache_key_tokens.end()),
      total_dictionary_size, total_dictionary_count});
}

SQLitePersistentSharedDictionaryStore::Error
SQLitePersistentSharedDictionaryStore::Backend::
    MaybeEvictDictionariesForPerSiteLimit(
        const SchemefulSite& top_frame_site,
        uint64_t max_size_per_site,
        uint64_t max_count_per_site,
        std::vector<base::UnguessableToken>* evicted_disk_cache_key_tokens,
        uint64_t* total_dictionary_size_out) {
  std::vector<int64_t> primary_keys;
  int64_t total_candidate_dictionary_size = 0;
  Error error = SelectCandidatesForPerSiteEviction(
      top_frame_site, max_size_per_site, max_count_per_site, &primary_keys,
      evicted_disk_cache_key_tokens, &total_candidate_dictionary_size);
  if (error != Error::kOk) {
    return error;
  }
  CHECK_EQ(primary_keys.size(), evicted_disk_cache_key_tokens->size());
  if (primary_keys.empty()) {
    return Error::kOk;
  }
  for (int64_t primary_key : primary_keys) {
    error = DeleteDictionaryByPrimaryKey(primary_key);
    if (error != Error::kOk) {
      return error;
    }
  }
  error = UpdateTotalDictionarySizeInMetaTable(-total_candidate_dictionary_size,
                                               total_dictionary_size_out);
  if (error != Error::kOk) {
    return error;
  }
  return Error::kOk;
}

SQLitePersistentSharedDictionaryStore::Error
SQLitePersistentSharedDictionaryStore::Backend::
    SelectCandidatesForPerSiteEviction(
        const SchemefulSite& top_frame_site,
        uint64_t max_size_per_site,
        uint64_t max_count_per_site,
        std::vector<int64_t>* primary_keys_out,
        std::vector<base::UnguessableToken>* tokens_out,
        int64_t* total_size_of_candidates_out) {
  CHECK(primary_keys_out->empty());
  CHECK(tokens_out->empty());
  CHECK_EQ(0, *total_size_of_candidates_out);

  ASSIGN_OR_RETURN(uint64_t size_per_site,
                   GetDictionarySizePerSite(top_frame_site));
  ASSIGN_OR_RETURN(uint64_t count_per_site,
                   GetDictionaryCountPerSite(top_frame_site));

  base::UmaHistogramMemoryKB(
      base::StrCat({kHistogramPrefix, "DictionarySizeKBPerSiteWhenAdded"}),
      size_per_site);
  base::UmaHistogramCounts1000(
      base::StrCat({kHistogramPrefix, "DictionaryCountPerSiteWhenAdded"}),
      count_per_site);

  if ((max_size_per_site == 0 || size_per_site <= max_size_per_site) &&
      count_per_site <= max_count_per_site) {
    return Error::kOk;
  }

  uint64_t to_be_removed_count = 0;
  if (count_per_site > max_count_per_site) {
    to_be_removed_count = count_per_site - max_count_per_site;
  }

  int64_t to_be_removed_size = 0;
  if (max_size_per_site != 0 && size_per_site > max_size_per_site) {
    to_be_removed_size = size_per_site - max_size_per_site;
  }
  static constexpr char kQuery[] =
      // clang-format off
      "SELECT "
          "primary_key,"
          "size,"
          "token_high,"
          "token_low FROM dictionaries "
          "WHERE top_frame_site=? "
          "ORDER BY last_used_time";
  // clang-format on

  if (!db()->IsSQLValid(kQuery)) {
    return Error::kInvalidSql;
  }
  sql::Statement statement(db()->GetCachedStatement(SQL_FROM_HERE, kQuery));
  statement.BindString(0, top_frame_site.Serialize());

  base::CheckedNumeric<int64_t> checked_total_size_of_candidates;
  while (statement.Step()) {
    const int64_t primary_key_in_database = statement.ColumnInt64(0);
    const size_t size = statement.ColumnInt64(1);
    const int64_t token_high = statement.ColumnInt64(2);
    const int64_t token_low = statement.ColumnInt64(3);

    std::optional<base::UnguessableToken> disk_cache_key_token =
        ToUnguessableToken(token_high, token_low);
    if (!disk_cache_key_token) {
      LOG(WARNING) << "Invalid token";
      continue;
    }
    checked_total_size_of_candidates += size;

    if (!checked_total_size_of_candidates.IsValid()) {
      base::debug::DumpWithoutCrashing();
      return Error::kInvalidTotalDictSize;
    }

    *total_size_of_candidates_out =
        checked_total_size_of_candidates.ValueOrDie();
    primary_keys_out->emplace_back(primary_key_in_database);
    tokens_out->emplace_back(*disk_cache_key_token);

    if (*total_size_of_candidates_out >= to_be_removed_size &&
        tokens_out->size() >= to_be_removed_count) {
      break;
    }
  }

  return Error::kOk;
}

base::expected<uint64_t, SQLitePersistentSharedDictionaryStore::Error>
SQLitePersistentSharedDictionaryStore::Backend::GetDictionaryCountPerSite(
    const SchemefulSite& top_frame_site) {
  CHECK(background_task_runner()->RunsTasksInCurrentSequence());
  static constexpr char kQuery[] =
      // clang-format off
      "SELECT "
          "COUNT(primary_key) FROM dictionaries "
          "WHERE top_frame_site=?";
  // clang-format on

  if (!db()->IsSQLValid(kQuery)) {
    return base::unexpected(Error::kInvalidSql);
  }
  sql::Statement statement(db()->GetCachedStatement(SQL_FROM_HERE, kQuery));
  statement.BindString(0, top_frame_site.Serialize());
  uint64_t count_per_site = 0;
  if (statement.Step()) {
    count_per_site = statement.ColumnInt64(0);
  }
  return base::ok(count_per_site);
}

base::expected<uint64_t, SQLitePersistentSharedDictionaryStore::Error>
SQLitePersistentSharedDictionaryStore::Backend::GetDictionarySizePerSite(
    const SchemefulSite& top_frame_site) {
  CHECK(background_task_runner()->RunsTasksInCurrentSequence());
  static constexpr char kQuery[] =
      // clang-format off
      "SELECT "
          "SUM(size) FROM dictionaries "
          "WHERE top_frame_site=?";
  // clang-format on

  if (!db()->IsSQLValid(kQuery)) {
    return base::unexpected(Error::kInvalidSql);
  }
  sql::Statement statement(db()->GetCachedStatement(SQL_FROM_HERE, kQuery));
  statement.BindString(0, top_frame_site.Serialize());
  uint64_t size_per_site = 0;
  if (statement.Step()) {
    size_per_site = statement.ColumnInt64(0);
  }
  return base::ok(size_per_site);
}

SQLitePersistentSharedDictionaryStore::DictionaryListOrError
SQLitePersistentSharedDictionaryStore::Backend::GetDictionariesImpl(
    const SharedDictionaryIsolationKey& isolation_key) {
  CHECK(background_task_runner()->RunsTasksInCurrentSequence());
  std::vector<SharedDictionaryInfo> result;

  if (!InitializeDatabase()) {
    return base::unexpected(Error::kFailedToInitializeDatabase);
  }

  // Commit `pending_last_used_time_updates_`.
  DoCommit();

  static constexpr char kQuery[] =
      // clang-format off
      "SELECT "
          "primary_key,"
          "match,"
          "match_dest,"
          "id,"
          "url,"
          "last_fetch_time,"
          "res_time,"
          "exp_time,"
          "last_used_time,"
          "size,"
          "sha256,"
          "token_high,"
          "token_low FROM dictionaries "
          "WHERE frame_origin=? AND top_frame_site=? "
          "ORDER BY primary_key";
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
    const std::string match_dest = statement.ColumnString(2);
    const std::string id = statement.ColumnString(3);
    const std::string url_string = statement.ColumnString(4);
    const base::Time last_fetch_time = statement.ColumnTime(5);
    const base::Time response_time = statement.ColumnTime(6);
    const base::Time expiration_time = statement.ColumnTime(7);
    const base::Time last_used_time = statement.ColumnTime(8);
    const size_t size = statement.ColumnInt64(9);

    std::optional<SHA256HashValue> sha256_hash =
        ToSHA256HashValue(statement.ColumnBlob(10));
    if (!sha256_hash) {
      LOG(WARNING) << "Invalid hash";
      continue;
    }
    std::optional<base::UnguessableToken> disk_cache_key_token =
        ToUnguessableToken(statement.ColumnInt64(11),
                           statement.ColumnInt64(12));
    if (!disk_cache_key_token) {
      LOG(WARNING) << "Invalid token";
      continue;
    }
    result.emplace_back(GURL(url_string), last_fetch_time, response_time,
                        expiration_time - response_time, match, match_dest, id,
                        last_used_time, size, *sha256_hash,
                        *disk_cache_key_token, primary_key_in_database);
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
          "primary_key,"
          "frame_origin,"
          "top_frame_site,"
          "match,"
          "match_dest,"
          "id,"
          "url,"
          "last_fetch_time,"
          "res_time,"
          "exp_time,"
          "last_used_time,"
          "size,"
          "sha256,"
          "token_high,"
          "token_low FROM dictionaries "
          "ORDER BY primary_key";
  // clang-format on

  if (!db()->IsSQLValid(kQuery)) {
    return base::unexpected(Error::kInvalidSql);
  }

  std::map<SharedDictionaryIsolationKey, std::vector<SharedDictionaryInfo>>
      result;
  sql::Statement statement(db()->GetCachedStatement(SQL_FROM_HERE, kQuery));

  while (statement.Step()) {
    const int64_t primary_key_in_database = statement.ColumnInt64(0);
    const std::string frame_origin_string = statement.ColumnString(1);
    const std::string top_frame_site_string = statement.ColumnString(2);
    const std::string match = statement.ColumnString(3);
    const std::string match_dest = statement.ColumnString(4);
    const std::string id = statement.ColumnString(5);
    const std::string url_string = statement.ColumnString(6);
    const base::Time last_fetch_time = statement.ColumnTime(7);
    const base::Time response_time = statement.ColumnTime(8);
    const base::Time expiration_time = statement.ColumnTime(9);
    const base::Time last_used_time = statement.ColumnTime(10);
    const size_t size = statement.ColumnInt64(11);

    std::optional<SHA256HashValue> sha256_hash =
        ToSHA256HashValue(statement.ColumnBlob(12));
    if (!sha256_hash) {
      LOG(WARNING) << "Invalid hash";
      continue;
    }

    std::optional<base::UnguessableToken> disk_cache_key_token =
        ToUnguessableToken(statement.ColumnInt64(13),
                           statement.ColumnInt64(14));
    if (!disk_cache_key_token) {
      LOG(WARNING) << "Invalid token";
      continue;
    }

    url::Origin frame_origin = url::Origin::Create(GURL(frame_origin_string));
    SchemefulSite top_frame_site = SchemefulSite(GURL(top_frame_site_string));

    result[SharedDictionaryIsolationKey(frame_origin, top_frame_site)]
        .emplace_back(GURL(url_string), last_fetch_time, response_time,
                      expiration_time - response_time, match, match_dest, id,
                      last_used_time, size, *sha256_hash, *disk_cache_key_token,
                      primary_key_in_database);
  }
  return base::ok(std::move(result));
}

SQLitePersistentSharedDictionaryStore::UsageInfoOrError
SQLitePersistentSharedDictionaryStore::Backend::GetUsageInfoImpl() {
  CHECK(background_task_runner()->RunsTasksInCurrentSequence());
  if (!InitializeDatabase()) {
    return base::unexpected(Error::kFailedToInitializeDatabase);
  }

  static constexpr char kQuery[] =
      // clang-format off
      "SELECT "
          "frame_origin,"
          "top_frame_site,"
          "size FROM dictionaries "
          "ORDER BY primary_key";
  // clang-format on

  if (!db()->IsSQLValid(kQuery)) {
    return base::unexpected(Error::kInvalidSql);
  }

  std::map<SharedDictionaryIsolationKey, SharedDictionaryUsageInfo> result_map;
  sql::Statement statement(db()->GetCachedStatement(SQL_FROM_HERE, kQuery));

  while (statement.Step()) {
    const std::string frame_origin_string = statement.ColumnString(0);
    const std::string top_frame_site_string = statement.ColumnString(1);
    const size_t size = statement.ColumnInt64(2);

    const SharedDictionaryIsolationKey key = SharedDictionaryIsolationKey(
        url::Origin::Create(GURL(frame_origin_string)),
        SchemefulSite(GURL(top_frame_site_string)));
    auto it = result_map.find(key);
    if (it != result_map.end()) {
      it->second.total_size_bytes += size;
    } else {
      result_map[key] = SharedDictionaryUsageInfo{.isolation_key = key,
                                                  .total_size_bytes = size};
    }
  }

  std::vector<SharedDictionaryUsageInfo> result;
  for (auto& it : result_map) {
    result.push_back(std::move(it.second));
  }
  return base::ok(std::move(result));
}

SQLitePersistentSharedDictionaryStore::OriginListOrError
SQLitePersistentSharedDictionaryStore::Backend::GetOriginsBetweenImpl(
    const base::Time start_time,
    const base::Time end_time) {
  CHECK(background_task_runner()->RunsTasksInCurrentSequence());
  if (!InitializeDatabase()) {
    return base::unexpected(Error::kFailedToInitializeDatabase);
  }

  static constexpr char kQuery[] =
      // clang-format off
      "SELECT "
          "frame_origin FROM dictionaries "
          "WHERE res_time>=? AND res_time<? "
          "ORDER BY primary_key";
  // clang-format on

  if (!db()->IsSQLValid(kQuery)) {
    return base::unexpected(Error::kInvalidSql);
  }

  sql::Statement statement(db()->GetCachedStatement(SQL_FROM_HERE, kQuery));
  statement.BindTime(0, start_time);
  statement.BindTime(1, end_time);

  std::set<url::Origin> origins;
  while (statement.Step()) {
    const std::string frame_origin_string = statement.ColumnString(0);
    origins.insert(url::Origin::Create(GURL(frame_origin_string)));
  }
  return base::ok(std::vector<url::Origin>(origins.begin(), origins.end()));
}

SQLitePersistentSharedDictionaryStore::UnguessableTokenSetOrError
SQLitePersistentSharedDictionaryStore::Backend::ClearAllDictionariesImpl() {
  CHECK(background_task_runner()->RunsTasksInCurrentSequence());

  if (!InitializeDatabase()) {
    return base::unexpected(Error::kFailedToInitializeDatabase);
  }

  sql::Transaction transaction(db());
  if (!transaction.Begin()) {
    return base::unexpected(Error::kFailedToBeginTransaction);
  }

  static constexpr char kQuery[] =
      "DELETE FROM dictionaries RETURNING token_high, token_low";
  if (!db()->IsSQLValid(kQuery)) {
    return base::unexpected(Error::kInvalidSql);
  }
  sql::Statement statement(db()->GetCachedStatement(SQL_FROM_HERE, kQuery));

  std::vector<base::UnguessableToken> tokens;
  while (statement.Step()) {
    const int64_t token_high = statement.ColumnInt64(0);
    const int64_t token_low = statement.ColumnInt64(1);
    std::optional<base::UnguessableToken> disk_cache_key_token =
        ToUnguessableToken(token_high, token_low);
    if (!disk_cache_key_token) {
      continue;
    }
    tokens.emplace_back(*disk_cache_key_token);
  }

  if (!meta_table()->SetValue(kTotalDictSizeKey, 0)) {
    return base::unexpected(Error::kFailedToSetTotalDictSize);
  }

  if (!transaction.Commit()) {
    return base::unexpected(Error::kFailedToCommitTransaction);
  }
  return base::ok(
      std::set<base::UnguessableToken>(tokens.begin(), tokens.end()));
}

SQLitePersistentSharedDictionaryStore::UnguessableTokenSetOrError
SQLitePersistentSharedDictionaryStore::Backend::ClearDictionariesImpl(
    base::Time start_time,
    base::Time end_time,
    base::RepeatingCallback<bool(const GURL&)> url_matcher) {
  CHECK(background_task_runner()->RunsTasksInCurrentSequence());
  if (!InitializeDatabase()) {
    return base::unexpected(Error::kFailedToInitializeDatabase);
  }

  // Commit `pending_last_used_time_updates_`.
  DoCommit();

  sql::Transaction transaction(db());
  if (!transaction.Begin()) {
    return base::unexpected(Error::kFailedToBeginTransaction);
  }
  std::vector<int64_t> primary_keys;
  std::vector<base::UnguessableToken> tokens;
  int64_t total_size = 0;
  Error error = url_matcher ? SelectMatchingDictionariesWithUrlMatcher(
                                  start_time, end_time, std::move(url_matcher),
                                  &primary_keys, &tokens, &total_size)
                            : SelectMatchingDictionaries(start_time, end_time,
                                                         &primary_keys, &tokens,
                                                         &total_size);
  if (error != Error::kOk) {
    return base::unexpected(error);
  }
  for (int64_t primary_key : primary_keys) {
    error = DeleteDictionaryByPrimaryKey(primary_key);
    if (error != Error::kOk) {
      return base::unexpected(error);
    }
  }
  if (total_size != 0) {
    uint64_t total_dictionary_size = 0;
    error = UpdateTotalDictionarySizeInMetaTable(-total_size,
                                                 &total_dictionary_size);
    if (error != Error::kOk) {
      return base::unexpected(error);
    }
  }

  transaction.Commit();
  return base::ok(
      std::set<base::UnguessableToken>(tokens.begin(), tokens.end()));
}

SQLitePersistentSharedDictionaryStore::Error
SQLitePersistentSharedDictionaryStore::Backend::SelectMatchingDictionaries(
    base::Time start_time,
    base::Time end_time,
    std::vector<int64_t>* primary_keys_out,
    std::vector<base::UnguessableToken>* tokens_out,
    int64_t* total_size_out) {
  CHECK(background_task_runner()->RunsTasksInCurrentSequence());
  static constexpr char kQuery[] =
      // clang-format off
      "SELECT "
          "primary_key,"
          "size,"
          "token_high,"
          "token_low FROM dictionaries "
          "WHERE res_time>=? AND res_time<? "
          "ORDER BY primary_key";
  // clang-format on

  if (!db()->IsSQLValid(kQuery)) {
    return Error::kInvalidSql;
  }

  sql::Statement statement(db()->GetCachedStatement(SQL_FROM_HERE, kQuery));
  statement.BindTime(0, start_time);
  statement.BindTime(1, end_time.is_null() ? base::Time::Max() : end_time);

  base::CheckedNumeric<int64_t> checked_total_size;
  while (statement.Step()) {
    const int64_t primary_key_in_database = statement.ColumnInt64(0);
    const size_t size = statement.ColumnInt64(1);
    const int64_t token_high = statement.ColumnInt64(2);
    const int64_t token_low = statement.ColumnInt64(3);
    std::optional<base::UnguessableToken> disk_cache_key_token =
        ToUnguessableToken(token_high, token_low);
    if (!disk_cache_key_token) {
      LOG(WARNING) << "Invalid token";
      continue;
    }
    primary_keys_out->emplace_back(primary_key_in_database);
    tokens_out->emplace_back(*disk_cache_key_token);
    checked_total_size += size;
  }
  *total_size_out = checked_total_size.ValueOrDie();
  return Error::kOk;
}

SQLitePersistentSharedDictionaryStore::Error
SQLitePersistentSharedDictionaryStore::Backend::
    SelectMatchingDictionariesWithUrlMatcher(
        base::Time start_time,
        base::Time end_time,
        base::RepeatingCallback<bool(const GURL&)> url_matcher,
        std::vector<int64_t>* primary_keys_out,
        std::vector<base::UnguessableToken>* tokens_out,
        int64_t* total_size_out) {
  CHECK(background_task_runner()->RunsTasksInCurrentSequence());
  static constexpr char kQuery[] =
      // clang-format off
      "SELECT "
          "primary_key,"
          "frame_origin,"
          "top_frame_site,"
          "host,"
          "size,"
          "token_high,"
          "token_low FROM dictionaries "
          "WHERE res_time>=? AND res_time<? "
          "ORDER BY primary_key";
  // clang-format on

  if (!db()->IsSQLValid(kQuery)) {
    return Error::kInvalidSql;
  }

  sql::Statement statement(db()->GetCachedStatement(SQL_FROM_HERE, kQuery));
  statement.BindTime(0, start_time);
  statement.BindTime(1, end_time.is_null() ? base::Time::Max() : end_time);

  base::CheckedNumeric<int64_t> checked_total_size;
  while (statement.Step()) {
    const int64_t primary_key_in_database = statement.ColumnInt64(0);
    const std::string frame_origin_string = statement.ColumnString(1);
    const std::string top_frame_site_string = statement.ColumnString(2);
    const std::string host = statement.ColumnString(3);
    const size_t size = statement.ColumnInt64(4);
    const int64_t token_high = statement.ColumnInt64(5);
    const int64_t token_low = statement.ColumnInt64(6);

    if (!url_matcher.Run(GURL(frame_origin_string)) &&
        !url_matcher.Run(GURL(top_frame_site_string)) &&
        !url_matcher.Run(GURL(host))) {
      continue;
    }
    std::optional<base::UnguessableToken> disk_cache_key_token =
        ToUnguessableToken(token_high, token_low);
    if (!disk_cache_key_token) {
      LOG(WARNING) << "Invalid token";
      continue;
    }
    primary_keys_out->emplace_back(primary_key_in_database);
    tokens_out->emplace_back(*disk_cache_key_token);
    checked_total_size += size;
  }
  *total_size_out = checked_total_size.ValueOrDie();
  return Error::kOk;
}

SQLitePersistentSharedDictionaryStore::UnguessableTokenSetOrError
SQLitePersistentSharedDictionaryStore::Backend::
    ClearDictionariesForIsolationKeyImpl(
        const SharedDictionaryIsolationKey& isolation_key) {
  CHECK(background_task_runner()->RunsTasksInCurrentSequence());
  if (!InitializeDatabase()) {
    return base::unexpected(Error::kFailedToInitializeDatabase);
  }
  sql::Transaction transaction(db());
  if (!transaction.Begin()) {
    return base::unexpected(Error::kFailedToBeginTransaction);
  }

  static constexpr char kQuery[] =
      // clang-format off
      "DELETE FROM dictionaries "
          "WHERE frame_origin=? AND top_frame_site=? "
          "RETURNING size, token_high, token_low";
  // clang-format on

  if (!db()->IsSQLValid(kQuery)) {
    return base::unexpected(Error::kInvalidSql);
  }

  sql::Statement statement(db()->GetCachedStatement(SQL_FROM_HERE, kQuery));
  statement.BindString(0, isolation_key.frame_origin().Serialize());
  statement.BindString(1, isolation_key.top_frame_site().Serialize());

  std::vector<base::UnguessableToken> tokens;
  base::CheckedNumeric<int64_t> checked_total_size = 0;
  while (statement.Step()) {
    const size_t size = statement.ColumnInt64(0);
    const int64_t token_high = statement.ColumnInt64(1);
    const int64_t token_low = statement.ColumnInt64(2);

    checked_total_size += size;

    std::optional<base::UnguessableToken> disk_cache_key_token =
        ToUnguessableToken(token_high, token_low);
    if (!disk_cache_key_token) {
      continue;
    }
    tokens.emplace_back(*disk_cache_key_token);
  }

  int64_t total_size = checked_total_size.ValueOrDie();
  if (total_size != 0) {
    uint64_t total_dictionary_size = 0;
    Error error = UpdateTotalDictionarySizeInMetaTable(-total_size,
                                                       &total_dictionary_size);
    if (error != Error::kOk) {
      return base::unexpected(error);
    }
  }
  transaction.Commit();
  return base::ok(
      std::set<base::UnguessableToken>(tokens.begin(), tokens.end()));
}

SQLitePersistentSharedDictionaryStore::UnguessableTokenSetOrError
SQLitePersistentSharedDictionaryStore::Backend::DeleteExpiredDictionariesImpl(
    base::Time now) {
  CHECK(background_task_runner()->RunsTasksInCurrentSequence());
  if (!InitializeDatabase()) {
    return base::unexpected(Error::kFailedToInitializeDatabase);
  }
  sql::Transaction transaction(db());
  if (!transaction.Begin()) {
    return base::unexpected(Error::kFailedToBeginTransaction);
  }
  static constexpr char kQuery[] =
      // clang-format off
      "DELETE FROM dictionaries "
          "WHERE exp_time<=? "
          "RETURNING size, token_high, token_low";
  // clang-format on

  if (!db()->IsSQLValid(kQuery)) {
    return base::unexpected(Error::kInvalidSql);
  }

  sql::Statement statement(db()->GetCachedStatement(SQL_FROM_HERE, kQuery));
  statement.BindTime(0, now);

  std::vector<base::UnguessableToken> tokens;
  base::CheckedNumeric<int64_t> checked_total_size = 0;
  while (statement.Step()) {
    const size_t size = statement.ColumnInt64(0);
    const int64_t token_high = statement.ColumnInt64(1);
    const int64_t token_low = statement.ColumnInt64(2);

    checked_total_size += size;

    std::optional<base::UnguessableToken> disk_cache_key_token =
        ToUnguessableToken(token_high, token_low);
    if (!disk_cache_key_token) {
      LOG(WARNING) << "Invalid token";
      continue;
    }
    tokens.emplace_back(*disk_cache_key_token);
  }

  int64_t total_size = checked_total_size.ValueOrDie();
  if (total_size != 0) {
    uint64_t total_dictionary_size = 0;
    Error error = UpdateTotalDictionarySizeInMetaTable(-total_size,
                                                       &total_dictionary_size);
    if (error != Error::kOk) {
      return base::unexpected(error);
    }
  }
  transaction.Commit();
  return base::ok(
      std::set<base::UnguessableToken>(tokens.begin(), tokens.end()));
}

SQLitePersistentSharedDictionaryStore::UnguessableTokenSetOrError
SQLitePersistentSharedDictionaryStore::Backend::ProcessEvictionImpl(
    uint64_t cache_max_size,
    uint64_t size_low_watermark,
    uint64_t cache_max_count,
    uint64_t count_low_watermark) {
  if (!InitializeDatabase()) {
    return base::unexpected(Error::kFailedToInitializeDatabase);
  }

  // Commit `pending_last_used_time_updates_`.
  DoCommit();

  sql::Transaction transaction(db());
  if (!transaction.Begin()) {
    return base::unexpected(Error::kFailedToBeginTransaction);
  }
  std::vector<int64_t> primary_keys;
  std::vector<base::UnguessableToken> tokens;
  int64_t total_size_after_eviction = 0;
  Error error = SelectEvictionCandidates(
      cache_max_size, size_low_watermark, cache_max_count, count_low_watermark,
      &primary_keys, &tokens, &total_size_after_eviction);
  if (error != Error::kOk) {
    return base::unexpected(error);
  }
  CHECK_EQ(primary_keys.size(), tokens.size());
  if (primary_keys.empty()) {
    return base::ok(std::set<base::UnguessableToken>());
  }
  for (int64_t primary_key : primary_keys) {
    error = DeleteDictionaryByPrimaryKey(primary_key);
    if (error != Error::kOk) {
      return base::unexpected(error);
    }
  }

  if (!meta_table()->SetValue(kTotalDictSizeKey, total_size_after_eviction)) {
    return base::unexpected(Error::kFailedToSetTotalDictSize);
  }

  transaction.Commit();
  return base::ok(
      std::set<base::UnguessableToken>(tokens.begin(), tokens.end()));
}

SQLitePersistentSharedDictionaryStore::Error
SQLitePersistentSharedDictionaryStore::Backend::SelectEvictionCandidates(
    uint64_t cache_max_size,
    uint64_t size_low_watermark,
    uint64_t cache_max_count,
    uint64_t count_low_watermark,
    std::vector<int64_t>* primary_keys_out,
    std::vector<base::UnguessableToken>* tokens_out,
    int64_t* total_size_after_eviction_out) {
  ASSIGN_OR_RETURN(uint64_t total_dictionary_size,
                   GetTotalDictionarySizeImpl());
  ASSIGN_OR_RETURN(uint64_t total_dictionary_count, GetTotalDictionaryCount());

  if ((cache_max_size == 0 || total_dictionary_size <= cache_max_size) &&
      total_dictionary_count <= cache_max_count) {
    return Error::kOk;
  }

  uint64_t to_be_removed_count = 0;
  if (total_dictionary_count > count_low_watermark) {
    to_be_removed_count = total_dictionary_count - count_low_watermark;
  }

  base::CheckedNumeric<uint64_t> checked_total_dictionary_size =
      total_dictionary_size;

  static constexpr char kQuery[] =
      // clang-format off
      "SELECT "
          "primary_key,"
          "size,"
          "token_high,"
          "token_low FROM dictionaries "
          "ORDER BY last_used_time";
  // clang-format on

  if (!db()->IsSQLValid(kQuery)) {
    return Error::kInvalidSql;
  }

  sql::Statement statement(db()->GetCachedStatement(SQL_FROM_HERE, kQuery));
  while (statement.Step()) {
    const int64_t primary_key_in_database = statement.ColumnInt64(0);
    const size_t size = statement.ColumnInt64(1);
    const int64_t token_high = statement.ColumnInt64(2);
    const int64_t token_low = statement.ColumnInt64(3);
    std::optional<base::UnguessableToken> disk_cache_key_token =
        ToUnguessableToken(token_high, token_low);
    if (!disk_cache_key_token) {
      LOG(WARNING) << "Invalid token";
      continue;
    }
    checked_total_dictionary_size -= size;

    if (!checked_total_dictionary_size.IsValid()) {
      base::debug::DumpWithoutCrashing();
      return Error::kInvalidTotalDictSize;
    }

    *total_size_after_eviction_out =
        base::checked_cast<int64_t>(checked_total_dictionary_size.ValueOrDie());
    primary_keys_out->emplace_back(primary_key_in_database);
    tokens_out->emplace_back(*disk_cache_key_token);

    if ((cache_max_size == 0 ||
         size_low_watermark >= checked_total_dictionary_size.ValueOrDie()) &&
        tokens_out->size() >= to_be_removed_count) {
      break;
    }
  }
  return Error::kOk;
}

SQLitePersistentSharedDictionaryStore::Error
SQLitePersistentSharedDictionaryStore::Backend::DeleteDictionaryByPrimaryKey(
    int64_t primary_key) {
  CHECK(background_task_runner()->RunsTasksInCurrentSequence());
  static constexpr char kQuery[] =
      "DELETE FROM dictionaries WHERE primary_key=?";
  if (!db()->IsSQLValid(kQuery)) {
    return Error::kInvalidSql;
  }
  sql::Statement statement(db()->GetCachedStatement(SQL_FROM_HERE, kQuery));
  statement.BindInt64(0, primary_key);

  if (!statement.Run()) {
    return Error::kFailedToExecuteSql;
  }
  return Error::kOk;
}

SQLitePersistentSharedDictionaryStore::Error
SQLitePersistentSharedDictionaryStore::Backend::
    DeleteDictionariesByDiskCacheKeyTokensImpl(
        const std::set<base::UnguessableToken>& disk_cache_key_tokens) {
  if (!InitializeDatabase()) {
    return Error::kFailedToInitializeDatabase;
  }

  sql::Transaction transaction(db());
  if (!transaction.Begin()) {
    return Error::kFailedToBeginTransaction;
  }

  base::CheckedNumeric<int64_t> checked_total_deleted_dictionary_size;
  for (const auto& token : disk_cache_key_tokens) {
    ASSIGN_OR_RETURN(uint64_t deleted_dictionary_size,
                     DeleteDictionaryByDiskCacheToken(token));
    checked_total_deleted_dictionary_size += deleted_dictionary_size;
  }

  int64_t total_deleted_dictionary_size =
      checked_total_deleted_dictionary_size.ValueOrDie();
  if (total_deleted_dictionary_size != 0) {
    uint64_t total_dictionary_size = 0;
    Error error = UpdateTotalDictionarySizeInMetaTable(
        -total_deleted_dictionary_size, &total_dictionary_size);
    if (error != Error::kOk) {
      return error;
    }
  }

  if (!transaction.Commit()) {
    return Error::kFailedToCommitTransaction;
  }
  return Error::kOk;
}

SQLitePersistentSharedDictionaryStore::Error
SQLitePersistentSharedDictionaryStore::Backend::
    UpdateDictionaryLastFetchTimeImpl(int64_t primary_key_in_database,
                                      base::Time last_fetch_time) {
  if (!InitializeDatabase()) {
    return Error::kFailedToInitializeDatabase;
  }
  static constexpr char kQuery[] =
      "UPDATE dictionaries SET last_fetch_time=? WHERE primary_key=?";

  if (!db()->IsSQLValid(kQuery)) {
    return Error::kInvalidSql;
  }
  sql::Statement statement(db()->GetCachedStatement(SQL_FROM_HERE, kQuery));
  statement.BindTime(0, last_fetch_time);
  statement.BindInt64(1, primary_key_in_database);
  if (!statement.Run()) {
    return Error::kFailedToExecuteSql;
  }
  return Error::kOk;
}

base::expected<uint64_t, SQLitePersistentSharedDictionaryStore::Error>
SQLitePersistentSharedDictionaryStore::Backend::
    DeleteDictionaryByDiskCacheToken(
        const base::UnguessableToken& disk_cache_key_token) {
  CHECK(background_task_runner()->RunsTasksInCurrentSequence());
  if (!InitializeDatabase()) {
    return base::unexpected(Error::kFailedToInitializeDatabase);
  }
  static constexpr char kQuery[] =
      // clang-format off
      "DELETE FROM dictionaries "
          "WHERE token_high=? AND token_low=?"
          "RETURNING size";
  // clang-format on

  if (!db()->IsSQLValid(kQuery)) {
    return base::unexpected(Error::kInvalidSql);
  }

  sql::Statement statement(db()->GetCachedStatement(SQL_FROM_HERE, kQuery));
  // There is no `sql::Statement::BindUint64()` method. So we cast to int64_t.
  int64_t token_high =
      static_cast<int64_t>(disk_cache_key_token.GetHighForSerialization());
  int64_t token_low =
      static_cast<int64_t>(disk_cache_key_token.GetLowForSerialization());
  statement.BindInt64(0, token_high);
  statement.BindInt64(1, token_low);

  base::CheckedNumeric<uint64_t> checked_size = 0;
  while (statement.Step()) {
    const size_t size = statement.ColumnInt64(0);
    checked_size += size;
  }
  return base::ok(checked_size.ValueOrDie());
}

SQLitePersistentSharedDictionaryStore::UnguessableTokenSetOrError
SQLitePersistentSharedDictionaryStore::Backend::GetAllDiskCacheKeyTokensImpl() {
  CHECK(background_task_runner()->RunsTasksInCurrentSequence());
  if (!InitializeDatabase()) {
    return base::unexpected(Error::kFailedToInitializeDatabase);
  }

  static constexpr char kQuery[] =
      // clang-format off
      "SELECT "
          "primary_key,"
          "token_high,"
          "token_low FROM dictionaries "
          "ORDER BY primary_key";
  // clang-format on

  if (!db()->IsSQLValid(kQuery)) {
    return base::unexpected(Error::kInvalidSql);
  }

  sql::Statement statement(db()->GetCachedStatement(SQL_FROM_HERE, kQuery));
  std::vector<base::UnguessableToken> tokens;
  while (statement.Step()) {
    std::optional<base::UnguessableToken> disk_cache_key_token =
        ToUnguessableToken(statement.ColumnInt64(1), statement.ColumnInt64(2));
    if (!disk_cache_key_token) {
      LOG(WARNING) << "Invalid token";
      continue;
    }
    tokens.emplace_back(*disk_cache_key_token);
  }
  return base::ok(
      std::set<base::UnguessableToken>(tokens.begin(), tokens.end()));
}

void SQLitePersistentSharedDictionaryStore::Backend::
    UpdateDictionaryLastUsedTime(int64_t primary_key_in_database,
                                 base::Time last_used_time) {
  CHECK(client_task_runner()->RunsTasksInCurrentSequence());
  CHECK(!background_task_runner()->RunsTasksInCurrentSequence());
  size_t num_pending;
  {
    base::AutoLock locked(lock_);
    pending_last_used_time_updates_[primary_key_in_database] = last_used_time;
    num_pending = ++num_pending_;
  }
  // Commit every 30 seconds.
  static const int kCommitIntervalMs = 30 * 1000;
  // Commit right away if we have more than 100 operations.
  static const size_t kCommitAfterBatchSize = 100;
  if (num_pending == 1) {
    // We've gotten our first entry for this batch, fire off the timer.
    if (!background_task_runner()->PostDelayedTask(
            FROM_HERE, base::BindOnce(&Backend::Commit, this),
            base::Milliseconds(kCommitIntervalMs))) {
      NOTREACHED_IN_MIGRATION() << "background_task_runner_ is not running.";
    }
  } else if (num_pending >= kCommitAfterBatchSize) {
    // We've reached a big enough batch, fire off a commit now.
    PostBackgroundTask(FROM_HERE, base::BindOnce(&Backend::Commit, this));
  }
}

base::expected<uint64_t, SQLitePersistentSharedDictionaryStore::Error>
SQLitePersistentSharedDictionaryStore::Backend::GetTotalDictionaryCount() {
  CHECK(background_task_runner()->RunsTasksInCurrentSequence());
  static constexpr char kQuery[] =
      "SELECT COUNT(primary_key) FROM dictionaries";

  if (!db()->IsSQLValid(kQuery)) {
    return base::unexpected(Error::kInvalidSql);
  }
  uint64_t dictionary_count = 0;
  sql::Statement statement(db()->GetCachedStatement(SQL_FROM_HERE, kQuery));
  if (statement.Step()) {
    dictionary_count = statement.ColumnInt64(0);
  }
  return base::ok(dictionary_count);
}

bool SQLitePersistentSharedDictionaryStore::Backend::
    GetExistingDictionarySizeAndDiskCacheKeyToken(
        const SharedDictionaryIsolationKey& isolation_key,
        const url::SchemeHostPort& host,
        const std::string& match,
        const std::string& match_dest,
        int64_t* size_out,
        std::optional<base::UnguessableToken>* disk_cache_key_out) {
  CHECK(background_task_runner()->RunsTasksInCurrentSequence());

  static constexpr char kQuery[] =
      // clang-format off
      "SELECT "
          "size,"
          "token_high,"
          "token_low FROM dictionaries "
          "WHERE "
              "frame_origin=? AND "
              "top_frame_site=? AND "
              "host=? AND "
              "match=? AND "
              "match_dest=? "
          "ORDER BY primary_key";
  // clang-format on

  if (!db()->IsSQLValid(kQuery)) {
    return false;
  }
  sql::Statement statement(db()->GetCachedStatement(SQL_FROM_HERE, kQuery));
  statement.BindString(0, isolation_key.frame_origin().Serialize());
  statement.BindString(1, isolation_key.top_frame_site().Serialize());
  statement.BindString(2, host.Serialize());
  statement.BindString(3, match);
  statement.BindString(4, match_dest);

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
  ASSIGN_OR_RETURN(uint64_t total_dictionary_size,
                   GetTotalDictionarySizeImpl());
  base::CheckedNumeric<uint64_t> checked_total_dictionary_size =
      total_dictionary_size;
  checked_total_dictionary_size += size_delta;
  if (!checked_total_dictionary_size.IsValid()) {
    LOG(ERROR) << "Invalid total_dict_size detected.";
    base::debug::DumpWithoutCrashing();
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
    base::OnceCallback<void(SizeOrError)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_->GetTotalDictionarySize(
      WrapCallbackWithWeakPtrCheck(GetWeakPtr(), std::move(callback)));
}

void SQLitePersistentSharedDictionaryStore::RegisterDictionary(
    const SharedDictionaryIsolationKey& isolation_key,
    SharedDictionaryInfo dictionary_info,
    const uint64_t max_size_per_site,
    const uint64_t max_count_per_site,
    base::OnceCallback<void(RegisterDictionaryResultOrError)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_->RegisterDictionary(
      WrapCallbackWithWeakPtrCheck(GetWeakPtr(), std::move(callback)),
      isolation_key, std::move(dictionary_info), max_size_per_site,
      max_count_per_site);
}

void SQLitePersistentSharedDictionaryStore::GetDictionaries(
    const SharedDictionaryIsolationKey& isolation_key,
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

void SQLitePersistentSharedDictionaryStore::GetUsageInfo(
    base::OnceCallback<void(UsageInfoOrError)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_->GetUsageInfo(
      WrapCallbackWithWeakPtrCheck(GetWeakPtr(), std::move(callback)));
}

void SQLitePersistentSharedDictionaryStore::GetOriginsBetween(
    const base::Time start_time,
    const base::Time end_time,
    base::OnceCallback<void(OriginListOrError)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_->GetOriginsBetween(
      WrapCallbackWithWeakPtrCheck(GetWeakPtr(), std::move(callback)),
      start_time, end_time);
}

void SQLitePersistentSharedDictionaryStore::ClearAllDictionaries(
    base::OnceCallback<void(UnguessableTokenSetOrError)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_->ClearAllDictionaries(
      WrapCallbackWithWeakPtrCheck(GetWeakPtr(), std::move(callback)));
}

void SQLitePersistentSharedDictionaryStore::ClearDictionaries(
    const base::Time start_time,
    const base::Time end_time,
    base::RepeatingCallback<bool(const GURL&)> url_matcher,
    base::OnceCallback<void(UnguessableTokenSetOrError)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_->ClearDictionaries(
      WrapCallbackWithWeakPtrCheck(GetWeakPtr(), std::move(callback)),
      start_time, end_time, std::move(url_matcher));
}

void SQLitePersistentSharedDictionaryStore::ClearDictionariesForIsolationKey(
    const SharedDictionaryIsolationKey& isolation_key,
    base::OnceCallback<void(UnguessableTokenSetOrError)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_->ClearDictionariesForIsolationKey(
      WrapCallbackWithWeakPtrCheck(GetWeakPtr(), std::move(callback)),
      isolation_key);
}

void SQLitePersistentSharedDictionaryStore::DeleteExpiredDictionaries(
    const base::Time now,
    base::OnceCallback<void(UnguessableTokenSetOrError)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_->DeleteExpiredDictionaries(
      WrapCallbackWithWeakPtrCheck(GetWeakPtr(), std::move(callback)), now);
}

void SQLitePersistentSharedDictionaryStore::ProcessEviction(
    const uint64_t cache_max_size,
    const uint64_t size_low_watermark,
    const uint64_t cache_max_count,
    const uint64_t count_low_watermark,
    base::OnceCallback<void(UnguessableTokenSetOrError)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_->ProcessEviction(
      WrapCallbackWithWeakPtrCheck(GetWeakPtr(), std::move(callback)),
      cache_max_size, size_low_watermark, cache_max_count, count_low_watermark);
}

void SQLitePersistentSharedDictionaryStore::GetAllDiskCacheKeyTokens(
    base::OnceCallback<void(UnguessableTokenSetOrError)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_->GetAllDiskCacheKeyTokens(
      WrapCallbackWithWeakPtrCheck(GetWeakPtr(), std::move(callback)));
}

void SQLitePersistentSharedDictionaryStore::
    DeleteDictionariesByDiskCacheKeyTokens(
        std::set<base::UnguessableToken> disk_cache_key_tokens,
        base::OnceCallback<void(Error)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_->DeleteDictionariesByDiskCacheKeyTokens(
      WrapCallbackWithWeakPtrCheck(GetWeakPtr(), std::move(callback)),
      std::move(disk_cache_key_tokens));
}

void SQLitePersistentSharedDictionaryStore::UpdateDictionaryLastFetchTime(
    const int64_t primary_key_in_database,
    const base::Time last_fetch_time,
    base::OnceCallback<void(Error)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_->UpdateDictionaryLastFetchTime(
      WrapCallbackWithWeakPtrCheck(GetWeakPtr(), std::move(callback)),
      primary_key_in_database, last_fetch_time);
}

void SQLitePersistentSharedDictionaryStore::UpdateDictionaryLastUsedTime(
    int64_t primary_key_in_database,
    base::Time last_used_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_->UpdateDictionaryLastUsedTime(primary_key_in_database,
                                         last_used_time);
}

base::WeakPtr<SQLitePersistentSharedDictionaryStore>
SQLitePersistentSharedDictionaryStore::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_factory_.GetWeakPtr();
}

}  // namespace net
