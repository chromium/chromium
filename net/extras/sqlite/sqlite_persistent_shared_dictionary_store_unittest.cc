// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/extras/sqlite/sqlite_persistent_shared_dictionary_store.h"

#include <optional>
#include <tuple>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/test_file_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "net/base/schemeful_site.h"
#include "net/extras/shared_dictionary/shared_dictionary_info.h"
#include "net/shared_dictionary/shared_dictionary_isolation_key.h"
#include "net/test/test_with_task_environment.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/test/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Pair;
using ::testing::UnorderedElementsAreArray;

namespace net {

namespace {

const base::FilePath::CharType kSharedDictionaryStoreFilename[] =
    FILE_PATH_LITERAL("SharedDictionary");

const int kCurrentVersionNumber = 3;

int GetDBCurrentVersionNumber(sql::Database* db) {
  static constexpr char kGetDBCurrentVersionQuery[] =
      "SELECT value FROM meta WHERE key='version'";
  sql::Statement statement(db->GetUniqueStatement(kGetDBCurrentVersionQuery));
  statement.Step();
  return statement.ColumnInt(0);
}

bool CreateV1Schema(sql::Database* db) {
  sql::MetaTable meta_table;
  CHECK(meta_table.Init(db, 1, 1));
  constexpr char kTotalDictSizeKey[] = "total_dict_size";
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

  if (!db->Execute(kCreateTableQuery) ||
      !db->Execute(kCreateUniqueIndexQuery) ||
      !db->Execute(kCreateTopFrameSiteIndexQuery) ||
      !db->Execute(kCreateIsolationIndexQuery) ||
      !db->Execute(kCreateTokenIndexQuery) ||
      !db->Execute(kCreateExpirationTimeIndexQuery) ||
      !db->Execute(kCreateLastUsedTimeIndexQuery) ||
      !meta_table.SetValue(kTotalDictSizeKey, 0)) {
    return false;
  }
  return true;
}

bool CreateV2Schema(sql::Database* db) {
  sql::MetaTable meta_table;
  CHECK(meta_table.Init(db, 2, 2));
  constexpr char kTotalDictSizeKey[] = "total_dict_size";
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

  if (!db->Execute(kCreateTableQuery) ||
      !db->Execute(kCreateUniqueIndexQuery) ||
      !db->Execute(kCreateTopFrameSiteIndexQuery) ||
      !db->Execute(kCreateIsolationIndexQuery) ||
      !db->Execute(kCreateTokenIndexQuery) ||
      !db->Execute(kCreateExpirationTimeIndexQuery) ||
      !db->Execute(kCreateLastUsedTimeIndexQuery) ||
      !meta_table.SetValue(kTotalDictSizeKey, 0)) {
    return false;
  }
  return true;
}

SQLitePersistentSharedDictionaryStore::RegisterDictionaryResult
RegisterDictionaryImpl(SQLitePersistentSharedDictionaryStore* store,
                       const SharedDictionaryIsolationKey& isolation_key,
                       SharedDictionaryInfo dictionary_info,
                       uint64_t max_size_per_site = 1000000,
                       uint64_t max_count_per_site = 1000) {
  std::optional<SQLitePersistentSharedDictionaryStore::RegisterDictionaryResult>
      result_out;
  base::RunLoop run_loop;
  store->RegisterDictionary(
      isolation_key, std::move(dictionary_info), max_size_per_site,
      max_count_per_site,
      base::BindLambdaForTesting(
          [&](SQLitePersistentSharedDictionaryStore::
                  RegisterDictionaryResultOrError result) {
            ASSERT_TRUE(result.has_value());
            result_out = result.value();
            run_loop.Quit();
          }));
  run_loop.Run();
  CHECK(result_out);
  return *result_out;
}

// Register following 4 dictionaries for ProcessEviction tests.
//   dict1: size=1000 last_used_time=now
//   dict2: size=3000 last_used_time=now+4
//   dict3: size=5000 last_used_time=now+2
//   dict4: size=7000 last_used_time=now+3
std::tuple<SharedDictionaryInfo,
           SharedDictionaryInfo,
           SharedDictionaryInfo,
           SharedDictionaryInfo>
RegisterSharedDictionariesForProcessEvictionTest(
    SQLitePersistentSharedDictionaryStore* store,
    const SharedDictionaryIsolationKey& isolation_key) {
  const base::Time now = base::Time::Now();
  auto token1 = base::UnguessableToken::Create();
  SharedDictionaryInfo dict1 =
      SharedDictionaryInfo(GURL("https://a.example/dict"),
                           /*last_fetch_time=*/now, /*response_time=*/now,
                           /*expiration*/ base::Seconds(100), "/pattern*",
                           /*match_dest_string=*/"", /*id=*/"",
                           /*last_used_time*/ now,
                           /*size=*/1000, SHA256HashValue({{0x00, 0x01}}),
                           /*disk_cache_key_token=*/token1,
                           /*primary_key_in_database=*/std::nullopt);
  auto result1 = RegisterDictionaryImpl(store, isolation_key, dict1);
  dict1.set_primary_key_in_database(result1.primary_key_in_database());

  auto token2 = base::UnguessableToken::Create();
  SharedDictionaryInfo dict2 =
      SharedDictionaryInfo(GURL("https://b.example/dict"),
                           /*last_fetch_time=*/now, /*response_time=*/now,
                           /*expiration*/ base::Seconds(100), "/pattern*",
                           /*match_dest_string=*/"", /*id=*/"",
                           /*last_used_time*/ now + base::Seconds(1),
                           /*size=*/3000, SHA256HashValue({{0x00, 0x02}}),
                           /*disk_cache_key_token=*/token2,
                           /*primary_key_in_database=*/std::nullopt);
  auto result2 = RegisterDictionaryImpl(store, isolation_key, dict2);
  dict2.set_primary_key_in_database(result2.primary_key_in_database());

  auto token3 = base::UnguessableToken::Create();
  SharedDictionaryInfo dict3 =
      SharedDictionaryInfo(GURL("https://c.example/dict"),
                           /*last_fetch_time=*/now, /*response_time=*/now,
                           /*expiration*/ base::Seconds(100), "/pattern*",
                           /*match_dest_string=*/"", /*id=*/"",
                           /*last_used_time*/ now + base::Seconds(2),
                           /*size=*/5000, SHA256HashValue({{0x00, 0x03}}),
                           /*disk_cache_key_token=*/token3,
                           /*primary_key_in_database=*/std::nullopt);
  auto result3 = RegisterDictionaryImpl(store, isolation_key, dict3);
  dict3.set_primary_key_in_database(result3.primary_key_in_database());

  auto token4 = base::UnguessableToken::Create();
  SharedDictionaryInfo dict4 =
      SharedDictionaryInfo(GURL("https://d.example/dict"),
                           /*last_fetch_time=*/now, /*response_time=*/now,
                           /*expiration*/ base::Seconds(100), "/pattern*",
                           /*match_dest_string=*/"", /*id=*/"",
                           /*last_used_time*/ now + base::Seconds(3),
                           /*size=*/7000, SHA256HashValue({{0x00, 0x04}}),
                           /*disk_cache_key_token=*/token4,
                           /*primary_key_in_database=*/std::nullopt);
  auto result4 = RegisterDictionaryImpl(store, isolation_key, dict4);
  dict4.set_primary_key_in_database(result4.primary_key_in_database());

  // Call UpdateDictionaryLastUsedTime to update the last used time of dict2.
  store->UpdateDictionaryLastUsedTime(*dict2.primary_key_in_database(),
                                      now + base::Seconds(4));

  SharedDictionaryInfo updated_dict2 = SharedDictionaryInfo(
      dict2.url(), dict2.last_fetch_time(), dict2.response_time(),
      dict2.expiration(), dict2.match(), dict2.match_dest_string(), dict2.id(),
      now + base::Seconds(4), dict2.size(), dict2.hash(),
      dict2.disk_cache_key_token(), dict2.primary_key_in_database());

  return {dict1, updated_dict2, dict3, dict4};
}

}  // namespace

SharedDictionaryIsolationKey CreateIsolationKey(
    const std::string& frame_origin_str,
    const std::optional<std::string>& top_frame_site_str = std::nullopt) {
  return SharedDictionaryIsolationKey(
      url::Origin::Create(GURL(frame_origin_str)),
      top_frame_site_str ? SchemefulSite(GURL(*top_frame_site_str))
                         : SchemefulSite(GURL(frame_origin_str)));
}

class SQLitePersistentSharedDictionaryStoreTest : public ::testing::Test,
                                                  public WithTaskEnvironment {
 public:
  SQLitePersistentSharedDictionaryStoreTest()
      : WithTaskEnvironment(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        isolation_key_(CreateIsolationKey("https://origin.test/")),
        dictionary_info_(
            GURL("https://origin.test/dict"),
            /*last_fetch_time=*/base::Time::Now() - base::Seconds(9),
            /*response_time=*/base::Time::Now() - base::Seconds(10),
            /*expiration*/ base::Seconds(100),
            "/pattern*",
            /*match_dest_string=*/"",
            /*id=*/"dictionary_id",
            /*last_used_time*/ base::Time::Now(),
            /*size=*/1000,
            SHA256HashValue({{0x00, 0x01}}),
            /*disk_cache_key_token=*/base::UnguessableToken::Create(),
            /*primary_key_in_database=*/std::nullopt) {}

  SQLitePersistentSharedDictionaryStoreTest(
      const SQLitePersistentSharedDictionaryStoreTest&) = delete;
  SQLitePersistentSharedDictionaryStoreTest& operator=(
      const SQLitePersistentSharedDictionaryStoreTest&) = delete;

  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void TearDown() override { DestroyStore(); }

 protected:
  base::FilePath GetStroeFilePath() const {
    return temp_dir_.GetPath().Append(kSharedDictionaryStoreFilename);
  }

  void CreateStore() {
    CHECK(!store_);
    store_ = std::make_unique<SQLitePersistentSharedDictionaryStore>(
        GetStroeFilePath(), client_task_runner_, background_task_runner_);
  }

  void DestroyStore() {
    store_.reset();
    // Make sure we wait until the destructor has run by running all
    // TaskEnvironment tasks.
    RunUntilIdle();
  }

  uint64_t GetTotalDictionarySize() {
    base::RunLoop run_loop;
    uint64_t total_dictionary_size_out = 0;
    store_->GetTotalDictionarySize(base::BindLambdaForTesting(
        [&](base::expected<
            uint64_t, SQLitePersistentSharedDictionaryStore::Error> result) {
          ASSERT_TRUE(result.has_value());
          total_dictionary_size_out = result.value();
          run_loop.Quit();
        }));
    run_loop.Run();
    return total_dictionary_size_out;
  }

  SQLitePersistentSharedDictionaryStore::RegisterDictionaryResult
  RegisterDictionary(const SharedDictionaryIsolationKey& isolation_key,
                     SharedDictionaryInfo dictionary_info) {
    return RegisterDictionaryImpl(store_.get(), isolation_key,
                                  std::move(dictionary_info));
  }

  std::vector<SharedDictionaryInfo> GetDictionaries(
      const SharedDictionaryIsolationKey& isolation_key) {
    std::vector<SharedDictionaryInfo> result_dictionaries;
    base::RunLoop run_loop;
    store_->GetDictionaries(
        isolation_key,
        base::BindLambdaForTesting(
            [&](SQLitePersistentSharedDictionaryStore::DictionaryListOrError
                    result) {
              ASSERT_TRUE(result.has_value());
              result_dictionaries = std::move(result.value());
              run_loop.Quit();
            }));
    run_loop.Run();
    return result_dictionaries;
  }

  std::map<SharedDictionaryIsolationKey, std::vector<SharedDictionaryInfo>>
  GetAllDictionaries() {
    std::map<SharedDictionaryIsolationKey, std::vector<SharedDictionaryInfo>>
        result_all_dictionaries;
    base::RunLoop run_loop;
    store_->GetAllDictionaries(base::BindLambdaForTesting(
        [&](SQLitePersistentSharedDictionaryStore::DictionaryMapOrError
                result) {
          ASSERT_TRUE(result.has_value());
          result_all_dictionaries = std::move(result.value());
          run_loop.Quit();
        }));
    run_loop.Run();
    return result_all_dictionaries;
  }

  std::vector<SharedDictionaryUsageInfo> GetUsageInfo() {
    std::vector<SharedDictionaryUsageInfo> result_usage_info;
    base::RunLoop run_loop;
    store_->GetUsageInfo(base::BindLambdaForTesting(
        [&](SQLitePersistentSharedDictionaryStore::UsageInfoOrError result) {
          ASSERT_TRUE(result.has_value());
          result_usage_info = std::move(result.value());
          run_loop.Quit();
        }));
    run_loop.Run();
    return result_usage_info;
  }

  std::vector<url::Origin> GetOriginsBetween(const base::Time start_time,
                                             const base::Time end_time) {
    std::vector<url::Origin> origins;
    base::RunLoop run_loop;
    store_->GetOriginsBetween(
        start_time, end_time,
        base::BindLambdaForTesting(
            [&](SQLitePersistentSharedDictionaryStore::OriginListOrError
                    result) {
              ASSERT_TRUE(result.has_value());
              origins = std::move(result.value());
              run_loop.Quit();
            }));
    run_loop.Run();
    return origins;
  }

  std::set<base::UnguessableToken> ClearAllDictionaries() {
    base::RunLoop run_loop;
    std::set<base::UnguessableToken> tokens;
    store_->ClearAllDictionaries(base::BindLambdaForTesting(
        [&](SQLitePersistentSharedDictionaryStore::UnguessableTokenSetOrError
                result) {
          ASSERT_TRUE(result.has_value());
          tokens = std::move(result.value());
          run_loop.Quit();
        }));
    run_loop.Run();
    return tokens;
  }

  std::set<base::UnguessableToken> ClearDictionaries(
      const base::Time start_time,
      const base::Time end_time,
      base::RepeatingCallback<bool(const GURL&)> url_matcher) {
    base::RunLoop run_loop;
    std::set<base::UnguessableToken> tokens;
    store_->ClearDictionaries(
        start_time, end_time, std::move(url_matcher),
        base::BindLambdaForTesting([&](SQLitePersistentSharedDictionaryStore::
                                           UnguessableTokenSetOrError result) {
          ASSERT_TRUE(result.has_value());
          tokens = std::move(result.value());
          run_loop.Quit();
        }));
    run_loop.Run();
    return tokens;
  }

  std::set<base::UnguessableToken> ClearDictionariesForIsolationKey(
      const SharedDictionaryIsolationKey& isolation_key) {
    base::RunLoop run_loop;
    std::set<base::UnguessableToken> tokens;
    store_->ClearDictionariesForIsolationKey(
        isolation_key,
        base::BindLambdaForTesting([&](SQLitePersistentSharedDictionaryStore::
                                           UnguessableTokenSetOrError result) {
          ASSERT_TRUE(result.has_value());
          tokens = std::move(result.value());
          run_loop.Quit();
        }));
    run_loop.Run();
    return tokens;
  }

  std::set<base::UnguessableToken> DeleteExpiredDictionaries(
      const base::Time now) {
    base::RunLoop run_loop;
    std::set<base::UnguessableToken> tokens;
    store_->DeleteExpiredDictionaries(
        now,
        base::BindLambdaForTesting([&](SQLitePersistentSharedDictionaryStore::
                                           UnguessableTokenSetOrError result) {
          ASSERT_TRUE(result.has_value());
          tokens = std::move(result.value());
          run_loop.Quit();
        }));
    run_loop.Run();
    return tokens;
  }

  std::set<base::UnguessableToken> ProcessEviction(
      uint64_t cache_max_size,
      uint64_t size_low_watermark,
      uint64_t cache_max_count,
      uint64_t count_low_watermark) {
    base::RunLoop run_loop;
    std::set<base::UnguessableToken> tokens;
    store_->ProcessEviction(
        cache_max_size, size_low_watermark, cache_max_count,
        count_low_watermark,
        base::BindLambdaForTesting([&](SQLitePersistentSharedDictionaryStore::
                                           UnguessableTokenSetOrError result) {
          ASSERT_TRUE(result.has_value());
          tokens = std::move(result.value());
          run_loop.Quit();
        }));
    run_loop.Run();
    return tokens;
  }

  std::set<base::UnguessableToken> GetAllDiskCacheKeyTokens() {
    base::RunLoop run_loop;
    std::set<base::UnguessableToken> tokens;
    store_->GetAllDiskCacheKeyTokens(base::BindLambdaForTesting(
        [&](SQLitePersistentSharedDictionaryStore::UnguessableTokenSetOrError
                result) {
          ASSERT_TRUE(result.has_value());
          tokens = std::move(result.value());
          run_loop.Quit();
        }));
    run_loop.Run();
    return tokens;
  }

  SQLitePersistentSharedDictionaryStore::Error
  DeleteDictionariesByDiskCacheKeyTokens(
      std::set<base::UnguessableToken> disk_cache_key_tokens) {
    base::RunLoop run_loop;
    SQLitePersistentSharedDictionaryStore::Error error_out;
    store_->DeleteDictionariesByDiskCacheKeyTokens(
        std::move(disk_cache_key_tokens),
        base::BindLambdaForTesting(
            [&](SQLitePersistentSharedDictionaryStore::Error result_error) {
              error_out = result_error;
              run_loop.Quit();
            }));
    run_loop.Run();
    return error_out;
  }

  SQLitePersistentSharedDictionaryStore::Error UpdateDictionaryLastFetchTime(
      const int64_t primary_key_in_database,
      const base::Time last_fetch_time) {
    base::RunLoop run_loop;
    SQLitePersistentSharedDictionaryStore::Error error_out;
    store_->UpdateDictionaryLastFetchTime(
        primary_key_in_database, last_fetch_time,
        base::BindLambdaForTesting(
            [&](SQLitePersistentSharedDictionaryStore::Error result_error) {
              error_out = result_error;
              run_loop.Quit();
            }));
    run_loop.Run();
    return error_out;
  }

  void CorruptDatabaseFile() {
    // Execute CreateStore(), ClearAllDictionaries() and DestroyStore() to
    // create a database file.
    CreateStore();
    ClearAllDictionaries();
    DestroyStore();

    // Corrupt the database.
    CHECK(sql::test::CorruptSizeInHeader(GetStroeFilePath()));
  }

  void ManipulateDatabase(const std::vector<std::string>& queries) {
    // We don't allow manipulating the database while `store_` exists.
    ASSERT_FALSE(store_);

    std::unique_ptr<sql::Database> db =
        std::make_unique<sql::Database>(sql::DatabaseOptions{});
    ASSERT_TRUE(db->Open(GetStroeFilePath()));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(db.get(), kCurrentVersionNumber,
                                kCurrentVersionNumber));
    for (const std::string& query : queries) {
      ASSERT_TRUE(db->Execute(query));
    }
    db->Close();
  }

  void MakeFileUnwritable() {
    file_permissions_restorer_ =
        std::make_unique<base::FilePermissionRestorer>(GetStroeFilePath());
    ASSERT_TRUE(base::MakeFileUnwritable(GetStroeFilePath()));
  }

  void CheckStoreRecovered() {
    CreateStore();
    EXPECT_TRUE(GetDictionaries(isolation_key_).empty());
    EXPECT_TRUE(GetAllDictionaries().empty());
    DestroyStore();
  }

  void RunMultipleDictionariesTest(
      const SharedDictionaryIsolationKey isolation_key1,
      const SharedDictionaryInfo dictionary_info1,
      const SharedDictionaryIsolationKey isolation_key2,
      const SharedDictionaryInfo dictionary_info2,
      bool expect_merged);

  void RunGetTotalDictionarySizeFailureTest(
      SQLitePersistentSharedDictionaryStore::Error expected_error);
  void RunRegisterDictionaryFailureTest(
      SQLitePersistentSharedDictionaryStore::Error expected_error);
  void RunGetDictionariesFailureTest(
      SQLitePersistentSharedDictionaryStore::Error expected_error);
  void RunGetAllDictionariesFailureTest(
      SQLitePersistentSharedDictionaryStore::Error expected_error);
  void RunGetUsageInfoFailureTest(
      SQLitePersistentSharedDictionaryStore::Error expected_error);
  void RunGetOriginsBetweenFailureTest(
      SQLitePersistentSharedDictionaryStore::Error expected_error);
  void RunClearAllDictionariesFailureTest(
      SQLitePersistentSharedDictionaryStore::Error expected_error);
  void RunClearDictionariesFailureTest(
      base::RepeatingCallback<bool(const GURL&)> url_matcher,
      SQLitePersistentSharedDictionaryStore::Error expected_error);
  void RunClearDictionariesForIsolationKeyFailureTest(
      SQLitePersistentSharedDictionaryStore::Error expected_error);
  void RunDeleteExpiredDictionariesFailureTest(
      SQLitePersistentSharedDictionaryStore::Error expected_error);
  void RunProcessEvictionFailureTest(
      SQLitePersistentSharedDictionaryStore::Error expected_error);
  void RunGetAllDiskCacheKeyTokensFailureTest(
      SQLitePersistentSharedDictionaryStore::Error expected_error);

  base::ScopedTempDir temp_dir_;
  std::unique_ptr<SQLitePersistentSharedDictionaryStore> store_;
  const scoped_refptr<base::SequencedTaskRunner> client_task_runner_ =
      base::SingleThreadTaskRunner::GetCurrentDefault();
  const scoped_refptr<base::SequencedTaskRunner> background_task_runner_ =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
  // `file_permissions_restorer_` must be below `temp_dir_` to restore the
  // file permission correctly.
  std::unique_ptr<base::FilePermissionRestorer> file_permissions_restorer_;

  const SharedDictionaryIsolationKey isolation_key_;
  const SharedDictionaryInfo dictionary_info_;
};

TEST_F(SQLitePersistentSharedDictionaryStoreTest, SingleDictionary) {
  CreateStore();

  EXPECT_EQ(0u, GetTotalDictionarySize());

  auto register_dictionary_result =
      RegisterDictionary(isolation_key_, dictionary_info_);
  EXPECT_EQ(dictionary_info_.size(),
            register_dictionary_result.total_dictionary_size());
  EXPECT_EQ(1u, register_dictionary_result.total_dictionary_count());

  SharedDictionaryInfo expected_info = dictionary_info_;
  expected_info.set_primary_key_in_database(
      register_dictionary_result.primary_key_in_database());

  EXPECT_EQ(dictionary_info_.size(), GetTotalDictionarySize());
  EXPECT_THAT(GetDictionaries(isolation_key_),
              ElementsAreArray({expected_info}));
  EXPECT_THAT(
      GetAllDictionaries(),
      ElementsAre(Pair(isolation_key_, ElementsAreArray({expected_info}))));
  EXPECT_THAT(GetUsageInfo(),
              ElementsAre(SharedDictionaryUsageInfo{
                  .isolation_key = isolation_key_,
                  .total_size_bytes = dictionary_info_.size()}));
  EXPECT_TRUE(
      GetOriginsBetween(dictionary_info_.response_time() - base::Seconds(1),
                        dictionary_info_.response_time())
          .empty());
  EXPECT_THAT(
      GetOriginsBetween(dictionary_info_.response_time(),
                        dictionary_info_.response_time() + base::Seconds(1)),
      ElementsAreArray({isolation_key_.frame_origin()}));

  EXPECT_THAT(
      ClearAllDictionaries(),
      UnorderedElementsAreArray({dictionary_info_.disk_cache_key_token()}));

  EXPECT_EQ(0u, GetTotalDictionarySize());
  EXPECT_TRUE(GetDictionaries(isolation_key_).empty());
  EXPECT_TRUE(GetAllDictionaries().empty());
  EXPECT_TRUE(GetUsageInfo().empty());
}

void SQLitePersistentSharedDictionaryStoreTest::RunMultipleDictionariesTest(
    const SharedDictionaryIsolationKey isolation_key1,
    const SharedDictionaryInfo dictionary_info1,
    const SharedDictionaryIsolationKey isolation_key2,
    const SharedDictionaryInfo dictionary_info2,
    bool expect_merged) {
  CreateStore();

  auto register_dictionary_result1 =
      RegisterDictionary(isolation_key1, dictionary_info1);
  EXPECT_EQ(dictionary_info1.size(),
            register_dictionary_result1.total_dictionary_size());
  EXPECT_EQ(1u, register_dictionary_result1.total_dictionary_count());
  auto register_dictionary_result2 =
      RegisterDictionary(isolation_key2, dictionary_info2);
  EXPECT_EQ(expect_merged ? 1u : 2u,
            register_dictionary_result2.total_dictionary_count());

  EXPECT_NE(register_dictionary_result1.primary_key_in_database(),
            register_dictionary_result2.primary_key_in_database());

  SharedDictionaryInfo expected_info1 = dictionary_info1;
  SharedDictionaryInfo expected_info2 = dictionary_info2;
  expected_info1.set_primary_key_in_database(
      register_dictionary_result1.primary_key_in_database());
  expected_info2.set_primary_key_in_database(
      register_dictionary_result2.primary_key_in_database());
  base::Time oldest_response_time = std::min(dictionary_info1.response_time(),
                                             dictionary_info2.response_time());
  base::Time latest_response_time = std::max(dictionary_info1.response_time(),
                                             dictionary_info2.response_time());

  std::set<base::UnguessableToken> registered_tokens;

  if (isolation_key1 == isolation_key2) {
    if (expect_merged) {
      registered_tokens.insert(expected_info2.disk_cache_key_token());
      EXPECT_EQ(dictionary_info2.size(),
                register_dictionary_result2.total_dictionary_size());
      EXPECT_THAT(GetDictionaries(isolation_key1),
                  ElementsAreArray({expected_info2}));
      EXPECT_THAT(GetAllDictionaries(),
                  ElementsAre(Pair(isolation_key1,
                                   ElementsAreArray({expected_info2}))));
      ASSERT_TRUE(register_dictionary_result2.replaced_disk_cache_key_token());
      EXPECT_EQ(dictionary_info1.disk_cache_key_token(),
                *register_dictionary_result2.replaced_disk_cache_key_token());
      EXPECT_THAT(GetUsageInfo(),
                  ElementsAre(SharedDictionaryUsageInfo{
                      .isolation_key = isolation_key1,
                      .total_size_bytes = dictionary_info2.size()}));
      EXPECT_THAT(GetOriginsBetween(oldest_response_time,
                                    latest_response_time + base::Seconds(1)),
                  ElementsAreArray({isolation_key2.frame_origin()}));
    } else {
      registered_tokens.insert(expected_info1.disk_cache_key_token());
      registered_tokens.insert(expected_info2.disk_cache_key_token());

      EXPECT_EQ(dictionary_info1.size() + dictionary_info2.size(),
                register_dictionary_result2.total_dictionary_size());
      EXPECT_THAT(GetDictionaries(isolation_key1),
                  UnorderedElementsAreArray({expected_info1, expected_info2}));
      EXPECT_THAT(GetAllDictionaries(),
                  ElementsAre(Pair(isolation_key1,
                                   UnorderedElementsAreArray(
                                       {expected_info1, expected_info2}))));
      EXPECT_THAT(GetUsageInfo(),
                  ElementsAre(SharedDictionaryUsageInfo{
                      .isolation_key = isolation_key1,
                      .total_size_bytes =
                          dictionary_info1.size() + dictionary_info2.size()}));
      EXPECT_THAT(GetOriginsBetween(oldest_response_time,
                                    latest_response_time + base::Seconds(1)),
                  UnorderedElementsAreArray({isolation_key1.frame_origin()}));
    }
  } else {
    registered_tokens.insert(expected_info1.disk_cache_key_token());
    registered_tokens.insert(expected_info2.disk_cache_key_token());
    EXPECT_EQ(dictionary_info1.size() + dictionary_info2.size(),
              register_dictionary_result2.total_dictionary_size());
    EXPECT_THAT(GetDictionaries(isolation_key1),
                ElementsAreArray({expected_info1}));
    EXPECT_THAT(GetDictionaries(isolation_key2),
                ElementsAreArray({expected_info2}));
    EXPECT_THAT(
        GetAllDictionaries(),
        ElementsAre(Pair(isolation_key1, ElementsAreArray({expected_info1})),
                    Pair(isolation_key2, ElementsAreArray({expected_info2}))));
    EXPECT_THAT(GetUsageInfo(),
                UnorderedElementsAreArray(
                    {SharedDictionaryUsageInfo{
                         .isolation_key = isolation_key1,
                         .total_size_bytes = dictionary_info1.size()},
                     SharedDictionaryUsageInfo{
                         .isolation_key = isolation_key2,
                         .total_size_bytes = dictionary_info2.size()}}));
    EXPECT_THAT(GetOriginsBetween(oldest_response_time,
                                  latest_response_time + base::Seconds(1)),
                UnorderedElementsAreArray({isolation_key1.frame_origin(),
                                           isolation_key2.frame_origin()}));
  }

  EXPECT_THAT(ClearAllDictionaries(),
              UnorderedElementsAreArray(registered_tokens));
  EXPECT_TRUE(GetDictionaries(isolation_key_).empty());
  EXPECT_TRUE(GetAllDictionaries().empty());
  EXPECT_TRUE(GetUsageInfo().empty());
  EXPECT_TRUE(GetOriginsBetween(oldest_response_time,
                                latest_response_time + base::Seconds(1))
                  .empty());
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       MultipleDictionariesDifferentOriginSameSite) {
  SharedDictionaryIsolationKey isolation_key1 =
      CreateIsolationKey("https://www1.origin.test/");
  SharedDictionaryIsolationKey isolation_key2 =
      CreateIsolationKey("https://www2.origin.test/");
  EXPECT_NE(isolation_key1, isolation_key2);
  EXPECT_NE(isolation_key1.frame_origin(), isolation_key2.frame_origin());
  EXPECT_EQ(isolation_key1.top_frame_site(), isolation_key2.top_frame_site());
  RunMultipleDictionariesTest(isolation_key1, dictionary_info_, isolation_key2,
                              dictionary_info_, /*expect_merged=*/false);
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       MultipleDictionariesDifferentSite) {
  SharedDictionaryIsolationKey isolation_key1 =
      CreateIsolationKey("https://origin1.test/");
  SharedDictionaryIsolationKey isolation_key2 =
      CreateIsolationKey("https://origin2.test/");
  EXPECT_NE(isolation_key1, isolation_key2);
  EXPECT_NE(isolation_key1.frame_origin(), isolation_key2.frame_origin());
  EXPECT_NE(isolation_key1.top_frame_site(), isolation_key2.top_frame_site());
  RunMultipleDictionariesTest(isolation_key1, dictionary_info_, isolation_key2,
                              dictionary_info_, /*expect_merged=*/false);
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       MultipleDictionariesDifferentHostDifferentMatch) {
  RunMultipleDictionariesTest(
      isolation_key_,
      SharedDictionaryInfo(
          GURL("https://origin1.test/dict"),
          /*last_fetch_time=*/base::Time::Now() - base::Seconds(9),
          /*response_time=*/base::Time::Now() - base::Seconds(10),
          /*expiration*/ base::Seconds(100), /*match=*/"/pattern1*",
          /*match_dest_string=*/"", /*id=*/"",
          /*last_used_time*/ base::Time::Now(),
          /*size=*/1000, SHA256HashValue({{0x00, 0x01}}),
          /*disk_cache_key_token=*/base::UnguessableToken::Create(),
          /*primary_key_in_database=*/std::nullopt),
      isolation_key_,
      SharedDictionaryInfo(
          GURL("https://origin2.test/dict"),
          /*last_fetch_time=*/base::Time::Now() - base::Seconds(19),
          /*response_time=*/base::Time::Now() - base::Seconds(20),
          /*expiration*/ base::Seconds(200), /*match=*/"/pattern2*",
          /*match_dest_string=*/"", /*id=*/"",
          /*last_used_time*/ base::Time::Now(),
          /*size=*/2000, SHA256HashValue({{0x00, 0x02}}),
          /*disk_cache_key_token=*/base::UnguessableToken::Create(),
          /*primary_key_in_database=*/std::nullopt),
      /*expect_merged=*/false);
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       SameIsolationKeySameHostDifferentMatchSameMatchDest) {
  RunMultipleDictionariesTest(
      isolation_key_,
      SharedDictionaryInfo(
          GURL("https://origin.test/dict"),
          /*last_fetch_time=*/base::Time::Now() - base::Seconds(9),
          /*response_time=*/base::Time::Now() - base::Seconds(10),
          /*expiration*/ base::Seconds(100), /*match=*/"/pattern1*",
          /*match_dest_string=*/"", /*id=*/"",
          /*last_used_time*/ base::Time::Now(),
          /*size=*/1000, SHA256HashValue({{0x00, 0x01}}),
          /*disk_cache_key_token=*/base::UnguessableToken::Create(),
          /*primary_key_in_database=*/std::nullopt),
      isolation_key_,
      SharedDictionaryInfo(
          GURL("https://origin.test/dict"),
          /*last_fetch_time=*/base::Time::Now() - base::Seconds(19),
          /*response_time=*/base::Time::Now() - base::Seconds(20),
          /*expiration*/ base::Seconds(200), /*match=*/"/pattern2*",
          /*match_dest_string=*/"", /*id=*/"",
          /*last_used_time*/ base::Time::Now(),
          /*size=*/2000, SHA256HashValue({{0x00, 0x02}}),
          /*disk_cache_key_token=*/base::UnguessableToken::Create(),
          /*primary_key_in_database=*/std::nullopt),
      /*expect_merged=*/false);
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       SameIsolationKeySameHostSameMatchSameMatchDest) {
  RunMultipleDictionariesTest(
      isolation_key_,
      SharedDictionaryInfo(
          GURL("https://origin.test/dict"),
          /*last_fetch_time=*/base::Time::Now() - base::Seconds(9),
          /*response_time=*/base::Time::Now() - base::Seconds(10),
          /*expiration*/ base::Seconds(100), /*match=*/"/pattern*",
          /*match_dest_string=*/"", /*id=*/"",
          /*last_used_time*/ base::Time::Now(),
          /*size=*/1000, SHA256HashValue({{0x00, 0x01}}),
          /*disk_cache_key_token=*/base::UnguessableToken::Create(),
          /*primary_key_in_database=*/std::nullopt),
      isolation_key_,
      SharedDictionaryInfo(
          GURL("https://origin.test/dict"),
          /*last_fetch_time=*/base::Time::Now() - base::Seconds(19),
          /*response_time=*/base::Time::Now() - base::Seconds(20),
          /*expiration*/ base::Seconds(200), /*match=*/"/pattern*",
          /*match_dest_string=*/"", /*id=*/"",
          /*last_used_time*/ base::Time::Now(),
          /*size=*/2000, SHA256HashValue({{0x00, 0x02}}),
          /*disk_cache_key_token=*/base::UnguessableToken::Create(),
          /*primary_key_in_database=*/std::nullopt),
      /*expect_merged=*/true);
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       SameIsolationKeySameHostSameMatchDifferentMatchDest) {
  RunMultipleDictionariesTest(
      isolation_key_,
      SharedDictionaryInfo(
          GURL("https://origin.test/dict"),
          /*last_fetch_time=*/base::Time::Now() - base::Seconds(9),
          /*response_time=*/base::Time::Now() - base::Seconds(10),
          /*expiration*/ base::Seconds(100), /*match=*/"/pattern*",
          /*match_dest_string=*/"document", /*id=*/"",
          /*last_used_time*/ base::Time::Now(),
          /*size=*/1000, SHA256HashValue({{0x00, 0x01}}),
          /*disk_cache_key_token=*/base::UnguessableToken::Create(),
          /*primary_key_in_database=*/std::nullopt),
      isolation_key_,
      SharedDictionaryInfo(
          GURL("https://origin.test/dict"),
          /*last_fetch_time=*/base::Time::Now() - base::Seconds(19),
          /*response_time=*/base::Time::Now() - base::Seconds(20),
          /*expiration*/ base::Seconds(200), /*match=*/"/pattern*",
          /*match_dest_string=*/"script", /*id=*/"",
          /*last_used_time*/ base::Time::Now(),
          /*size=*/2000, SHA256HashValue({{0x00, 0x02}}),
          /*disk_cache_key_token=*/base::UnguessableToken::Create(),
          /*primary_key_in_database=*/std::nullopt),
      /*expect_merged=*/false);
}

void SQLitePersistentSharedDictionaryStoreTest::
    RunGetTotalDictionarySizeFailureTest(
        SQLitePersistentSharedDictionaryStore::Error expected_error) {
  CreateStore();
  base::RunLoop run_loop;
  store_->GetTotalDictionarySize(base::BindLambdaForTesting(
      [&](base::expected<uint64_t, SQLitePersistentSharedDictionaryStore::Error>
              result) {
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(expected_error, result.error());
        run_loop.Quit();
      }));
  run_loop.Run();
  DestroyStore();
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       GetTotalDictionarySizeErrorInitializationFailure) {
  CorruptDatabaseFile();
  RunGetTotalDictionarySizeFailureTest(SQLitePersistentSharedDictionaryStore::
                                           Error::kFailedToInitializeDatabase);
  CheckStoreRecovered();
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       GetTotalDictionarySizeErrorFailedToGetTotalDictSize) {
  CreateStore();
  EXPECT_TRUE(ClearAllDictionaries().empty());
  DestroyStore();
  ManipulateDatabase({"DELETE FROM meta WHERE key='total_dict_size'"});

  RunGetTotalDictionarySizeFailureTest(
      SQLitePersistentSharedDictionaryStore::Error::kFailedToGetTotalDictSize);

  CreateStore();
  // ClearAllDictionaries() resets total_dict_size in metadata.
  EXPECT_TRUE(ClearAllDictionaries().empty());
  // So GetTotalDictionarySize() should succeed.
  EXPECT_EQ(0u, GetTotalDictionarySize());
}

void SQLitePersistentSharedDictionaryStoreTest::
    RunRegisterDictionaryFailureTest(
        SQLitePersistentSharedDictionaryStore::Error expected_error) {
  CreateStore();
  base::RunLoop run_loop;
  store_->RegisterDictionary(
      isolation_key_, dictionary_info_, /*max_size_per_site=*/1000000,
      /*max_count_per_site=*/1000,
      base::BindLambdaForTesting(
          [&](SQLitePersistentSharedDictionaryStore::
                  RegisterDictionaryResultOrError result) {
            ASSERT_FALSE(result.has_value());
            EXPECT_EQ(expected_error, result.error());
            run_loop.Quit();
          }));
  run_loop.Run();
  DestroyStore();
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       RegisterDictionaryErrorDatabaseInitializationFailure) {
  CorruptDatabaseFile();
  RunRegisterDictionaryFailureTest(SQLitePersistentSharedDictionaryStore::
                                       Error::kFailedToInitializeDatabase);
  CheckStoreRecovered();
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       RegisterDictionaryErrorInvalidSql) {
  ManipulateDatabase({"CREATE TABLE dictionaries (dummy TEST NOT NULL)"});
  RunRegisterDictionaryFailureTest(
      SQLitePersistentSharedDictionaryStore::Error::kInvalidSql);
}

#if !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_WIN)
// MakeFileUnwritable() doesn't cause the failure on Fuchsia and Windows. So
// disabling the test on Fuchsia and Windows.
TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       RegisterDictionaryErrorSqlExecutionFailure) {
  CreateStore();
  ClearAllDictionaries();
  DestroyStore();
  MakeFileUnwritable();
  RunRegisterDictionaryFailureTest(
      SQLitePersistentSharedDictionaryStore::Error::kFailedToExecuteSql);
}
#endif  // !BUILDFLAG(IS_FUCHSIA)

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       RegisterDictionaryErrorFailedToGetTotalDictSize) {
  CreateStore();
  ClearAllDictionaries();
  DestroyStore();
  ManipulateDatabase({"DELETE FROM meta WHERE key='total_dict_size'"});

  RunRegisterDictionaryFailureTest(
      SQLitePersistentSharedDictionaryStore::Error::kFailedToGetTotalDictSize);

  CreateStore();
  // ClearAllDictionaries() resets total_dict_size in metadata.
  EXPECT_TRUE(ClearAllDictionaries().empty());
  // So RegisterDictionary() should succeed.
  RegisterDictionary(isolation_key_, dictionary_info_);
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       RegisterDictionaryErrorInvalidTotalDictSize) {
  CreateStore();

  SharedDictionaryInfo dictionary_info(
      dictionary_info_.url(), /*last_fetch_time*/ base::Time::Now(),
      /*response_time*/ base::Time::Now(), dictionary_info_.expiration(),
      dictionary_info_.match(), dictionary_info_.match_dest_string(),
      dictionary_info_.id(),
      /*last_used_time*/ base::Time::Now(), dictionary_info_.size() + 1,
      SHA256HashValue({{0x00, 0x02}}),
      /*disk_cache_key_token=*/base::UnguessableToken::Create(),
      /*primary_key_in_database=*/std::nullopt);

  // Register the dictionary which size is dictionary_info_.size() + 1.
  base::RunLoop run_loop;
  store_->RegisterDictionary(
      isolation_key_, dictionary_info, /*max_size_per_site=*/1000000,
      /*max_count_per_site=*/1000,
      base::BindLambdaForTesting(
          [&](SQLitePersistentSharedDictionaryStore::
                  RegisterDictionaryResultOrError result) {
            EXPECT_TRUE(result.has_value());
            run_loop.Quit();
          }));
  run_loop.Run();

  DestroyStore();

  // Set total_dict_size in metadata to 0.
  ManipulateDatabase({"UPDATE meta SET value=0 WHERE key='total_dict_size'"});

  // Registering `dictionary_info_` which size is smaller than the previous
  // dictionary cause InvalidTotalDictSize error because the calculated total
  // size will be negative.
  RunRegisterDictionaryFailureTest(
      SQLitePersistentSharedDictionaryStore::Error::kInvalidTotalDictSize);
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       RegisterDictionaryErrorTooBigDictionary) {
  CreateStore();
  uint64_t max_size_per_site = 10000;
  base::RunLoop run_loop;
  store_->RegisterDictionary(
      isolation_key_,
      SharedDictionaryInfo(
          GURL("https://a.example/dict"), /*last_fetch_time*/ base::Time::Now(),
          /*response_time=*/base::Time::Now(),
          /*expiration*/ base::Seconds(100), "/pattern*",
          /*match_dest_string=*/"", /*id=*/"",
          /*last_used_time*/ base::Time::Now(),
          /*size=*/max_size_per_site + 1, SHA256HashValue({{0x00, 0x01}}),
          /*disk_cache_key_token=*/base::UnguessableToken::Create(),
          /*primary_key_in_database=*/std::nullopt),
      max_size_per_site,
      /*max_count_per_site=*/1000,
      base::BindLambdaForTesting(
          [&](SQLitePersistentSharedDictionaryStore::
                  RegisterDictionaryResultOrError result) {
            ASSERT_FALSE(result.has_value());
            EXPECT_EQ(
                SQLitePersistentSharedDictionaryStore::Error::kTooBigDictionary,
                result.error());
            run_loop.Quit();
          }));
  run_loop.Run();
  EXPECT_EQ(0u, GetTotalDictionarySize());
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       RegisterDictionaryPerSiteEvictionWhenExceededSizeLimit) {
  CreateStore();

  uint64_t max_size_per_site = 10000;
  uint64_t max_count_per_site = 100;

  auto isolation_key1 = CreateIsolationKey("https://origin1.test",
                                           "https://top-frame-site1.test");
  auto dict1 = SharedDictionaryInfo(
      GURL("https://a.example/dict"), /*last_fetch_time*/ base::Time::Now(),
      /*response_time=*/base::Time::Now(),
      /*expiration*/ base::Seconds(100), "/pattern*", /*match_dest_string=*/"",
      /*id=*/"",
      /*last_used_time*/ base::Time::Now(),
      /*size=*/max_size_per_site, SHA256HashValue({{0x00, 0x01}}),
      /*disk_cache_key_token=*/base::UnguessableToken::Create(),
      /*primary_key_in_database=*/std::nullopt);
  auto result1 = RegisterDictionaryImpl(store_.get(), isolation_key1, dict1,
                                        max_size_per_site, max_count_per_site);
  dict1.set_primary_key_in_database(result1.primary_key_in_database());
  EXPECT_TRUE(result1.evicted_disk_cache_key_tokens().empty());
  EXPECT_THAT(GetAllDictionaries(),
              ElementsAre(Pair(isolation_key1, ElementsAreArray({dict1}))));

  FastForwardBy(base::Seconds(1));

  auto isolation_key2 = CreateIsolationKey("https://origin1.test",
                                           "https://top-frame-site2.test");
  auto dict2 = SharedDictionaryInfo(
      GURL("https://b.example/dict"), /*last_fetch_time*/ base::Time::Now(),
      /*response_time=*/base::Time::Now(),
      /*expiration*/ base::Seconds(100), "/pattern*", /*match_dest_string=*/"",
      /*id=*/"",
      /*last_used_time*/ base::Time::Now(),
      /*size=*/max_size_per_site / 2, SHA256HashValue({{0x00, 0x02}}),
      /*disk_cache_key_token=*/base::UnguessableToken::Create(),
      /*primary_key_in_database=*/std::nullopt);
  auto result2 = RegisterDictionaryImpl(store_.get(), isolation_key2, dict2,
                                        max_size_per_site, max_count_per_site);
  dict2.set_primary_key_in_database(result2.primary_key_in_database());
  EXPECT_TRUE(result2.evicted_disk_cache_key_tokens().empty());
  EXPECT_THAT(GetAllDictionaries(),
              ElementsAre(Pair(isolation_key1, ElementsAreArray({dict1})),
                          Pair(isolation_key2, ElementsAreArray({dict2}))));

  FastForwardBy(base::Seconds(1));

  auto dict3 = SharedDictionaryInfo(
      GURL("https://c.example/dict"), /*last_fetch_time*/ base::Time::Now(),
      /*response_time=*/base::Time::Now(),
      /*expiration*/ base::Seconds(100), "/pattern*", /*match_dest_string=*/"",
      /*id=*/"",
      /*last_used_time*/ base::Time::Now(),
      /*size=*/max_size_per_site / 2, SHA256HashValue({{0x00, 0x03}}),
      /*disk_cache_key_token=*/base::UnguessableToken::Create(),
      /*primary_key_in_database=*/std::nullopt);
  auto result3 = RegisterDictionaryImpl(store_.get(), isolation_key2, dict3,
                                        max_size_per_site, max_count_per_site);
  dict3.set_primary_key_in_database(result3.primary_key_in_database());
  EXPECT_TRUE(result3.evicted_disk_cache_key_tokens().empty());
  EXPECT_THAT(GetAllDictionaries(),
              ElementsAre(Pair(isolation_key1, ElementsAreArray({dict1})),
                          Pair(isolation_key2,
                               UnorderedElementsAreArray({dict2, dict3}))));

  FastForwardBy(base::Seconds(1));

  // The top frame site of `isolation_key3` is same as the top frame site of
  // `isolation_key2`.
  auto isolation_key3 = CreateIsolationKey("https://origin2.test",
                                           "https://top-frame-site2.test");
  auto dict4 = SharedDictionaryInfo(
      GURL("https://d.example/dict"), /*last_fetch_time*/ base::Time::Now(),
      /*response_time=*/base::Time::Now(),
      /*expiration*/ base::Seconds(100), "/pattern*", /*match_dest_string=*/"",
      /*id=*/"",
      /*last_used_time*/ base::Time::Now(),
      /*size=*/1, SHA256HashValue({{0x00, 0x04}}),
      /*disk_cache_key_token=*/base::UnguessableToken::Create(),
      /*primary_key_in_database=*/std::nullopt);
  auto result4 = RegisterDictionaryImpl(store_.get(), isolation_key3, dict4,
                                        max_size_per_site, max_count_per_site);
  dict4.set_primary_key_in_database(result4.primary_key_in_database());
  // dict2.size() + dict3.size() + dict4.size() exceeds `max_size_per_site`. So
  // the oldest dictionary `dict2` must be evicted.
  EXPECT_THAT(result4.evicted_disk_cache_key_tokens(),
              ElementsAreArray({dict2.disk_cache_key_token()}));
  EXPECT_THAT(GetAllDictionaries(),
              ElementsAre(Pair(isolation_key1, ElementsAreArray({dict1})),
                          Pair(isolation_key2, ElementsAreArray({dict3})),
                          Pair(isolation_key3, ElementsAreArray({dict4}))));
  EXPECT_EQ(dict1.size() + dict3.size() + dict4.size(),
            GetTotalDictionarySize());
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       RegisterDictionaryPerSiteEvictionWhenExceededCountLimit) {
  CreateStore();

  uint64_t max_size_per_site = 10000;
  uint64_t max_count_per_site = 2;

  auto isolation_key1 = CreateIsolationKey("https://origin1.test",
                                           "https://top-frame-site1.test");
  auto dict1 = SharedDictionaryInfo(
      GURL("https://a.example/dict"), /*last_fetch_time*/ base::Time::Now(),
      /*response_time=*/base::Time::Now(),
      /*expiration*/ base::Seconds(100), "/pattern*", /*match_dest_string=*/"",
      /*id=*/"",
      /*last_used_time*/ base::Time::Now(),
      /*size=*/100, SHA256HashValue({{0x00, 0x01}}),
      /*disk_cache_key_token=*/base::UnguessableToken::Create(),
      /*primary_key_in_database=*/std::nullopt);
  auto result1 = RegisterDictionaryImpl(store_.get(), isolation_key1, dict1,
                                        max_size_per_site, max_count_per_site);
  dict1.set_primary_key_in_database(result1.primary_key_in_database());
  EXPECT_TRUE(result1.evicted_disk_cache_key_tokens().empty());
  EXPECT_THAT(GetAllDictionaries(),
              ElementsAre(Pair(isolation_key1, ElementsAreArray({dict1}))));

  FastForwardBy(base::Seconds(1));

  auto isolation_key2 = CreateIsolationKey("https://origin1.test",
                                           "https://top-frame-site2.test");
  auto dict2 = SharedDictionaryInfo(
      GURL("https://b.example/dict"), /*last_fetch_time*/ base::Time::Now(),
      /*response_time=*/base::Time::Now(),
      /*expiration*/ base::Seconds(100), "/pattern*", /*match_dest_string=*/"",
      /*id=*/"",
      /*last_used_time*/ base::Time::Now(),
      /*size=*/200, SHA256HashValue({{0x00, 0x02}}),
      /*disk_cache_key_token=*/base::UnguessableToken::Create(),
      /*primary_key_in_database=*/std::nullopt);
  auto result2 = RegisterDictionaryImpl(store_.get(), isolation_key2, dict2,
                                        max_size_per_site, max_count_per_site);
  dict2.set_primary_key_in_database(result2.primary_key_in_database());
  EXPECT_TRUE(result2.evicted_disk_cache_key_tokens().empty());
  EXPECT_THAT(GetAllDictionaries(),
              ElementsAre(Pair(isolation_key1, ElementsAreArray({dict1})),
                          Pair(isolation_key2, ElementsAreArray({dict2}))));

  FastForwardBy(base::Seconds(1));

  auto dict3 = SharedDictionaryInfo(
      GURL("https://c.example/dict"), /*last_fetch_time*/ base::Time::Now(),
      /*response_time=*/base::Time::Now(),
      /*expiration*/ base::Seconds(100), "/pattern*", /*match_dest_string=*/"",
      /*id=*/"",
      /*last_used_time*/ base::Time::Now(),
      /*size=*/400, SHA256HashValue({{0x00, 0x03}}),
      /*disk_cache_key_token=*/base::UnguessableToken::Create(),
      /*primary_key_in_database=*/std::nullopt);
  auto result3 = RegisterDictionaryImpl(store_.get(), isolation_key2, dict3,
                                        max_size_per_site, max_count_per_site);
  dict3.set_primary_key_in_database(result3.primary_key_in_database());
  EXPECT_TRUE(result3.evicted_disk_cache_key_tokens().empty());
  EXPECT_THAT(GetAllDictionaries(),
              ElementsAre(Pair(isolation_key1, ElementsAreArray({dict1})),
                          Pair(isolation_key2,
                               UnorderedElementsAreArray({dict2, dict3}))));

  FastForwardBy(base::Seconds(1));

  // The top frame site of `isolation_key3` is same as the top frame site of
  // `isolation_key2`.
  auto isolation_key3 = CreateIsolationKey("https://origin2.test",
                                           "https://top-frame-site2.test");
  auto dict4 = SharedDictionaryInfo(
      GURL("https://d.example/dict"), /*last_fetch_time*/ base::Time::Now(),
      /*response_time=*/base::Time::Now(),
      /*expiration*/ base::Seconds(100), "/pattern*", /*match_dest_string=*/"",
      /*id=*/"",
      /*last_used_time*/ base::Time::Now(),
      /*size=*/800, SHA256HashValue({{0x00, 0x04}}),
      /*disk_cache_key_token=*/base::UnguessableToken::Create(),
      /*primary_key_in_database=*/std::nullopt);
  auto result4 = RegisterDictionaryImpl(store_.get(), isolation_key3, dict4,
                                        max_size_per_site, max_count_per_site);
  dict4.set_primary_key_in_database(result4.primary_key_in_database());
  // The dictionary count on "https://top-frame-site2.test" exceeds
  // `max_count_per_site`. So the oldest dictionary `dict2` must be evicted.
  EXPECT_THAT(result4.evicted_disk_cache_key_tokens(),
              ElementsAreArray({dict2.disk_cache_key_token()}));
  EXPECT_THAT(GetAllDictionaries(),
              ElementsAre(Pair(isolation_key1, ElementsAreArray({dict1})),
                          Pair(isolation_key2, ElementsAreArray({dict3})),
                          Pair(isolation_key3, ElementsAreArray({dict4}))));
  EXPECT_EQ(dict1.size() + dict3.size() + dict4.size(),
            GetTotalDictionarySize());
}

TEST_F(
    SQLitePersistentSharedDictionaryStoreTest,
    RegisterDictionaryPerSiteEvictionWhenExceededCountLimitWithoutSizeLimit) {
  CreateStore();

  uint64_t max_size_per_site = 0;
  uint64_t max_count_per_site = 2;

  auto isolation_key1 = CreateIsolationKey("https://origin1.test",
                                           "https://top-frame-site1.test");
  auto dict1 = SharedDictionaryInfo(
      GURL("https://a.example/dict"), /*last_fetch_time*/ base::Time::Now(),
      /*response_time=*/base::Time::Now(),
      /*expiration*/ base::Seconds(100), "/pattern*", /*match_dest_string=*/"",
      /*id=*/"",
      /*last_used_time*/ base::Time::Now(),
      /*size=*/100, SHA256HashValue({{0x00, 0x01}}),
      /*disk_cache_key_token=*/base::UnguessableToken::Create(),
      /*primary_key_in_database=*/std::nullopt);
  auto result1 = RegisterDictionaryImpl(store_.get(), isolation_key1, dict1,
                                        max_size_per_site, max_count_per_site);
  dict1.set_primary_key_in_database(result1.primary_key_in_database());
  EXPECT_TRUE(result1.evicted_disk_cache_key_tokens().empty());
  EXPECT_THAT(GetAllDictionaries(),
              ElementsAre(Pair(isolation_key1, ElementsAreArray({dict1}))));

  FastForwardBy(base::Seconds(1));

  auto isolation_key2 = CreateIsolationKey("https://origin1.test",
                                           "https://top-frame-site2.test");
  auto dict2 = SharedDictionaryInfo(
      GURL("https://b.example/dict"), /*last_fetch_time*/ base::Time::Now(),
      /*response_time=*/base::Time::Now(),
      /*expiration*/ base::Seconds(100), "/pattern*", /*match_dest_string=*/"",
      /*id=*/"",
      /*last_used_time*/ base::Time::Now(),
      /*size=*/200, SHA256HashValue({{0x00, 0x02}}),
      /*disk_cache_key_token=*/base::UnguessableToken::Create(),
      /*primary_key_in_database=*/std::nullopt);
  auto result2 = RegisterDictionaryImpl(store_.get(), isolation_key2, dict2,
                                        max_size_per_site, max_count_per_site);
  dict2.set_primary_key_in_database(result2.primary_key_in_database());
  EXPECT_TRUE(result2.evicted_disk_cache_key_tokens().empty());
  EXPECT_THAT(GetAllDictionaries(),
              ElementsAre(Pair(isolation_key1, ElementsAreArray({dict1})),
                          Pair(isolation_key2, ElementsAreArray({dict2}))));

  FastForwardBy(base::Seconds(1));

  auto dict3 = SharedDictionaryInfo(
      GURL("https://c.example/dict"), /*last_fetch_time*/ base::Time::Now(),
      /*response_time=*/base::Time::Now(),
      /*expiration*/ base::Seconds(100), "/pattern*", /*match_dest_string=*/"",
      /*id=*/"",
      /*last_used_time*/ base::Time::Now(),
      /*size=*/400, SHA256HashValue({{0x00, 0x03}}),
      /*disk_cache_key_token=*/base::UnguessableToken::Create(),
      /*primary_key_in_database=*/std::nullopt);
  auto result3 = RegisterDictionaryImpl(store_.get(), isolation_key2, dict3,
                                        max_size_per_site, max_count_per_site);
  dict3.set_primary_key_in_database(result3.primary_key_in_database());
  EXPECT_TRUE(result3.evicted_disk_cache_key_tokens().empty());
  EXPECT_THAT(GetAllDictionaries(),
              ElementsAre(Pair(isolation_key1, ElementsAreArray({dict1})),
                          Pair(isolation_key2,
                               UnorderedElementsAreArray({dict2, dict3}))));

  FastForwardBy(base::Seconds(1));

  // The top frame site of `isolation_key3` is same as the top frame site of
  // `isolation_key2`.
  auto isolation_key3 = CreateIsolationKey("https://origin2.test",
                                           "https://top-frame-site2.test");
  auto dict4 = SharedDictionaryInfo(
      GURL("https://d.example/dict"), /*last_fetch_time*/ base::Time::Now(),
      /*response_time=*/base::Time::Now(),
      /*expiration*/ base::Seconds(100), "/pattern*", /*match_dest_string=*/"",
      /*id=*/"",
      /*last_used_time*/ base::Time::Now(),
      /*size=*/800, SHA256HashValue({{0x00, 0x04}}),
      /*disk_cache_key_token=*/base::UnguessableToken::Create(),
      /*primary_key_in_database=*/std::nullopt);
  auto result4 = RegisterDictionaryImpl(store_.get(), isolation_key3, dict4,
                                        max_size_per_site, max_count_per_site);
  dict4.set_primary_key_in_database(result4.primary_key_in_database());
  // The dictionary count on "https://top-frame-site2.test" exceeds
  // `max_count_per_site`. So the oldest dictionary `dict2` must be evicted.
  EXPECT_THAT(result4.evicted_disk_cache_key_tokens(),
              ElementsAreArray({dict2.disk_cache_key_token()}));
  EXPECT_THAT(GetAllDictionaries(),
              ElementsAre(Pair(isolation_key1, ElementsAreArray({dict1})),
                          Pair(isolation_key2, ElementsAreArray({dict3})),
                          Pair(isolation_key3, ElementsAreArray({dict4}))));
  EXPECT_EQ(dict1.size() + dict3.size() + dict4.size(),
            GetTotalDictionarySize());
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       RegisterDictionaryPerSiteEvictionWhenExceededBothSizeAndCountLimit) {
  CreateStore();

  uint64_t max_size_per_site = 800;
  uint64_t max_count_per_site = 2;

  auto isolation_key1 = CreateIsolationKey("https://origin1.test",
                                           "https://top-frame-site1.test");
  auto dict1 = SharedDictionaryInfo(
      GURL("https://a.example/dict"), /*last_fetch_time*/ base::Time::Now(),
      /*response_time=*/base::Time::Now(),
      /*expiration*/ base::Seconds(100), "/pattern*", /*match_dest_string=*/"",
      /*id=*/"",
      /*last_used_time*/ base::Time::Now(),
      /*size=*/100, SHA256HashValue({{0x00, 0x01}}),
      /*disk_cache_key_token=*/base::UnguessableToken::Create(),
      /*primary_key_in_database=*/std::nullopt);
  auto result1 = RegisterDictionaryImpl(store_.get(), isolation_key1, dict1,
                                        max_size_per_site, max_count_per_site);
  dict1.set_primary_key_in_database(result1.primary_key_in_database());
  EXPECT_TRUE(result1.evicted_disk_cache_key_tokens().empty());
  EXPECT_THAT(GetAllDictionaries(),
              ElementsAre(Pair(isolation_key1, ElementsAreArray({dict1}))));

  FastForwardBy(base::Seconds(1));

  auto isolation_key2 = CreateIsolationKey("https://origin1.test",
                                           "https://top-frame-site2.test");
  auto dict2 = SharedDictionaryInfo(
      GURL("https://b.example/dict"), /*last_fetch_time*/ base::Time::Now(),
      /*response_time=*/base::Time::Now(),
      /*expiration*/ base::Seconds(100), "/pattern*", /*match_dest_string=*/"",
      /*id=*/"",
      /*last_used_time*/ base::Time::Now(),
      /*size=*/200, SHA256HashValue({{0x00, 0x02}}),
      /*disk_cache_key_token=*/base::UnguessableToken::Create(),
      /*primary_key_in_database=*/std::nullopt);
  auto result2 = RegisterDictionaryImpl(store_.get(), isolation_key2, dict2,
                                        max_size_per_site, max_count_per_site);
  dict2.set_primary_key_in_database(result2.primary_key_in_database());
  EXPECT_TRUE(result2.evicted_disk_cache_key_tokens().empty());
  EXPECT_THAT(GetAllDictionaries(),
              ElementsAre(Pair(isolation_key1, ElementsAreArray({dict1})),
                          Pair(isolation_key2, ElementsAreArray({dict2}))));

  FastForwardBy(base::Seconds(1));

  auto dict3 = SharedDictionaryInfo(
      GURL("https://c.example/dict"), /*last_fetch_time*/ base::Time::Now(),
      /*response_time=*/base::Time::Now(),
      /*expiration*/ base::Seconds(100), "/pattern*", /*match_dest_string=*/"",
      /*id=*/"",
      /*last_used_time*/ base::Time::Now(),
      /*size=*/400, SHA256HashValue({{0x00, 0x03}}),
      /*disk_cache_key_token=*/base::UnguessableToken::Create(),
      /*primary_key_in_database=*/std::nullopt);
  auto result3 = RegisterDictionaryImpl(store_.get(), isolation_key2, dict3,
                                        max_size_per_site, max_count_per_site);
  dict3.set_primary_key_in_database(result3.primary_key_in_database());
  EXPECT_TRUE(result3.evicted_disk_cache_key_tokens().empty());
  EXPECT_THAT(GetAllDictionaries(),
              ElementsAre(Pair(isolation_key1, ElementsAreArray({dict1})),
                          Pair(isolation_key2,
                               UnorderedElementsAreArray({dict2, dict3}))));

  FastForwardBy(base::Seconds(1));

  // The top frame site of `isolation_key3` is same as the top frame site of
  // `isolation_key2`.
  auto isolation_key3 = CreateIsolationKey("https://origin2.test",
                                           "https://top-frame-site2.test");
  auto dict4 = SharedDictionaryInfo(
      GURL("https://d.example/dict"), /*last_fetch_time*/ base::Time::Now(),
      /*response_time=*/base::Time::Now(),
      /*expiration*/ base::Seconds(100), "/pattern*", /*match_dest_string=*/"",
      /*id=*/"",
      /*last_used_time*/ base::Time::Now(),
      /*size=*/800, SHA256HashValue({{0x00, 0x04}}),
      /*disk_cache_key_token=*/base::UnguessableToken::Create(),
      /*primary_key_in_database=*/std::nullopt);
  auto result4 = RegisterDictionaryImpl(store_.get(), isolation_key3, dict4,
                                        max_size_per_site, max_count_per_site);
  dict4.set_primary_key_in_database(result4.primary_key_in_database());
  // The dictionary count on "https://top-frame-site2.test" exceeds
  // `max_count_per_site`. Also dictionary size on
  // "https://top-frame-site2.test" exceeds `max_size_per_site`.
  // So both `dict2` and `dict3` must be evicted.
  EXPECT_THAT(result4.evicted_disk_cache_key_tokens(),
              UnorderedElementsAreArray({dict2.disk_cache_key_token(),
                                         dict3.disk_cache_key_token()}));
  EXPECT_THAT(GetAllDictionaries(),
              ElementsAre(Pair(isolation_key1, ElementsAreArray({dict1})),
                          Pair(isolation_key3, ElementsAreArray({dict4}))));
  EXPECT_EQ(dict1.size() + dict4.size(), GetTotalDictionarySize());
}

void SQLitePersistentSharedDictionaryStoreTest::RunGetDictionariesFailureTest(
    SQLitePersistentSharedDictionaryStore::Error expected_error) {
  CreateStore();
  base::RunLoop run_loop;
  store_->GetDictionaries(
      isolation_key_,
      base::BindLambdaForTesting(
          [&](SQLitePersistentSharedDictionaryStore::DictionaryListOrError
                  result) {
            ASSERT_FALSE(result.has_value());
            EXPECT_EQ(expected_error, result.error());
            run_loop.Quit();
          }));
  run_loop.Run();
  DestroyStore();
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       GetDictionariesErrorDatabaseInitializationFailure) {
  CorruptDatabaseFile();
  RunGetDictionariesFailureTest(SQLitePersistentSharedDictionaryStore::Error::
                                    kFailedToInitializeDatabase);
  CheckStoreRecovered();
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       GetDictionariesErrorInvalidSql) {
  ManipulateDatabase({"CREATE TABLE dictionaries (dummy TEST NOT NULL)"});
  RunGetDictionariesFailureTest(
      SQLitePersistentSharedDictionaryStore::Error::kInvalidSql);
}

void SQLitePersistentSharedDictionaryStoreTest::
    RunGetAllDictionariesFailureTest(
        SQLitePersistentSharedDictionaryStore::Error expected_error) {
  CreateStore();
  base::RunLoop run_loop;
  store_->GetAllDictionaries(base::BindLambdaForTesting(
      [&](SQLitePersistentSharedDictionaryStore::DictionaryMapOrError result) {
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(expected_error, result.error());
        run_loop.Quit();
      }));
  run_loop.Run();
  DestroyStore();
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       GetAllDictionariesErrorDatabaseInitializationFailure) {
  CorruptDatabaseFile();
  RunGetAllDictionariesFailureTest(SQLitePersistentSharedDictionaryStore::
                                       Error::kFailedToInitializeDatabase);
  CheckStoreRecovered();
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       GetAllDictionariesErrorInvalidSql) {
  ManipulateDatabase({"CREATE TABLE dictionaries (dummy TEST NOT NULL)"});
  RunGetAllDictionariesFailureTest(
      SQLitePersistentSharedDictionaryStore::Error::kInvalidSql);
}

void SQLitePersistentSharedDictionaryStoreTest::RunGetUsageInfoFailureTest(
    SQLitePersistentSharedDictionaryStore::Error expected_error) {
  CreateStore();
  base::RunLoop run_loop;
  store_->GetUsageInfo(base::BindLambdaForTesting(
      [&](SQLitePersistentSharedDictionaryStore::UsageInfoOrError result) {
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(expected_error, result.error());
        run_loop.Quit();
      }));
  run_loop.Run();
  DestroyStore();
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       RunGetUsageInfoErrorDatabaseInitializationFailure) {
  CorruptDatabaseFile();
  RunGetUsageInfoFailureTest(SQLitePersistentSharedDictionaryStore::Error::
                                 kFailedToInitializeDatabase);
  CheckStoreRecovered();
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       RunGetUsageInfoErrorInvalidSql) {
  ManipulateDatabase({"CREATE TABLE dictionaries (dummy TEST NOT NULL)"});
  RunGetUsageInfoFailureTest(
      SQLitePersistentSharedDictionaryStore::Error::kInvalidSql);
}

void SQLitePersistentSharedDictionaryStoreTest::RunGetOriginsBetweenFailureTest(
    SQLitePersistentSharedDictionaryStore::Error expected_error) {
  CreateStore();
  base::RunLoop run_loop;
  store_->GetOriginsBetween(
      base::Time::Now(), base::Time::Now() + base::Seconds(1),
      base::BindLambdaForTesting(
          [&](SQLitePersistentSharedDictionaryStore::OriginListOrError result) {
            ASSERT_FALSE(result.has_value());
            EXPECT_EQ(expected_error, result.error());
            run_loop.Quit();
          }));
  run_loop.Run();
  DestroyStore();
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       RunGetOriginsBetweenErrorDatabaseInitializationFailure) {
  CorruptDatabaseFile();
  RunGetOriginsBetweenFailureTest(SQLitePersistentSharedDictionaryStore::Error::
                                      kFailedToInitializeDatabase);
  CheckStoreRecovered();
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       RunGetOriginsBetweenErrorInvalidSql) {
  ManipulateDatabase({"CREATE TABLE dictionaries (dummy TEST NOT NULL)"});
  RunGetOriginsBetweenFailureTest(
      SQLitePersistentSharedDictionaryStore::Error::kInvalidSql);
}

void SQLitePersistentSharedDictionaryStoreTest::
    RunClearAllDictionariesFailureTest(
        SQLitePersistentSharedDictionaryStore::Error expected_error) {
  CreateStore();
  base::RunLoop run_loop;
  store_->ClearAllDictionaries(base::BindLambdaForTesting(
      [&](SQLitePersistentSharedDictionaryStore::UnguessableTokenSetOrError
              result) {
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(expected_error, result.error());
        run_loop.Quit();
      }));
  run_loop.Run();
  DestroyStore();
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       ClearAllDictionariesErrorDatabaseInitializationFailure) {
  CorruptDatabaseFile();
  RunClearAllDictionariesFailureTest(SQLitePersistentSharedDictionaryStore::
                                         Error::kFailedToInitializeDatabase);
  CheckStoreRecovered();
}

#if !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_WIN)
// MakeFileUnwritable() doesn't cause the failure on Fuchsia and Windows. So
// disabling the test on Fuchsia and Windows.
TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       ClearAllDictionariesErrorSqlExecutionFailure) {
  CreateStore();
  ClearAllDictionaries();
  DestroyStore();
  MakeFileUnwritable();
  RunClearAllDictionariesFailureTest(
      SQLitePersistentSharedDictionaryStore::Error::kFailedToSetTotalDictSize);
}
#endif  // !BUILDFLAG(IS_FUCHSIA)

void SQLitePersistentSharedDictionaryStoreTest::RunClearDictionariesFailureTest(
    base::RepeatingCallback<bool(const GURL&)> url_matcher,
    SQLitePersistentSharedDictionaryStore::Error expected_error) {
  CreateStore();
  base::RunLoop run_loop;
  store_->ClearDictionaries(
      base::Time::Now() - base::Seconds(10), base::Time::Now(),
      std::move(url_matcher),
      base::BindLambdaForTesting(
          [&](SQLitePersistentSharedDictionaryStore::UnguessableTokenSetOrError
                  result) {
            ASSERT_FALSE(result.has_value());
            EXPECT_EQ(expected_error, result.error());
            run_loop.Quit();
          }));
  run_loop.Run();
  DestroyStore();
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       ClearDictionariesErrorDatabaseInitializationFailure) {
  CorruptDatabaseFile();
  RunClearDictionariesFailureTest(base::RepeatingCallback<bool(const GURL&)>(),
                                  SQLitePersistentSharedDictionaryStore::Error::
                                      kFailedToInitializeDatabase);
  CheckStoreRecovered();
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       ClearDictionariesErrorInvalidSql) {
  ManipulateDatabase({"CREATE TABLE dictionaries (dummy TEST NOT NULL)"});
  RunClearDictionariesFailureTest(
      base::RepeatingCallback<bool(const GURL&)>(),
      SQLitePersistentSharedDictionaryStore::Error::kInvalidSql);
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       ClearDictionariesWithUrlMatcherErrorInvalidSql) {
  ManipulateDatabase({"CREATE TABLE dictionaries (dummy TEST NOT NULL)"});
  RunClearDictionariesFailureTest(
      base::BindRepeating([](const GURL&) { return true; }),
      SQLitePersistentSharedDictionaryStore::Error::kInvalidSql);
}

#if !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_WIN)
// MakeFileUnwritable() doesn't cause the failure on Fuchsia and Windows. So
// disabling the test on Fuchsia and Windows.
TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       ClearDictionariesErrorSqlExecutionFailure) {
  CreateStore();
  RegisterDictionary(isolation_key_, dictionary_info_);
  DestroyStore();
  MakeFileUnwritable();
  RunClearDictionariesFailureTest(
      base::RepeatingCallback<bool(const GURL&)>(),
      SQLitePersistentSharedDictionaryStore::Error::kFailedToExecuteSql);
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       ClearDictionariesWithUrlMatcherErrorSqlExecutionFailure) {
  CreateStore();
  RegisterDictionary(isolation_key_, dictionary_info_);
  DestroyStore();
  MakeFileUnwritable();
  RunClearDictionariesFailureTest(
      base::BindRepeating([](const GURL&) { return true; }),
      SQLitePersistentSharedDictionaryStore::Error::kFailedToExecuteSql);
}
#endif  // !BUILDFLAG(IS_FUCHSIA)

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       ClearDictionariesErrorFailedToGetTotalDictSize) {
  CreateStore();
  RegisterDictionary(isolation_key_, dictionary_info_);
  DestroyStore();
  ManipulateDatabase({"DELETE FROM meta WHERE key='total_dict_size'"});

  RunClearDictionariesFailureTest(
      base::RepeatingCallback<bool(const GURL&)>(),
      SQLitePersistentSharedDictionaryStore::Error::kFailedToGetTotalDictSize);

  CreateStore();
  // ClearAllDictionaries() resets total_dict_size in metadata.
  EXPECT_THAT(
      ClearAllDictionaries(),
      UnorderedElementsAreArray({dictionary_info_.disk_cache_key_token()}));
  // So ClearDictionaries() should succeed.
  EXPECT_TRUE(ClearDictionaries(base::Time::Now() - base::Seconds(10),
                                base::Time::Now(),
                                base::RepeatingCallback<bool(const GURL&)>())
                  .empty());
}

void SQLitePersistentSharedDictionaryStoreTest::
    RunClearDictionariesForIsolationKeyFailureTest(
        SQLitePersistentSharedDictionaryStore::Error expected_error) {
  CreateStore();
  base::RunLoop run_loop;
  store_->ClearDictionariesForIsolationKey(
      isolation_key_,
      base::BindLambdaForTesting(
          [&](SQLitePersistentSharedDictionaryStore::UnguessableTokenSetOrError
                  result) {
            ASSERT_FALSE(result.has_value());
            EXPECT_EQ(expected_error, result.error());
            run_loop.Quit();
          }));
  run_loop.Run();
  DestroyStore();
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       ClearDictionariesForIsolationKeyErrorDatabaseInitializationFailure) {
  CorruptDatabaseFile();
  RunClearDictionariesForIsolationKeyFailureTest(
      SQLitePersistentSharedDictionaryStore::Error::
          kFailedToInitializeDatabase);
  CheckStoreRecovered();
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       ClearDictionariesForIsolationKeyErrorInvalidSql) {
  ManipulateDatabase({"CREATE TABLE dictionaries (dummy TEST NOT NULL)"});
  RunClearDictionariesForIsolationKeyFailureTest(
      SQLitePersistentSharedDictionaryStore::Error::kInvalidSql);
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       ClearDictionariesForIsolationKeyErrorFailedToGetTotalDictSize) {
  CreateStore();
  RegisterDictionary(isolation_key_, dictionary_info_);
  DestroyStore();
  ManipulateDatabase({"DELETE FROM meta WHERE key='total_dict_size'"});

  RunClearDictionariesForIsolationKeyFailureTest(
      SQLitePersistentSharedDictionaryStore::Error::kFailedToGetTotalDictSize);

  CreateStore();
  // ClearAllDictionaries() resets total_dict_size in metadata.
  EXPECT_THAT(
      ClearAllDictionaries(),
      UnorderedElementsAreArray({dictionary_info_.disk_cache_key_token()}));
  // So ClearDictionariesForIsolationKey() should succeed.
  EXPECT_TRUE(ClearDictionariesForIsolationKey(isolation_key_).empty());
}

void SQLitePersistentSharedDictionaryStoreTest::
    RunDeleteExpiredDictionariesFailureTest(
        SQLitePersistentSharedDictionaryStore::Error expected_error) {
  CreateStore();
  base::RunLoop run_loop;
  store_->DeleteExpiredDictionaries(
      base::Time::Now(),
      base::BindLambdaForTesting(
          [&](SQLitePersistentSharedDictionaryStore::UnguessableTokenSetOrError
                  result) {
            ASSERT_FALSE(result.has_value());
            EXPECT_EQ(expected_error, result.error());
            run_loop.Quit();
          }));
  run_loop.Run();
  DestroyStore();
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       DeleteExpiredDictionariesErrorDatabaseInitializationFailure) {
  CorruptDatabaseFile();
  RunDeleteExpiredDictionariesFailureTest(
      SQLitePersistentSharedDictionaryStore::Error::
          kFailedToInitializeDatabase);
  CheckStoreRecovered();
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       DeleteExpiredDictionariesErrorInvalidSql) {
  ManipulateDatabase({"CREATE TABLE dictionaries (dummy TEST NOT NULL)"});
  RunDeleteExpiredDictionariesFailureTest(
      SQLitePersistentSharedDictionaryStore::Error::kInvalidSql);
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       DeleteExpiredDictionariesErrorFailedToGetTotalDictSize) {
  CreateStore();
  RegisterDictionary(isolation_key_, dictionary_info_);
  DestroyStore();
  ManipulateDatabase({"DELETE FROM meta WHERE key='total_dict_size'"});

  // Move the clock forward by 90 seconds to make `dictionary_info_` expired.
  FastForwardBy(base::Seconds(90));

  RunDeleteExpiredDictionariesFailureTest(
      SQLitePersistentSharedDictionaryStore::Error::kFailedToGetTotalDictSize);

  CreateStore();
  // ClearAllDictionaries() resets total_dict_size in metadata.
  EXPECT_THAT(
      ClearAllDictionaries(),
      UnorderedElementsAreArray({dictionary_info_.disk_cache_key_token()}));
  // So DeleteExpiredDictionaries() should succeed.
  EXPECT_TRUE(DeleteExpiredDictionaries(base::Time::Now()).empty());
}

void SQLitePersistentSharedDictionaryStoreTest::RunProcessEvictionFailureTest(
    SQLitePersistentSharedDictionaryStore::Error expected_error) {
  CreateStore();
  base::RunLoop run_loop;
  store_->ProcessEviction(
      /*cache_max_size=*/1, /*size_low_watermark=*/1,
      /*cache_max_count=*/1, /*count_low_watermark=*/1,
      base::BindLambdaForTesting(
          [&](SQLitePersistentSharedDictionaryStore::UnguessableTokenSetOrError
                  result) {
            ASSERT_FALSE(result.has_value());
            EXPECT_EQ(expected_error, result.error());
            run_loop.Quit();
          }));
  run_loop.Run();
  DestroyStore();
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       ProcessEvictionErrorDatabaseInitializationFailure) {
  CorruptDatabaseFile();
  RunProcessEvictionFailureTest(SQLitePersistentSharedDictionaryStore::Error::
                                    kFailedToInitializeDatabase);
  CheckStoreRecovered();
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       ProcessEvictionErrorInvalidSql) {
  CreateStore();
  RegisterDictionary(isolation_key_, dictionary_info_);
  DestroyStore();
  // Delete the existing `dictionaries` table, and create a broken
  // `dictionaries` table.
  ManipulateDatabase({"DROP TABLE dictionaries",
                      "CREATE TABLE dictionaries (dummy TEST NOT NULL)"});
  RunProcessEvictionFailureTest(
      SQLitePersistentSharedDictionaryStore::Error::kInvalidSql);
}

#if !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_WIN)
// MakeFileUnwritable() doesn't cause the failure on Fuchsia and Windows. So
// disabling the test on Fuchsia and Windows.
TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       ProcessEvictionErrorSqlExecutionFailure) {
  CreateStore();
  RegisterDictionary(isolation_key_, dictionary_info_);
  DestroyStore();
  MakeFileUnwritable();

  RunProcessEvictionFailureTest(
      SQLitePersistentSharedDictionaryStore::Error::kFailedToExecuteSql);
}
#endif  // !BUILDFLAG(IS_FUCHSIA)

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       ProcessEvictionErrorFailedToGetTotalDictSize) {
  CreateStore();
  RegisterDictionary(isolation_key_, dictionary_info_);
  DestroyStore();
  ManipulateDatabase({"DELETE FROM meta WHERE key='total_dict_size'"});

  RunProcessEvictionFailureTest(
      SQLitePersistentSharedDictionaryStore::Error::kFailedToGetTotalDictSize);

  CreateStore();
  // ClearAllDictionaries() resets total_dict_size in metadata.
  EXPECT_THAT(
      ClearAllDictionaries(),
      UnorderedElementsAreArray({dictionary_info_.disk_cache_key_token()}));
  // So ProcessEviction() should succeed.
  EXPECT_TRUE(ProcessEviction(
                  /*cache_max_size=*/1, /*size_low_watermark=*/1,
                  /*cache_max_count=*/1, /*count_low_watermark=*/1)
                  .empty());
}

void SQLitePersistentSharedDictionaryStoreTest::
    RunGetAllDiskCacheKeyTokensFailureTest(
        SQLitePersistentSharedDictionaryStore::Error expected_error) {
  CreateStore();
  base::RunLoop run_loop;
  store_->GetAllDiskCacheKeyTokens(base::BindLambdaForTesting(
      [&](SQLitePersistentSharedDictionaryStore::UnguessableTokenSetOrError
              result) {
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(expected_error, result.error());
        run_loop.Quit();
      }));
  run_loop.Run();
  DestroyStore();
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       GetAllDiskCacheKeyTokensErrorDatabaseInitializationFailure) {
  CorruptDatabaseFile();
  RunGetAllDiskCacheKeyTokensFailureTest(
      SQLitePersistentSharedDictionaryStore::Error::
          kFailedToInitializeDatabase);
  CheckStoreRecovered();
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       GetAllDiskCacheKeyTokensErrorInvalidSql) {
  ManipulateDatabase({"CREATE TABLE dictionaries (dummy TEST NOT NULL)"});
  RunGetAllDiskCacheKeyTokensFailureTest(
      SQLitePersistentSharedDictionaryStore::Error::kInvalidSql);
}

TEST_F(
    SQLitePersistentSharedDictionaryStoreTest,
    DeleteDictionariesByDiskCacheKeyTokensErrorDatabaseInitializationFailure) {
  CorruptDatabaseFile();
  CreateStore();
  EXPECT_EQ(
      SQLitePersistentSharedDictionaryStore::Error::kFailedToInitializeDatabase,
      DeleteDictionariesByDiskCacheKeyTokens(
          {dictionary_info_.disk_cache_key_token()}));
  DestroyStore();
  CheckStoreRecovered();
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       DeleteDictionariesByDiskCacheKeyTokensErrorInvalidSql) {
  ManipulateDatabase({"CREATE TABLE dictionaries (dummy TEST NOT NULL)"});
  CreateStore();
  EXPECT_EQ(SQLitePersistentSharedDictionaryStore::Error::kInvalidSql,
            DeleteDictionariesByDiskCacheKeyTokens(
                {dictionary_info_.disk_cache_key_token()}));
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       DeleteDictionariesByDiskCacheKeyTokensErrorFailedToGetTotalDictSize) {
  CreateStore();
  RegisterDictionary(isolation_key_, dictionary_info_);
  DestroyStore();
  ManipulateDatabase({"DELETE FROM meta WHERE key='total_dict_size'"});
  CreateStore();
  EXPECT_EQ(
      SQLitePersistentSharedDictionaryStore::Error::kFailedToGetTotalDictSize,
      DeleteDictionariesByDiskCacheKeyTokens(
          {dictionary_info_.disk_cache_key_token()}));

  // ClearAllDictionaries() resets total_dict_size in metadata.
  EXPECT_THAT(
      ClearAllDictionaries(),
      UnorderedElementsAreArray({dictionary_info_.disk_cache_key_token()}));
  // So DeleteDictionariesByDiskCacheKeyTokens() should succeed.
  EXPECT_EQ(SQLitePersistentSharedDictionaryStore::Error::kOk,
            DeleteDictionariesByDiskCacheKeyTokens(
                {base::UnguessableToken::Create()}));
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       UpdateDictionaryLastFetchTimeErrorDatabaseInitializationFailure) {
  CorruptDatabaseFile();
  CreateStore();
  EXPECT_EQ(
      SQLitePersistentSharedDictionaryStore::Error::kFailedToInitializeDatabase,
      UpdateDictionaryLastFetchTime(/*primary_key_in_database=*/0,
                                    /*last_fetch_time=*/base::Time::Now()));
  DestroyStore();
  CheckStoreRecovered();
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       UpdateDictionaryLastFetchTimeErrorInvalidSql) {
  ManipulateDatabase({"CREATE TABLE dictionaries (dummy TEST NOT NULL)"});
  CreateStore();
  EXPECT_EQ(
      SQLitePersistentSharedDictionaryStore::Error::kInvalidSql,
      UpdateDictionaryLastFetchTime(/*primary_key_in_database=*/0,
                                    /*last_fetch_time=*/base::Time::Now()));
}

#if !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_WIN)
// MakeFileUnwritable() doesn't cause the failure on Fuchsia and Windows. So
// disabling the test on Fuchsia and Windows.
TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       UpdateDictionaryLastFetchTimeErrorSqlExecutionFailure) {
  CreateStore();
  auto register_dictionary_result =
      RegisterDictionary(isolation_key_, dictionary_info_);
  DestroyStore();
  MakeFileUnwritable();
  CreateStore();
  EXPECT_EQ(SQLitePersistentSharedDictionaryStore::Error::kFailedToExecuteSql,
            UpdateDictionaryLastFetchTime(
                register_dictionary_result.primary_key_in_database(),
                /*last_fetch_time=*/base::Time::Now()));
}
#endif  // !BUILDFLAG(IS_FUCHSIA)

TEST_F(SQLitePersistentSharedDictionaryStoreTest, InvalidHash) {
  CreateStore();
  auto register_dictionary_result =
      RegisterDictionary(isolation_key_, dictionary_info_);
  SharedDictionaryInfo expected_info = dictionary_info_;
  expected_info.set_primary_key_in_database(
      register_dictionary_result.primary_key_in_database());
  EXPECT_THAT(GetDictionaries(isolation_key_),
              ElementsAreArray({expected_info}));
  DestroyStore();

  ManipulateDatabase({"UPDATE dictionaries set sha256='DUMMY'"});

  CreateStore();
  EXPECT_TRUE(GetDictionaries(isolation_key_).empty());
  EXPECT_TRUE(GetAllDictionaries().empty());
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest, InvalidToken) {
  CreateStore();
  auto register_dictionary_result =
      RegisterDictionary(isolation_key_, dictionary_info_);
  SharedDictionaryInfo expected_info = dictionary_info_;
  expected_info.set_primary_key_in_database(
      register_dictionary_result.primary_key_in_database());
  EXPECT_THAT(GetDictionaries(isolation_key_),
              ElementsAreArray({expected_info}));
  DestroyStore();

  // {token_low=0, token_high=0} token is treated as invalid.
  ManipulateDatabase({"UPDATE dictionaries set token_low=0, token_high=0"});

  CreateStore();
  EXPECT_TRUE(GetDictionaries(isolation_key_).empty());
  EXPECT_TRUE(GetAllDictionaries().empty());
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       GetTotalDictionarySizeCallbackNotCalledAfterStoreDeleted) {
  CreateStore();
  store_->GetTotalDictionarySize(base::BindLambdaForTesting(
      [](base::expected<uint64_t,
                        SQLitePersistentSharedDictionaryStore::Error>) {
        EXPECT_TRUE(false) << "Should not be reached.";
      }));
  store_.reset();
  RunUntilIdle();
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       RegisterDictionaryCallbackNotCalledAfterStoreDeleted) {
  CreateStore();
  store_->RegisterDictionary(
      isolation_key_, dictionary_info_,
      /*max_size_per_site=*/1000000,
      /*max_count_per_site=*/1000,
      base::BindLambdaForTesting(
          [](SQLitePersistentSharedDictionaryStore::
                 RegisterDictionaryResultOrError result) {
            EXPECT_TRUE(false) << "Should not be reached.";
          }));
  store_.reset();
  RunUntilIdle();
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       GetDictionariesCallbackNotCalledAfterStoreDeleted) {
  CreateStore();
  store_->GetDictionaries(
      isolation_key_,
      base::BindLambdaForTesting(
          [](SQLitePersistentSharedDictionaryStore::DictionaryListOrError) {
            EXPECT_TRUE(false) << "Should not be reached.";
          }));
  store_.reset();
  RunUntilIdle();
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       GetAllDictionariesCallbackNotCalledAfterStoreDeleted) {
  CreateStore();
  store_->GetAllDictionaries(base::BindLambdaForTesting(
      [](SQLitePersistentSharedDictionaryStore::DictionaryMapOrError result) {
        EXPECT_TRUE(false) << "Should not be reached.";
      }));
  store_.reset();
  RunUntilIdle();
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       ClearAllDictionariesCallbackNotCalledAfterStoreDeleted) {
  CreateStore();
  store_->ClearAllDictionaries(base::BindLambdaForTesting(
      [](SQLitePersistentSharedDictionaryStore::UnguessableTokenSetOrError
             result) { EXPECT_TRUE(false) << "Should not be reached."; }));
  store_.reset();
  RunUntilIdle();
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       ClearDictionariesCallbackNotCalledAfterStoreDeleted) {
  CreateStore();
  store_->ClearDictionaries(
      base::Time::Now() - base::Seconds(1), base::Time::Now(),
      base::RepeatingCallback<bool(const GURL&)>(),
      base::BindLambdaForTesting(
          [&](SQLitePersistentSharedDictionaryStore::UnguessableTokenSetOrError
                  result) { EXPECT_TRUE(false) << "Should not be reached."; }));
  store_.reset();
  RunUntilIdle();
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       DeleteExpiredDictionariesCallbackNotCalledAfterStoreDeleted) {
  CreateStore();
  store_->DeleteExpiredDictionaries(
      base::Time::Now(),
      base::BindLambdaForTesting(
          [&](SQLitePersistentSharedDictionaryStore::UnguessableTokenSetOrError
                  result) { EXPECT_TRUE(false) << "Should not be reached."; }));
  store_.reset();
  RunUntilIdle();
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest, ClearDictionaries) {
  CreateStore();

  auto token1 = base::UnguessableToken::Create();
  SharedDictionaryInfo dict1 = SharedDictionaryInfo(
      GURL("https://a.example/dict"),
      /*last_fetch_time=*/base::Time::Now() - base::Seconds(4),
      /*response_time=*/base::Time::Now() - base::Seconds(4),
      /*expiration*/ base::Seconds(100), "/pattern*", /*match_dest_string=*/"",
      /*id=*/"",
      /*last_used_time*/ base::Time::Now(),
      /*size=*/1000, SHA256HashValue({{0x00, 0x01}}),
      /*disk_cache_key_token=*/token1,
      /*primary_key_in_database=*/std::nullopt);
  auto result1 = RegisterDictionary(isolation_key_, dict1);
  dict1.set_primary_key_in_database(result1.primary_key_in_database());

  auto token2 = base::UnguessableToken::Create();
  SharedDictionaryInfo dict2 = SharedDictionaryInfo(
      GURL("https://b.example/dict"),
      /*last_fetch_time=*/base::Time::Now() - base::Seconds(3),
      /*response_time=*/base::Time::Now() - base::Seconds(3),
      /*expiration*/ base::Seconds(100), "/pattern*", /*match_dest_string=*/"",
      /*id=*/"",
      /*last_used_time*/ base::Time::Now(),
      /*size=*/3000, SHA256HashValue({{0x00, 0x02}}),
      /*disk_cache_key_token=*/token2,
      /*primary_key_in_database=*/std::nullopt);
  auto result2 = RegisterDictionary(isolation_key_, dict2);
  dict2.set_primary_key_in_database(result2.primary_key_in_database());

  auto token3 = base::UnguessableToken::Create();
  SharedDictionaryInfo dict3 = SharedDictionaryInfo(
      GURL("https://c.example/dict"),
      /*last_fetch_time=*/base::Time::Now() - base::Seconds(2),
      /*response_time=*/base::Time::Now() - base::Seconds(2),
      /*expiration*/ base::Seconds(100), "/pattern*", /*match_dest_string=*/"",
      /*id=*/"",
      /*last_used_time*/ base::Time::Now(),
      /*size=*/5000, SHA256HashValue({{0x00, 0x03}}),
      /*disk_cache_key_token=*/token3,
      /*primary_key_in_database=*/std::nullopt);
  auto result3 = RegisterDictionary(isolation_key_, dict3);
  dict3.set_primary_key_in_database(result3.primary_key_in_database());

  auto token4 = base::UnguessableToken::Create();
  SharedDictionaryInfo dict4 = SharedDictionaryInfo(
      GURL("https://d.example/dict"),
      /*last_fetch_time=*/base::Time::Now() - base::Seconds(1),
      /*response_time=*/base::Time::Now() - base::Seconds(1),
      /*expiration*/ base::Seconds(100), "/pattern*", /*match_dest_string=*/"",
      /*id=*/"",
      /*last_used_time*/ base::Time::Now(),
      /*size=*/7000, SHA256HashValue({{0x00, 0x04}}),
      /*disk_cache_key_token=*/token4,
      /*primary_key_in_database=*/std::nullopt);
  auto result4 = RegisterDictionary(isolation_key_, dict4);
  dict4.set_primary_key_in_database(result4.primary_key_in_database());

  // No matching dictionaries to be deleted.
  EXPECT_TRUE(ClearDictionaries(base::Time::Now() - base::Seconds(200),
                                base::Time::Now() - base::Seconds(4),
                                base::RepeatingCallback<bool(const GURL&)>())
                  .empty());

  std::set<base::UnguessableToken> tokens =
      ClearDictionaries(base::Time::Now() - base::Seconds(3),
                        base::Time::Now() - base::Seconds(1),
                        base::RepeatingCallback<bool(const GURL&)>());
  // The dict2 which res_time is "now - 3 sec" and the dict3
  // which res_time is "now - 2 sec" must be deleted.
  EXPECT_THAT(tokens, UnorderedElementsAreArray({token2, token3}));

  // Check the remaining dictionaries.
  EXPECT_THAT(GetAllDictionaries(),
              ElementsAre(Pair(isolation_key_,
                               UnorderedElementsAreArray({dict1, dict4}))));
  // Check the total size of remaining dictionaries.
  EXPECT_EQ(dict1.size() + dict4.size(), GetTotalDictionarySize());
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       ClearDictionariesWithUrlMatcher) {
  CreateStore();

  auto isolation_key1 =
      CreateIsolationKey("https://a1.example/", "https://a2.example/");
  auto token1 = base::UnguessableToken::Create();
  SharedDictionaryInfo dict1 = SharedDictionaryInfo(
      GURL("https://a3.example/dict"),
      /*last_fetch_time=*/base::Time::Now() - base::Seconds(4),
      /*response_time=*/base::Time::Now() - base::Seconds(4),
      /*expiration*/ base::Seconds(100), "/pattern*", /*match_dest_string=*/"",
      /*id=*/"",
      /*last_used_time*/ base::Time::Now(),
      /*size=*/1000, SHA256HashValue({{0x00, 0x01}}),
      /*disk_cache_key_token=*/token1,
      /*primary_key_in_database=*/std::nullopt);
  auto result1 = RegisterDictionary(isolation_key1, dict1);
  dict1.set_primary_key_in_database(result1.primary_key_in_database());

  auto isolation_key2 =
      CreateIsolationKey("https://b1.example/", "https://b2.example/");
  auto token2 = base::UnguessableToken::Create();
  SharedDictionaryInfo dict2 = SharedDictionaryInfo(
      GURL("https://b3.example/dict"),
      /*last_fetch_time=*/base::Time::Now() - base::Seconds(3),
      /*response_time=*/base::Time::Now() - base::Seconds(3),
      /*expiration*/ base::Seconds(100), "/pattern*", /*match_dest_string=*/"",
      /*id=*/"",
      /*last_used_time*/ base::Time::Now(),
      /*size=*/3000, SHA256HashValue({{0x00, 0x02}}),
      /*disk_cache_key_token=*/token2,
      /*primary_key_in_database=*/std::nullopt);
  auto result2 = RegisterDictionary(isolation_key2, dict2);
  dict2.set_primary_key_in_database(result2.primary_key_in_database());

  auto isolation_key3 =
      CreateIsolationKey("https://c1.example/", "https://c2.example/");
  auto token3 = base::UnguessableToken::Create();
  SharedDictionaryInfo dict3 = SharedDictionaryInfo(
      GURL("https://c3.example/dict"),
      /*last_fetch_time=*/base::Time::Now() - base::Seconds(2),
      /*response_time=*/base::Time::Now() - base::Seconds(2),
      /*expiration*/ base::Seconds(100), "/pattern*", /*match_dest_string=*/"",
      /*id=*/"",
      /*last_used_time*/ base::Time::Now(),
      /*size=*/5000, SHA256HashValue({{0x00, 0x03}}),
      /*disk_cache_key_token=*/token3,
      /*primary_key_in_database=*/std::nullopt);
  auto result3 = RegisterDictionary(isolation_key3, dict3);
  dict3.set_primary_key_in_database(result3.primary_key_in_database());

  auto isolation_key4 =
      CreateIsolationKey("https://d1.example/", "https://d2.example/");
  auto token4 = base::UnguessableToken::Create();
  SharedDictionaryInfo dict4 = SharedDictionaryInfo(
      GURL("https://d3.example/dict"),
      /*last_fetch_time=*/base::Time::Now() - base::Seconds(1),
      /*response_time=*/base::Time::Now() - base::Seconds(1),
      /*expiration*/ base::Seconds(100), "/pattern*", /*match_dest_string=*/"",
      /*id=*/"",
      /*last_used_time*/ base::Time::Now(),
      /*size=*/7000, SHA256HashValue({{0x00, 0x04}}),
      /*disk_cache_key_token=*/token4,
      /*primary_key_in_database=*/std::nullopt);
  auto result4 = RegisterDictionary(isolation_key4, dict4);
  dict4.set_primary_key_in_database(result4.primary_key_in_database());

  // No matching dictionaries to be deleted.
  EXPECT_TRUE(ClearDictionaries(base::Time::Now() - base::Seconds(200),
                                base::Time::Now() - base::Seconds(4),
                                base::BindRepeating([](const GURL&) {
                                  EXPECT_TRUE(false)
                                      << "Should not be reached.";
                                  return true;
                                }))
                  .empty());
  std::set<GURL> checked_urls;
  EXPECT_TRUE(
      ClearDictionaries(base::Time::Now() - base::Seconds(3),
                        base::Time::Now() - base::Seconds(1),
                        base::BindLambdaForTesting([&](const GURL& url) {
                          checked_urls.insert(url);
                          return false;
                        }))
          .empty());
  // The dict2 which last_used_time is "now - 3 sec" and the dict3
  // which last_used_time is "now - 2 sec" must be selected and the macher is
  // called with those dictionaries frame_origin, top_frame_site and host.
  EXPECT_THAT(checked_urls,
              UnorderedElementsAreArray(
                  {GURL("https://b1.example/"), GURL("https://b2.example/"),
                   GURL("https://b3.example/"), GURL("https://c1.example/"),
                   GURL("https://c2.example/"), GURL("https://c3.example/")}));

  // Deletes dict3.
  std::set<base::UnguessableToken> tokens =
      ClearDictionaries(base::Time::Now() - base::Seconds(3),
                        base::Time::Now() - base::Seconds(1),
                        base::BindRepeating([](const GURL& url) {
                          return url == GURL("https://c3.example/");
                        }));
  EXPECT_THAT(tokens, ElementsAreArray({token3}));

  // Check the remaining dictionaries.
  EXPECT_THAT(GetAllDictionaries(),
              ElementsAre(Pair(isolation_key1, ElementsAreArray({dict1})),
                          Pair(isolation_key2, ElementsAreArray({dict2})),
                          Pair(isolation_key4, ElementsAreArray({dict4}))));
  // Check the total size of remaining dictionaries.
  EXPECT_EQ(dict1.size() + dict2.size() + dict4.size(),
            GetTotalDictionarySize());
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       ClearDictionariesForIsolationKeyEmptyStore) {
  CreateStore();
  EXPECT_TRUE(ClearDictionariesForIsolationKey(isolation_key_).empty());
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       ClearDictionariesForIsolation) {
  CreateStore();

  auto isolation_key1 =
      CreateIsolationKey("https://a1.example/", "https://a2.example/");
  auto token1 = base::UnguessableToken::Create();
  SharedDictionaryInfo dict1 =
      SharedDictionaryInfo(GURL("https://a1.example/dict"),
                           /*last_fetch_time=*/base::Time::Now(),
                           /*response_time=*/base::Time::Now(),
                           /*expiration*/ base::Seconds(100), "/pattern*",
                           /*match_dest_string=*/"", /*id=*/"",
                           /*last_used_time*/ base::Time::Now(),
                           /*size=*/1000, SHA256HashValue({{0x00, 0x01}}),
                           /*disk_cache_key_token=*/token1,
                           /*primary_key_in_database=*/std::nullopt);
  auto result1 = RegisterDictionary(isolation_key1, dict1);
  dict1.set_primary_key_in_database(result1.primary_key_in_database());

  // Same frame origin, different top frame site.
  auto isolation_key2 =
      CreateIsolationKey("https://a1.example/", "https://a3.example/");
  auto token2 = base::UnguessableToken::Create();
  SharedDictionaryInfo dict2 =
      SharedDictionaryInfo(GURL("https://a2.example/dict"),
                           /*last_fetch_time=*/base::Time::Now(),
                           /*response_time=*/base::Time::Now(),
                           /*expiration*/ base::Seconds(100), "/pattern*",
                           /*match_dest_string=*/"", /*id=*/"",
                           /*last_used_time*/ base::Time::Now(),
                           /*size=*/2000, SHA256HashValue({{0x00, 0x02}}),
                           /*disk_cache_key_token=*/token2,
                           /*primary_key_in_database=*/std::nullopt);
  auto result2 = RegisterDictionary(isolation_key2, dict2);
  dict2.set_primary_key_in_database(result2.primary_key_in_database());

  // Different frame origin, same top frame site.
  auto isolation_key3 =
      CreateIsolationKey("https://a4.example/", "https://a2.example/");
  auto token3 = base::UnguessableToken::Create();
  SharedDictionaryInfo dict3 =
      SharedDictionaryInfo(GURL("https://a3.example/dict"),
                           /*last_fetch_time=*/base::Time::Now(),
                           /*response_time=*/base::Time::Now(),
                           /*expiration*/ base::Seconds(100), "/pattern*",
                           /*match_dest_string=*/"", /*id=*/"",
                           /*last_used_time*/ base::Time::Now(),
                           /*size=*/4000, SHA256HashValue({{0x00, 0x03}}),
                           /*disk_cache_key_token=*/token3,
                           /*primary_key_in_database=*/std::nullopt);
  auto result3 = RegisterDictionary(isolation_key3, dict3);
  dict3.set_primary_key_in_database(result3.primary_key_in_database());

  // Different frame origin, different top frame site.
  auto isolation_key4 =
      CreateIsolationKey("https://a4.example/", "https://a5.example/");
  auto token4 = base::UnguessableToken::Create();
  SharedDictionaryInfo dict4 =
      SharedDictionaryInfo(GURL("https://a4.example/dict"),
                           /*last_fetch_time=*/base::Time::Now(),
                           /*response_time=*/base::Time::Now(),
                           /*expiration*/ base::Seconds(100), "/pattern*",
                           /*match_dest_string=*/"", /*id=*/"",
                           /*last_used_time*/ base::Time::Now(),
                           /*size=*/8000, SHA256HashValue({{0x00, 0x04}}),
                           /*disk_cache_key_token=*/token4,
                           /*primary_key_in_database=*/std::nullopt);
  auto result4 = RegisterDictionary(isolation_key4, dict4);
  dict4.set_primary_key_in_database(result4.primary_key_in_database());

  // Deletes dictionaries for `isolation_key_`. The result should be empty.
  EXPECT_TRUE(ClearDictionariesForIsolationKey(isolation_key_).empty());

  // Deletes dictionaries for `isolation_key1`.
  EXPECT_THAT(ClearDictionariesForIsolationKey(isolation_key1),
              ElementsAreArray({token1}));

  // Check the remaining dictionaries.
  EXPECT_THAT(GetAllDictionaries(),
              ElementsAre(Pair(isolation_key2, ElementsAreArray({dict2})),
                          Pair(isolation_key3, ElementsAreArray({dict3})),
                          Pair(isolation_key4, ElementsAreArray({dict4}))));
  // Check the total size of remaining dictionaries.
  EXPECT_EQ(dict2.size() + dict3.size() + dict4.size(),
            GetTotalDictionarySize());
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       ClearDictionariesForIsolationMultipleDictionaries) {
  CreateStore();

  auto isolation_key1 = CreateIsolationKey("https://a1.example/");
  auto token1_1 = base::UnguessableToken::Create();
  SharedDictionaryInfo dict1_1 =
      SharedDictionaryInfo(GURL("https://a1.example/dict1"),
                           /*last_fetch_time=*/base::Time::Now(),
                           /*response_time=*/base::Time::Now(),
                           /*expiration*/ base::Seconds(100), "/pattern1*",
                           /*match_dest_string=*/"", /*id=*/"",
                           /*last_used_time*/ base::Time::Now(),
                           /*size=*/1000, SHA256HashValue({{0x00, 0x01}}),
                           /*disk_cache_key_token=*/token1_1,
                           /*primary_key_in_database=*/std::nullopt);
  auto result1_1 = RegisterDictionary(isolation_key1, dict1_1);
  dict1_1.set_primary_key_in_database(result1_1.primary_key_in_database());

  auto token1_2 = base::UnguessableToken::Create();
  SharedDictionaryInfo dict1_2 =
      SharedDictionaryInfo(GURL("https://a1.example/dict1"),
                           /*last_fetch_time=*/base::Time::Now(),
                           /*response_time=*/base::Time::Now(),
                           /*expiration*/ base::Seconds(100), "/pattern2*",
                           /*match_dest_string=*/"", /*id=*/"",
                           /*last_used_time*/ base::Time::Now(),
                           /*size=*/2000, SHA256HashValue({{0x00, 0x02}}),
                           /*disk_cache_key_token=*/token1_2,
                           /*primary_key_in_database=*/std::nullopt);
  auto result1_2 = RegisterDictionary(isolation_key1, dict1_2);
  dict1_2.set_primary_key_in_database(result1_2.primary_key_in_database());

  auto isolation_key2 = CreateIsolationKey("https://a2.example/");
  auto token2 = base::UnguessableToken::Create();
  SharedDictionaryInfo dict2 =
      SharedDictionaryInfo(GURL("https://a2.example/dict"),
                           /*last_fetch_time=*/base::Time::Now(),
                           /*response_time=*/base::Time::Now(),
                           /*expiration*/ base::Seconds(100), "/pattern*",
                           /*match_dest_string=*/"", /*id=*/"",
                           /*last_used_time*/ base::Time::Now(),
                           /*size=*/4000, SHA256HashValue({{0x00, 0x03}}),
                           /*disk_cache_key_token=*/token2,
                           /*primary_key_in_database=*/std::nullopt);
  auto result2 = RegisterDictionary(isolation_key2, dict2);
  dict2.set_primary_key_in_database(result2.primary_key_in_database());

  // Deletes dictionaries for `isolation_key1`.
  EXPECT_THAT(ClearDictionariesForIsolationKey(isolation_key1),
              UnorderedElementsAreArray({token1_1, token1_2}));

  // Check the remaining dictionaries.
  EXPECT_THAT(GetAllDictionaries(),
              ElementsAre(Pair(isolation_key2, ElementsAreArray({dict2}))));
  // Check the total size of remaining dictionaries.
  EXPECT_EQ(dict2.size(), GetTotalDictionarySize());
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest, DeleteExpiredDictionaries) {
  CreateStore();

  const base::Time now = base::Time::Now();
  auto token1 = base::UnguessableToken::Create();
  SharedDictionaryInfo dict1 =
      SharedDictionaryInfo(GURL("https://a.example/dict"),
                           /*last_fetch_time=*/now, /*response_time=*/now,
                           /*expiration*/ base::Seconds(100), "/pattern*",
                           /*match_dest_string=*/"", /*id=*/"",
                           /*last_used_time*/ now,
                           /*size=*/1000, SHA256HashValue({{0x00, 0x01}}),
                           /*disk_cache_key_token=*/token1,
                           /*primary_key_in_database=*/std::nullopt);
  auto result1 = RegisterDictionary(isolation_key_, dict1);
  dict1.set_primary_key_in_database(result1.primary_key_in_database());

  auto token2 = base::UnguessableToken::Create();
  SharedDictionaryInfo dict2 =
      SharedDictionaryInfo(GURL("https://b.example/dict"),
                           /*last_fetch_time=*/now + base::Seconds(1),
                           /*response_time=*/now + base::Seconds(1),
                           /*expiration*/ base::Seconds(99), "/pattern*",
                           /*match_dest_string=*/"", /*id=*/"",
                           /*last_used_time*/ now,
                           /*size=*/3000, SHA256HashValue({{0x00, 0x02}}),
                           /*disk_cache_key_token=*/token2,
                           /*primary_key_in_database=*/std::nullopt);
  auto result2 = RegisterDictionary(isolation_key_, dict2);
  dict2.set_primary_key_in_database(result2.primary_key_in_database());

  auto token3 = base::UnguessableToken::Create();
  SharedDictionaryInfo dict3 =
      SharedDictionaryInfo(GURL("https://c.example/dict"),
                           /*last_fetch_time=*/now + base::Seconds(1),
                           /*response_time=*/now + base::Seconds(1),
                           /*expiration*/ base::Seconds(100), "/pattern*",
                           /*match_dest_string=*/"", /*id=*/"",
                           /*last_used_time*/ now,
                           /*size=*/5000, SHA256HashValue({{0x00, 0x03}}),
                           /*disk_cache_key_token=*/token3,
                           /*primary_key_in_database=*/std::nullopt);
  auto result3 = RegisterDictionary(isolation_key_, dict3);
  dict3.set_primary_key_in_database(result3.primary_key_in_database());

  auto token4 = base::UnguessableToken::Create();
  SharedDictionaryInfo dict4 =
      SharedDictionaryInfo(GURL("https://d.example/dict"),
                           /*last_fetch_time=*/now + base::Seconds(2),
                           /*response_time=*/now + base::Seconds(2),
                           /*expiration*/ base::Seconds(99), "/pattern*",
                           /*match_dest_string=*/"", /*id=*/"",
                           /*last_used_time*/ now,
                           /*size=*/7000, SHA256HashValue({{0x00, 0x04}}),
                           /*disk_cache_key_token=*/token4,
                           /*primary_key_in_database=*/std::nullopt);
  auto result4 = RegisterDictionary(isolation_key_, dict4);
  dict4.set_primary_key_in_database(result4.primary_key_in_database());

  // No matching dictionaries to be deleted.
  EXPECT_TRUE(DeleteExpiredDictionaries(now + base::Seconds(99)).empty());

  std::set<base::UnguessableToken> tokens =
      DeleteExpiredDictionaries(now + base::Seconds(100));
  // The dict1 and dict2 must be deleted.
  EXPECT_THAT(tokens, UnorderedElementsAreArray({token1, token2}));

  // Check the remaining dictionaries.
  EXPECT_THAT(GetAllDictionaries(),
              ElementsAre(Pair(isolation_key_,
                               UnorderedElementsAreArray({dict3, dict4}))));
  // Check the total size of remaining dictionaries.
  EXPECT_EQ(dict3.size() + dict4.size(), GetTotalDictionarySize());
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest, ProcessEvictionNotExceeded) {
  CreateStore();
  auto [dict1, dict2, dict3, dict4] =
      RegisterSharedDictionariesForProcessEvictionTest(store_.get(),
                                                       isolation_key_);
  // The current status:
  //   dict1: size=1000 last_used_time=now
  //   dict3: size=5000 last_used_time=now+2
  //   dict4: size=7000 last_used_time=now+3
  //   dict2: size=3000 last_used_time=now+4

  // No matching dictionaries to be deleted.
  EXPECT_TRUE(ProcessEviction(16000, 15000, 10, 9).empty());
  // Check the remaining dictionaries.
  EXPECT_THAT(
      GetAllDictionaries(),
      ElementsAre(Pair(isolation_key_, UnorderedElementsAreArray(
                                           {dict1, dict2, dict3, dict4}))));
  // Check the total size of remaining dictionaries.
  EXPECT_EQ(dict1.size() + dict2.size() + dict3.size() + dict4.size(),
            GetTotalDictionarySize());
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest, ProcessEvictionSizeExceeded) {
  CreateStore();
  auto [dict1, dict2, dict3, dict4] =
      RegisterSharedDictionariesForProcessEvictionTest(store_.get(),
                                                       isolation_key_);
  // The current status:
  //   dict1: size=1000 last_used_time=now
  //   dict3: size=5000 last_used_time=now+2
  //   dict4: size=7000 last_used_time=now+3
  //   dict2: size=3000 last_used_time=now+4

  std::set<base::UnguessableToken> tokens =
      ProcessEviction(15000, 10000, 10, 9);
  // The dict1 and dict3 must be deleted.
  EXPECT_THAT(tokens,
              UnorderedElementsAreArray({dict1.disk_cache_key_token(),
                                         dict3.disk_cache_key_token()}));
  // Check the remaining dictionaries.
  EXPECT_THAT(GetAllDictionaries(),
              ElementsAre(Pair(isolation_key_,
                               UnorderedElementsAreArray({dict4, dict2}))));
  // Check the total size of remaining dictionaries.
  EXPECT_EQ(dict4.size() + dict2.size(), GetTotalDictionarySize());
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       ProcessEvictionSizeExceededEvictiedUntilCountLowWatermark) {
  CreateStore();
  auto [dict1, dict2, dict3, dict4] =
      RegisterSharedDictionariesForProcessEvictionTest(store_.get(),
                                                       isolation_key_);
  // The current status:
  //   dict1: size=1000 last_used_time=now
  //   dict3: size=5000 last_used_time=now+2
  //   dict4: size=7000 last_used_time=now+3
  //   dict2: size=3000 last_used_time=now+4

  std::set<base::UnguessableToken> tokens =
      ProcessEviction(15000, 10000, 10, 1);
  // The dict1 and dict3 and dict4 must be deleted.
  EXPECT_THAT(tokens,
              UnorderedElementsAreArray({dict1.disk_cache_key_token(),
                                         dict3.disk_cache_key_token(),
                                         dict4.disk_cache_key_token()}));
  // Check the remaining dictionaries.
  EXPECT_THAT(
      GetAllDictionaries(),
      ElementsAre(Pair(isolation_key_, UnorderedElementsAreArray({dict2}))));
  // Check the total size of remaining dictionaries.
  EXPECT_EQ(dict2.size(), GetTotalDictionarySize());
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       ProcessEvictionCountExceeded) {
  CreateStore();

  auto [dict1, dict2, dict3, dict4] =
      RegisterSharedDictionariesForProcessEvictionTest(store_.get(),
                                                       isolation_key_);
  // The current status:
  //   dict1: size=1000 last_used_time=now
  //   dict3: size=5000 last_used_time=now+2
  //   dict4: size=7000 last_used_time=now+3
  //   dict2: size=3000 last_used_time=now+4

  std::set<base::UnguessableToken> tokens = ProcessEviction(20000, 20000, 3, 2);
  // The dict1 and dict3 must be deleted.
  EXPECT_THAT(tokens,
              UnorderedElementsAreArray({dict1.disk_cache_key_token(),
                                         dict3.disk_cache_key_token()}));
  // Check the remaining dictionaries.
  EXPECT_THAT(GetAllDictionaries(),
              ElementsAre(Pair(isolation_key_,
                               UnorderedElementsAreArray({dict4, dict2}))));
  // Check the total size of remaining dictionaries.
  EXPECT_EQ(dict4.size() + dict2.size(), GetTotalDictionarySize());
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       ProcessEvictionCountExceededEvictedUntilSizeLowWaterMark) {
  CreateStore();

  auto [dict1, dict2, dict3, dict4] =
      RegisterSharedDictionariesForProcessEvictionTest(store_.get(),
                                                       isolation_key_);
  // The current status:
  //   dict1: size=1000 last_used_time=now
  //   dict3: size=5000 last_used_time=now+2
  //   dict4: size=7000 last_used_time=now+3
  //   dict2: size=3000 last_used_time=now+4

  std::set<base::UnguessableToken> tokens = ProcessEviction(20000, 3000, 3, 2);
  // The dict1 and dict3 and dict4 must be deleted.
  EXPECT_THAT(tokens,
              UnorderedElementsAreArray({dict1.disk_cache_key_token(),
                                         dict3.disk_cache_key_token(),
                                         dict4.disk_cache_key_token()}));
  // Check the remaining dictionaries.
  EXPECT_THAT(
      GetAllDictionaries(),
      ElementsAre(Pair(isolation_key_, UnorderedElementsAreArray({dict2}))));
  // Check the total size of remaining dictionaries.
  EXPECT_EQ(dict2.size(), GetTotalDictionarySize());
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest, ProcessEvictionZeroMaxSize) {
  CreateStore();

  auto [dict1, dict2, dict3, dict4] =
      RegisterSharedDictionariesForProcessEvictionTest(store_.get(),
                                                       isolation_key_);
  // The current status:
  //   dict1: size=1000 last_used_time=now
  //   dict3: size=5000 last_used_time=now+2
  //   dict4: size=7000 last_used_time=now+3
  //   dict2: size=3000 last_used_time=now+4

  EXPECT_TRUE(ProcessEviction(0, 0, 4, 2).empty());

  std::set<base::UnguessableToken> tokens = ProcessEviction(0, 0, 3, 2);
  // The dict1 and dict3 and dict4 must be deleted.
  EXPECT_THAT(tokens,
              UnorderedElementsAreArray({dict1.disk_cache_key_token(),
                                         dict3.disk_cache_key_token()}));
  // Check the remaining dictionaries.
  EXPECT_THAT(GetAllDictionaries(),
              ElementsAre(Pair(isolation_key_,
                               UnorderedElementsAreArray({dict2, dict4}))));
  // Check the total size of remaining dictionaries.
  EXPECT_EQ(dict2.size() + dict4.size(), GetTotalDictionarySize());
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest, ProcessEvictionDeletesAll) {
  CreateStore();

  const base::Time now = base::Time::Now();
  auto token1 = base::UnguessableToken::Create();
  SharedDictionaryInfo dict1 =
      SharedDictionaryInfo(GURL("https://a.example/dict"),
                           /*last_fetch_time=*/now, /*response_time=*/now,
                           /*expiration*/ base::Seconds(100), "/pattern*",
                           /*match_dest_string=*/"", /*id=*/"",
                           /*last_used_time*/ now,
                           /*size=*/1000, SHA256HashValue({{0x00, 0x01}}),
                           /*disk_cache_key_token=*/token1,
                           /*primary_key_in_database=*/std::nullopt);
  auto result1 = RegisterDictionary(isolation_key_, dict1);
  dict1.set_primary_key_in_database(result1.primary_key_in_database());

  auto token2 = base::UnguessableToken::Create();
  SharedDictionaryInfo dict2 =
      SharedDictionaryInfo(GURL("https://b.example/dict"),
                           /*last_fetch_time=*/now, /*response_time=*/now,
                           /*expiration*/ base::Seconds(100), "/pattern*",
                           /*match_dest_string=*/"", /*id=*/"",
                           /*last_used_time*/ now + base::Seconds(1),
                           /*size=*/3000, SHA256HashValue({{0x00, 0x02}}),
                           /*disk_cache_key_token=*/token2,
                           /*primary_key_in_database=*/std::nullopt);
  auto result2 = RegisterDictionary(isolation_key_, dict2);
  dict2.set_primary_key_in_database(result2.primary_key_in_database());

  // The current status:
  //   dict1: size=1000 last_used_time=now
  //   dict2: size=3000 last_used_time=now+1

  std::set<base::UnguessableToken> tokens = ProcessEviction(1000, 900, 10, 9);
  // The dict1 and dict2 must be deleted.
  EXPECT_THAT(tokens, UnorderedElementsAreArray({token1, token2}));

  EXPECT_TRUE(GetAllDictionaries().empty());

  // Check the total size of remaining dictionaries.
  EXPECT_EQ(0u, GetTotalDictionarySize());
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest, GetAllDiskCacheKeyTokens) {
  CreateStore();
  EXPECT_TRUE(GetAllDiskCacheKeyTokens().empty());

  auto token1 = base::UnguessableToken::Create();
  SharedDictionaryInfo dict1 = SharedDictionaryInfo(
      GURL("https://a.example/dict"),
      /*last_fetch_time=*/base::Time::Now() - base::Seconds(4),
      /*response_time=*/base::Time::Now() - base::Seconds(4),
      /*expiration*/ base::Seconds(100), "/pattern*", /*match_dest_string=*/"",
      /*id=*/"",
      /*last_used_time*/ base::Time::Now(),
      /*size=*/1000, SHA256HashValue({{0x00, 0x01}}),
      /*disk_cache_key_token=*/token1,
      /*primary_key_in_database=*/std::nullopt);
  RegisterDictionary(isolation_key_, dict1);

  EXPECT_THAT(GetAllDiskCacheKeyTokens(), ElementsAreArray({token1}));

  auto token2 = base::UnguessableToken::Create();
  SharedDictionaryInfo dict2 = SharedDictionaryInfo(
      GURL("https://b.example/dict"),
      /*last_fetch_time=*/base::Time::Now() - base::Seconds(3),
      /*response_time=*/base::Time::Now() - base::Seconds(3),
      /*expiration*/ base::Seconds(100), "/pattern*", /*match_dest_string=*/"",
      /*id=*/"",
      /*last_used_time*/ base::Time::Now(),
      /*size=*/3000, SHA256HashValue({{0x00, 0x02}}),
      /*disk_cache_key_token=*/token2,
      /*primary_key_in_database=*/std::nullopt);
  RegisterDictionary(isolation_key_, dict2);

  EXPECT_THAT(GetAllDiskCacheKeyTokens(),
              UnorderedElementsAreArray({token1, token2}));
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       DeleteDictionariesByDiskCacheKeyTokens) {
  CreateStore();
  EXPECT_TRUE(GetAllDiskCacheKeyTokens().empty());

  auto token1 = base::UnguessableToken::Create();
  SharedDictionaryInfo dict1 = SharedDictionaryInfo(
      GURL("https://a.example/dict"),
      /*last_fetch_time=*/base::Time::Now() - base::Seconds(4),
      /*response_time=*/base::Time::Now() - base::Seconds(4),
      /*expiration*/ base::Seconds(100), "/pattern*", /*match_dest_string=*/"",
      /*id=*/"",
      /*last_used_time*/ base::Time::Now(),
      /*size=*/1000, SHA256HashValue({{0x00, 0x01}}),
      /*disk_cache_key_token=*/token1,
      /*primary_key_in_database=*/std::nullopt);
  auto result1 = RegisterDictionary(isolation_key_, dict1);
  dict1.set_primary_key_in_database(result1.primary_key_in_database());

  EXPECT_THAT(GetAllDiskCacheKeyTokens(), ElementsAreArray({token1}));

  auto token2 = base::UnguessableToken::Create();
  SharedDictionaryInfo dict2 = SharedDictionaryInfo(
      GURL("https://b.example/dict"),
      /*last_fetch_time=*/base::Time::Now() - base::Seconds(3),
      /*response_time=*/base::Time::Now() - base::Seconds(3),
      /*expiration*/ base::Seconds(100), "/pattern*", /*match_dest_string=*/"",
      /*id=*/"",
      /*last_used_time*/ base::Time::Now(),
      /*size=*/3000, SHA256HashValue({{0x00, 0x02}}),
      /*disk_cache_key_token=*/token2,
      /*primary_key_in_database=*/std::nullopt);
  RegisterDictionary(isolation_key_, dict2);
  auto result2 = RegisterDictionary(isolation_key_, dict2);
  dict2.set_primary_key_in_database(result2.primary_key_in_database());

  auto token3 = base::UnguessableToken::Create();
  SharedDictionaryInfo dict3 = SharedDictionaryInfo(
      GURL("https://c.example/dict"),
      /*last_fetch_time=*/base::Time::Now() - base::Seconds(2),
      /*response_time=*/base::Time::Now() - base::Seconds(2),
      /*expiration*/ base::Seconds(100), "/pattern*", /*match_dest_string=*/"",
      /*id=*/"",
      /*last_used_time*/ base::Time::Now(),
      /*size=*/5000, SHA256HashValue({{0x00, 0x03}}),
      /*disk_cache_key_token=*/token3,
      /*primary_key_in_database=*/std::nullopt);
  auto result3 = RegisterDictionary(isolation_key_, dict3);
  dict3.set_primary_key_in_database(result3.primary_key_in_database());

  auto token4 = base::UnguessableToken::Create();
  SharedDictionaryInfo dict4 = SharedDictionaryInfo(
      GURL("https://d.example/dict"),
      /*last_fetch_time=*/base::Time::Now() - base::Seconds(1),
      /*response_time=*/base::Time::Now() - base::Seconds(1),
      /*expiration*/ base::Seconds(100), "/pattern*", /*match_dest_string=*/"",
      /*id=*/"",
      /*last_used_time*/ base::Time::Now(),
      /*size=*/7000, SHA256HashValue({{0x00, 0x04}}),
      /*disk_cache_key_token=*/token4,
      /*primary_key_in_database=*/std::nullopt);
  auto result4 = RegisterDictionary(isolation_key_, dict4);
  dict4.set_primary_key_in_database(result4.primary_key_in_database());

  EXPECT_EQ(SQLitePersistentSharedDictionaryStore::Error::kOk,
            DeleteDictionariesByDiskCacheKeyTokens({}));

  EXPECT_THAT(
      GetAllDictionaries(),
      ElementsAre(Pair(isolation_key_, UnorderedElementsAreArray(
                                           {dict1, dict2, dict3, dict4}))));
  EXPECT_EQ(16000u, GetTotalDictionarySize());

  EXPECT_EQ(SQLitePersistentSharedDictionaryStore::Error::kOk,
            DeleteDictionariesByDiskCacheKeyTokens({token1}));

  // dict1 must have been deleted.
  EXPECT_THAT(GetAllDictionaries(),
              ElementsAre(Pair(isolation_key_, UnorderedElementsAreArray(
                                                   {dict2, dict3, dict4}))));
  EXPECT_EQ(15000u, GetTotalDictionarySize());

  EXPECT_EQ(SQLitePersistentSharedDictionaryStore::Error::kOk,
            DeleteDictionariesByDiskCacheKeyTokens({token2, token3}));

  // dict2 and dict3 must have been deleted.
  EXPECT_THAT(
      GetAllDictionaries(),
      ElementsAre(Pair(isolation_key_, UnorderedElementsAreArray({dict4}))));
  EXPECT_EQ(7000u, GetTotalDictionarySize());

  // Call DeleteDictionariesByDiskCacheKeyTokens() with no-maching token.
  EXPECT_EQ(SQLitePersistentSharedDictionaryStore::Error::kOk,
            DeleteDictionariesByDiskCacheKeyTokens(
                {base::UnguessableToken::Create()}));
  EXPECT_THAT(
      GetAllDictionaries(),
      ElementsAre(Pair(isolation_key_, UnorderedElementsAreArray({dict4}))));
  EXPECT_EQ(7000u, GetTotalDictionarySize());

  EXPECT_EQ(SQLitePersistentSharedDictionaryStore::Error::kOk,
            DeleteDictionariesByDiskCacheKeyTokens({token4}));
  // dict4 must have been deleted.
  EXPECT_TRUE(GetAllDictionaries().empty());
  EXPECT_EQ(0u, GetTotalDictionarySize());
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       UpdateDictionaryLastFetchTime) {
  CreateStore();
  auto register_dictionary_result =
      RegisterDictionary(isolation_key_, dictionary_info_);

  std::vector<SharedDictionaryInfo> dicts1 = GetDictionaries(isolation_key_);
  ASSERT_EQ(1u, dicts1.size());

  // Move the clock forward by 1 second.
  FastForwardBy(base::Seconds(1));

  const base::Time updated_last_fetch_time = base::Time::Now();
  // Update the last fetch time.
  EXPECT_EQ(SQLitePersistentSharedDictionaryStore::Error::kOk,
            UpdateDictionaryLastFetchTime(
                register_dictionary_result.primary_key_in_database(),
                /*last_fetch_time=*/updated_last_fetch_time));

  std::vector<SharedDictionaryInfo> dicts2 = GetDictionaries(isolation_key_);
  ASSERT_EQ(1u, dicts2.size());

  EXPECT_EQ(dicts1[0].last_fetch_time(), dictionary_info_.last_fetch_time());
  EXPECT_EQ(dicts2[0].last_fetch_time(), updated_last_fetch_time);
  EXPECT_NE(dicts1[0].last_fetch_time(), dicts2[0].last_fetch_time());
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       UpdateDictionaryLastUsedTime) {
  CreateStore();
  auto register_dictionary_result =
      RegisterDictionary(isolation_key_, dictionary_info_);

  std::vector<SharedDictionaryInfo> dicts1 = GetDictionaries(isolation_key_);
  ASSERT_EQ(1u, dicts1.size());

  // Move the clock forward by 1 second.
  FastForwardBy(base::Seconds(1));

  std::vector<SharedDictionaryInfo> dicts2 = GetDictionaries(isolation_key_);
  ASSERT_EQ(1u, dicts2.size());

  EXPECT_EQ(dicts1[0].last_used_time(), dicts2[0].last_used_time());

  // Move the clock forward by 1 second.
  FastForwardBy(base::Seconds(1));
  base::Time updated_last_used_time = base::Time::Now();
  store_->UpdateDictionaryLastUsedTime(
      register_dictionary_result.primary_key_in_database(),
      updated_last_used_time);

  std::vector<SharedDictionaryInfo> dicts3 = GetDictionaries(isolation_key_);
  ASSERT_EQ(1u, dicts3.size());
  EXPECT_EQ(updated_last_used_time, dicts3[0].last_used_time());
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest,
       MassiveUpdateDictionaryLastUsedTime) {
  CreateStore();
  auto register_dictionary_result =
      RegisterDictionary(isolation_key_, dictionary_info_);
  base::Time updated_last_used_time;
  for (size_t i = 0; i < 1000; ++i) {
    // Move the clock forward by 10 millisecond.
    FastForwardBy(base::Milliseconds(10));
    updated_last_used_time = base::Time::Now();
    store_->UpdateDictionaryLastUsedTime(
        register_dictionary_result.primary_key_in_database(),
        updated_last_used_time);
  }

  std::vector<SharedDictionaryInfo> dicts3 = GetDictionaries(isolation_key_);
  ASSERT_EQ(1u, dicts3.size());
  EXPECT_EQ(updated_last_used_time, dicts3[0].last_used_time());
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest, MigrateFromV1ToV3) {
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(GetStroeFilePath()));
    CreateV1Schema(&db);
    ASSERT_EQ(GetDBCurrentVersionNumber(&db), 1);
  }
  CreateStore();
  EXPECT_EQ(GetTotalDictionarySize(), 0u);
  DestroyStore();
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(GetStroeFilePath()));
    ASSERT_EQ(GetDBCurrentVersionNumber(&db), 3);
  }
}

TEST_F(SQLitePersistentSharedDictionaryStoreTest, MigrateFromV2ToV3) {
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(GetStroeFilePath()));
    CreateV2Schema(&db);
    ASSERT_EQ(GetDBCurrentVersionNumber(&db), 2);
  }
  CreateStore();
  EXPECT_EQ(GetTotalDictionarySize(), 0u);
  DestroyStore();
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(GetStroeFilePath()));
    ASSERT_EQ(GetDBCurrentVersionNumber(&db), 3);
  }
}

}  // namespace net
