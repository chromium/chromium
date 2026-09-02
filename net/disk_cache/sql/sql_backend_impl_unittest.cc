// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_backend_impl.h"

#include <cstdint>
#include <variant>

#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/hash/hash.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial.h"
#include "base/pickle.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/test_file_util.h"
#include "base/test/test_future.h"
#include "components/performance_manager/scenario_api/performance_scenario_test_support.h"
#include "net/base/features.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
#include "net/base/schemeful_site.h"
#include "net/base/test_completion_callback.h"
#include "net/disk_cache/backend_cleanup_tracker.h"
#include "net/disk_cache/disk_cache_test_util.h"
#include "net/disk_cache/sql/mock_shared_cache_client_remote.h"
#include "net/disk_cache/sql/sql_async_task_manager.h"
#include "net/disk_cache/sql/sql_backend_constants.h"
#include "net/disk_cache/sql/sql_entry_impl.h"
#include "net/disk_cache/sql/sql_shared_cache.h"
#include "net/disk_cache/sql/sql_shared_cache_handle.h"
#include "net/disk_cache/sql/sql_shared_cache_isolated_database_reader.h"
#include "net/disk_cache/sql/sql_shared_cache_manager.h"
#include "net/http/http_cache.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using net::test::IsError;
using net::test::IsOk;

using performance_scenarios::InputScenario;
using performance_scenarios::LoadingScenario;
using performance_scenarios::PerformanceScenarioTestHelper;
using performance_scenarios::ScenarioScope;

namespace disk_cache {

using OperationHandle = ExclusiveOperationCoordinator::OperationHandle;

namespace {

using testing::ElementsAre;
using testing::Pair;
using FakeIndexFileError = SqlBackendImpl::FakeIndexFileError;

// Default max cache size for tests, 10 MB.
inline constexpr int64_t kDefaultMaxBytes = 10 * 1024 * 1024;

// Helper to create a new entry and write a data.
Entry* CreateEntryAndWriteData(SqlBackendImpl* backend,
                               const std::string& key,
                               const std::string& data) {
  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result = cb_create.GetResult(
      backend->CreateEntry(key, net::HIGHEST, cb_create.callback()));
  CHECK_EQ(create_result.net_error(), net::OK);
  auto* entry = create_result.ReleaseEntry();
  auto buffer = base::MakeRefCounted<net::StringIOBuffer>(data);
  net::TestCompletionCallback cb_write;
  EXPECT_EQ(
      cb_write.GetResult(entry->WriteData(1, 0, buffer.get(), buffer->size(),
                                          cb_write.callback(), false)),
      static_cast<int>(buffer->size()));
  return entry;
}

// Helper to read data and verify its content.
void ReadAndVerifyData(Entry* entry, std::string_view expected_data) {
  auto read_buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(expected_data.size() + 1);
  net::TestCompletionCallback cb_read;
  int rv_read = entry->ReadData(1, 0, read_buffer.get(), read_buffer->size(),
                                cb_read.callback());
  EXPECT_EQ(cb_read.GetResult(rv_read), static_cast<int>(expected_data.size()));
  EXPECT_EQ(std::string_view(read_buffer->data(), expected_data.size()),
            expected_data);
}

size_t GetShardCount() {
  return std::max(std::min(net::features::kSqlDiskCacheShardCount.Get(), 255),
                  1);
}
std::string GetExpectedFakeIndexContents() {
  base::FieldTrial* backend_field_trial =
      net::features::kDiskCacheBackendResetCacheOnGroupChange.Get()
          ? base::FeatureList::GetFieldTrial(
                net::features::kDiskCacheBackendExperiment)
          : nullptr;
  return base::StrCat(
      {kSqlBackendFakeIndexPrefix,
       net::features::kSqlDiskCacheWalMode.Get() ? "Wal" : "Truncate",
       base::NumberToString(GetShardCount()),
       backend_field_trial ? backend_field_trial->group_name() : ""});
}

class SqlBackendImplTest : public net::TestWithTaskEnvironment {
 public:
  SqlBackendImplTest()
      : net::TestWithTaskEnvironment(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~SqlBackendImplTest() override = default;

  // Sets up a temporary directory and a background task runner for each test.
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

 protected:
  void RunSparseDataExceedsMaxFileSizeTest(bool doom_entry);

  std::unique_ptr<SqlBackendImpl> CreateBackend(
      scoped_refptr<BackendCleanupTracker> cleanup_tracker = nullptr) {
    return std::make_unique<SqlBackendImpl>(
        temp_dir_.GetPath(), kDefaultMaxBytes, net::CacheType::DISK_CACHE,
        std::move(cleanup_tracker));
  }

  void WaitForCleanup(scoped_refptr<BackendCleanupTracker> cleanup_tracker) {
    CHECK(cleanup_tracker);
    base::RunLoop run_loop;
    cleanup_tracker->AddPostCleanupCallback(run_loop.QuitClosure());
    cleanup_tracker = nullptr;
    run_loop.Run();
  }

  std::unique_ptr<SqlBackendImpl> CreateBackendAndInit(
      int64_t max_bytes = kDefaultMaxBytes) {
    auto backend = std::make_unique<SqlBackendImpl>(
        temp_dir_.GetPath(), max_bytes, net::CacheType::DISK_CACHE,
        /*cleanup_tracker=*/nullptr);
    base::test::TestFuture<int> future;
    backend->Init(future.GetCallback());
    CHECK_EQ(future.Get(), net::OK);
    return backend;
  }
  void WaitUntilInitialized(SqlBackendImpl& backend,
                            const scoped_refptr<EntryDbHandle>& db_handle) {
    CHECK(db_handle);
    while (!db_handle->IsFinished()) {
      backend.RunUntilAllTasksCompleteForTest();
    }
  }

  void FlushQueueInTaskRunners(
      const std::vector<scoped_refptr<base::SequencedTaskRunner>>&
          task_runners) {
    for (auto& runner : task_runners) {
      base::RunLoop run_loop;
      runner->PostTask(FROM_HERE, run_loop.QuitClosure());
      run_loop.Run();
    }
  }

  bool LoadInMemoryIndex(SqlBackendImpl& backend) {
    auto* store = backend.GetSqlStoreForTest();
    base::test::TestFuture<SqlPersistentStore::Error> future;
    store->MaybeLoadInMemoryIndex(future.GetCallback());
    return future.Get() == SqlPersistentStore::Error::kOk;
  }

  SqlSharedCacheResourceId CreateEntryInSharedCache(SqlBackendImpl& backend,
                                                    std::string_view key,
                                                    std::string_view data) {
    TestEntryResultCompletionCallback cb_create;
    disk_cache::EntryResult create_result =
        cb_create.GetResult(backend.CreateEntry(std::string(key), net::HIGHEST,
                                                cb_create.callback()));
    EXPECT_THAT(create_result.net_error(), IsOk());
    auto* entry = create_result.ReleaseEntry();
    CHECK(entry);

    net::HttpResponseInfo response_info;
    response_info.response_time = base::Time::Now();
    response_info.headers =
        base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\0\0");
    auto pickle = response_info.MakePickle(false, false);
    std::string pickle_data(reinterpret_cast<const char*>(pickle->data()),
                            pickle->size());
    auto pickle_buffer = base::MakeRefCounted<net::StringIOBuffer>(pickle_data);
    net::TestCompletionCallback cb_write_pickle;
    EXPECT_EQ(cb_write_pickle.GetResult(
                  entry->WriteData(0, 0, pickle_buffer.get(), pickle->size(),
                                   cb_write_pickle.callback(), false)),
              static_cast<int>(pickle->size()));

    auto write_buf = base::MakeRefCounted<net::IOBufferWithSize>(data.size());
    write_buf->span().copy_from(base::as_byte_span(data));
    net::TestCompletionCallback cb_write;
    EXPECT_EQ(
        cb_write.GetResult(entry->WriteData(1, 0, write_buf.get(), data.size(),
                                            cb_write.callback(), false)),
        static_cast<int>(data.size()));
    entry->Close();
    backend.RunUntilAllTasksCompleteForTest();

    GURL url("https://example.com");
    net::SchemefulSite site(url);
    net::NetworkIsolationKey nik(site, site);
    backend.OnEntryEligibleForSharedCache(
        std::string(key), url,
        std::make_unique<net::HttpResponseInfo>(response_info), nik);

    base::RunLoop process_run_loop;
    backend.ProcessSharedCacheEligibleEntriesForTest(
        base::ScopedClosureRunner(process_run_loop.QuitClosure()),
        base::NullCallback());
    process_run_loop.Run();
    backend.RunUntilAllTasksCompleteForTest();

    CacheEntryKey cache_key{std::string(key)};
    auto* store = backend.GetSqlStoreForTest();
    base::test::TestFuture<SqlPersistentStore::EntryInfoOrError> open_future;
    store->OpenEntry(cache_key, open_future.GetCallback());
    auto open_res = open_future.Take();
    CHECK(open_res.has_value());
    return open_res->shared_cache_resource_id.value_or(
        SqlSharedCacheResourceId{});
  }

  // Gets the total size of all entries.
  int64_t GetSizeOfAllEntries(SqlBackendImpl& backend) {
    return backend.GetSqlStoreForTest()->GetSizeOfAllEntries();
  }

  // Opens the database for a specific shard and returns the count of blobs
  // associated with a given resource ID.
  int64_t OpenDatabaseAndGetBlobsCount(SqlPersistentStore::ShardId shard_id,
                                       SqlPersistentStore::ResId res_id) {
    auto db = std::make_unique<sql::Database>(
        sql::DatabaseOptions()
#if BUILDFLAG(IS_WIN)
            .set_exclusive_database_file_lock(true)
#endif  // IS_WIN
            .set_wal_mode(true),
        sql::Database::Tag("HttpCacheDiskCache"));
    CHECK(db->Open(temp_dir_.GetPath().AppendASCII(
        base::StrCat({kSqlBackendDatabaseFileNamePrefix,
                      base::NumberToString(shard_id.value())}))));
    sql::Statement s(
        db->GetUniqueStatement("SELECT COUNT(*) FROM blobs where res_id = ?"));
    s.BindInt64(0, res_id.value());
    CHECK(s.Step());
    return s.ColumnInt64(0);
  }

  void RunDelayedPostInitializationTasksTest();

  base::ScopedTempDir temp_dir_;
};

TEST_F(SqlBackendImplTest, InitWithNoFakeIndexFile) {
  const std::string expected_contents = GetExpectedFakeIndexContents();
  base::HistogramTester histogram_tester;
  auto backend = CreateBackend();
  base::test::TestFuture<int> future;
  backend->Init(future.GetCallback());
  ASSERT_EQ(future.Get(), net::OK);
  histogram_tester.ExpectUniqueSample("Net.SqlDiskCache.FakeIndexFileError",
                                      FakeIndexFileError::kOkNew, 1);

  base::FilePath file_path =
      temp_dir_.GetPath().Append(kSqlBackendFakeIndexFileName);
  const std::optional<int64_t> file_size = base::GetFileSize(file_path);
  ASSERT_TRUE(file_size.has_value());
  EXPECT_EQ(*file_size, expected_contents.size());
  std::vector<uint8_t> contents(expected_contents.size());
  ASSERT_TRUE(base::ReadFile(file_path, contents));
  EXPECT_EQ(contents, base::as_byte_span(expected_contents));
}

TEST_F(SqlBackendImplTest, InitWithFakeIndexFile) {
  const std::string expected_contents = GetExpectedFakeIndexContents();
  base::HistogramTester histogram_tester;
  base::FilePath file_path =
      temp_dir_.GetPath().Append(kSqlBackendFakeIndexFileName);
  ASSERT_TRUE(
      base::WriteFile(file_path, base::as_byte_span(expected_contents)));

  auto backend = CreateBackend();
  base::test::TestFuture<int> future;
  backend->Init(future.GetCallback());
  ASSERT_EQ(future.Get(), net::OK);
  histogram_tester.ExpectUniqueSample("Net.SqlDiskCache.FakeIndexFileError",
                                      FakeIndexFileError::kOkExisting, 1);
}

TEST_F(SqlBackendImplTest, ExperimentGroupChangeResetsCacheWhenParamSet) {
  {
    AddScopedFeatureList().InitFromCommandLine(
        "DiskCacheBackendExperiment<DiskCacheBackendExperiment.GroupA:"
        "DiskCacheBackendResetCacheOnGroupChange/true",
        "FeatureParamWithCache");

    auto cleanup_tracker = BackendCleanupTracker::TryCreate(temp_dir_.GetPath(),
                                                            base::DoNothing());
    CHECK(cleanup_tracker);

    // Create and init backend. This should create fake index with "GroupA".
    auto backend = CreateBackend(cleanup_tracker);
    base::test::TestFuture<int> future;
    backend->Init(future.GetCallback());
    ASSERT_EQ(future.Get(), net::OK);

    backend.reset();
    WaitForCleanup(std::move(cleanup_tracker));
  }

  {
    AddScopedFeatureList().InitFromCommandLine(
        "DiskCacheBackendExperiment<DiskCacheBackendExperiment.GroupB:"
        "DiskCacheBackendResetCacheOnGroupChange/true",
        "FeatureParamWithCache");

    // Initialize backend on same directory. It should fail because the group
    // changed and reset on group change is enabled.
    auto backend = CreateBackend();
    base::test::TestFuture<int> future;
    base::HistogramTester histogram_tester;
    backend->Init(future.GetCallback());
    EXPECT_EQ(future.Get(), net::ERR_FAILED);

    // "GroupA" and "GroupB" have same length, so it should be
    // kWrongMagicNumber.
    histogram_tester.ExpectUniqueSample("Net.SqlDiskCache.FakeIndexFileError",
                                        FakeIndexFileError::kWrongMagicNumber,
                                        1);
  }
}

TEST_F(SqlBackendImplTest,
       ExperimentGroupChangeDoesNotResetCacheWhenParamNotSet) {
  {
    AddScopedFeatureList().InitFromCommandLine(
        "DiskCacheBackendExperiment<TrialC.GroupA:dummy/1",
        "FeatureParamWithCache");

    auto cleanup_tracker = BackendCleanupTracker::TryCreate(temp_dir_.GetPath(),
                                                            base::DoNothing());
    CHECK(cleanup_tracker);

    // Create and init backend.
    auto backend = CreateBackend(cleanup_tracker);
    base::test::TestFuture<int> future;
    backend->Init(future.GetCallback());
    ASSERT_EQ(future.Get(), net::OK);

    backend.reset();
    WaitForCleanup(std::move(cleanup_tracker));
  }

  {
    AddScopedFeatureList().InitFromCommandLine(
        "DiskCacheBackendExperiment<TrialD.GroupB:dummy/1",
        "FeatureParamWithCache");

    // Initialize backend on same directory. It should succeed because reset on
    // group change is disabled by default.
    auto backend = CreateBackend();
    base::test::TestFuture<int> future;
    base::HistogramTester histogram_tester;
    backend->Init(future.GetCallback());
    EXPECT_EQ(future.Get(), net::OK);

    histogram_tester.ExpectUniqueSample("Net.SqlDiskCache.FakeIndexFileError",
                                        FakeIndexFileError::kOkExisting, 1);
  }
}

TEST_F(SqlBackendImplTest, WalModeChangeResetsCache) {
  {
    AddScopedFeatureList().InitAndEnableFeatureWithParameters(
        net::features::kDiskCacheBackendExperiment,
        {{"SqlDiskCacheWalMode", "false"}});

    auto cleanup_tracker = BackendCleanupTracker::TryCreate(temp_dir_.GetPath(),
                                                            base::DoNothing());
    CHECK(cleanup_tracker);

    auto backend = CreateBackend(cleanup_tracker);
    base::test::TestFuture<int> future;
    backend->Init(future.GetCallback());
    ASSERT_EQ(future.Get(), net::OK);

    backend.reset();
    WaitForCleanup(std::move(cleanup_tracker));
  }

  {
    AddScopedFeatureList().InitAndEnableFeatureWithParameters(
        net::features::kDiskCacheBackendExperiment,
        {{"SqlDiskCacheWalMode", "true"}});

    auto backend = CreateBackend();
    base::test::TestFuture<int> future;
    base::HistogramTester histogram_tester;
    backend->Init(future.GetCallback());
    EXPECT_EQ(future.Get(), net::ERR_FAILED);

    histogram_tester.ExpectUniqueSample("Net.SqlDiskCache.FakeIndexFileError",
                                        FakeIndexFileError::kWrongFileSize, 1);
  }
}

TEST_F(SqlBackendImplTest, SerialInit) {
  AddScopedFeatureList().InitAndEnableFeatureWithParameters(
      net::features::kDiskCacheBackendExperiment,
      {{"SqlDiskCacheSerialInitialize", "true"},
       {"SqlDiskCacheShardCount", "2"}});

  auto backend = CreateBackend();
  base::test::TestFuture<int> future;
  base::HistogramTester histogram_tester;
  backend->Init(future.GetCallback());
  ASSERT_EQ(future.Get(), net::OK);

  histogram_tester.ExpectTotalCount("Net.SqlDiskCache.Init.SuccessTime", 1);
  histogram_tester.ExpectTotalCount("Net.SqlDiskCache.Init.FailureTime", 0);
}

TEST_F(SqlBackendImplTest, SerialInitShardFail) {
  AddScopedFeatureList().InitAndEnableFeatureWithParameters(
      net::features::kDiskCacheBackendExperiment,
      {{"SqlDiskCacheSerialInitialize", "true"},
       {"SqlDiskCacheShardCount", "2"}});

  auto backend = CreateBackend();
  // Fail the second shard.
  backend->GetSqlStoreForTest()->SetSimulateDbShardFailureForTesting(1, true);

  base::test::TestFuture<int> future;
  base::HistogramTester histogram_tester;
  backend->Init(future.GetCallback());
  ASSERT_EQ(future.Get(), net::ERR_FAILED);

  histogram_tester.ExpectTotalCount("Net.SqlDiskCache.Init.SuccessTime", 0);
  histogram_tester.ExpectTotalCount("Net.SqlDiskCache.Init.FailureTime", 1);
}

TEST_F(SqlBackendImplTest, SerialInitFail) {
  AddScopedFeatureList().InitAndEnableFeatureWithParameters(
      net::features::kDiskCacheBackendExperiment,
      {{"SqlDiskCacheSerialInitialize", "true"}});

  auto backend = CreateBackend();
  // Make fake index file check fail by creating a directory where the file
  // should be.
  base::CreateDirectory(
      temp_dir_.GetPath().Append(kSqlBackendFakeIndexFileName));

  base::test::TestFuture<int> future;
  base::HistogramTester histogram_tester;
  backend->Init(future.GetCallback());
  ASSERT_EQ(future.Get(), net::ERR_FAILED);
  histogram_tester.ExpectTotalCount("Net.SqlDiskCache.Init.SuccessTime", 0);
  histogram_tester.ExpectTotalCount("Net.SqlDiskCache.Init.FailureTime", 1);
}

TEST_F(SqlBackendImplTest, InitWithCorruptedFakeIndexFile) {
  std::string corrupted_contents = GetExpectedFakeIndexContents();
  base::span<uint8_t> corrupted_contents_span =
      base::as_writable_bytes(base::span(corrupted_contents));
  // Rewrite the last char to 'X'.
  corrupted_contents_span.subspan(corrupted_contents_span.size() - 1)
      .copy_from({'X'});
  base::HistogramTester histogram_tester;
  base::FilePath file_path =
      temp_dir_.GetPath().Append(kSqlBackendFakeIndexFileName);
  ASSERT_TRUE(base::WriteFile(file_path, corrupted_contents_span));

  auto backend = CreateBackend();
  base::test::TestFuture<int> future;
  backend->Init(future.GetCallback());
  ASSERT_EQ(future.Get(), net::ERR_FAILED);
  histogram_tester.ExpectUniqueSample("Net.SqlDiskCache.FakeIndexFileError",
                                      FakeIndexFileError::kWrongMagicNumber, 1);
}

TEST_F(SqlBackendImplTest, InitWithWrongSizeFakeIndexFile) {
  base::HistogramTester histogram_tester;
  base::FilePath file_path =
      temp_dir_.GetPath().Append(kSqlBackendFakeIndexFileName);
  const int32_t kWrongMagicNumber = 0xDEADBEEF;
  ASSERT_TRUE(
      base::WriteFile(file_path, base::byte_span_from_ref(kWrongMagicNumber)));

  auto backend = CreateBackend();
  base::test::TestFuture<int> future;
  backend->Init(future.GetCallback());
  ASSERT_EQ(future.Get(), net::ERR_FAILED);
  histogram_tester.ExpectUniqueSample("Net.SqlDiskCache.FakeIndexFileError",
                                      FakeIndexFileError::kWrongFileSize, 1);
}

TEST_F(SqlBackendImplTest, InitWithOpenFileFailed) {
  const std::string expected_contents = GetExpectedFakeIndexContents();
  base::HistogramTester histogram_tester;
  base::FilePath file_path =
      temp_dir_.GetPath().Append(kSqlBackendFakeIndexFileName);
  ASSERT_TRUE(
      base::WriteFile(file_path, base::as_byte_span(expected_contents)));
  base::FilePermissionRestorer permission_restorer(file_path);
  // Make the file unreadable.
  ASSERT_TRUE(base::MakeFileUnreadable(file_path));

  auto backend = CreateBackend();
  base::test::TestFuture<int> future;
  backend->Init(future.GetCallback());
  ASSERT_EQ(future.Get(), net::ERR_FAILED);
  histogram_tester.ExpectUniqueSample("Net.SqlDiskCache.FakeIndexFileError",
                                      FakeIndexFileError::kOpenFileFailed, 1);
}

TEST_F(SqlBackendImplTest, InitWithCreateFileFailed) {
  base::HistogramTester histogram_tester;
  base::FilePermissionRestorer permission_restorer(temp_dir_.GetPath());
  // Make the directory unwrittable.
  ASSERT_TRUE(base::MakeFileUnwritable(temp_dir_.GetPath()));

  auto backend = CreateBackend();
  base::test::TestFuture<int> future;
  backend->Init(future.GetCallback());
  ASSERT_EQ(future.Get(), net::ERR_FAILED);
  histogram_tester.ExpectUniqueSample("Net.SqlDiskCache.FakeIndexFileError",
                                      FakeIndexFileError::kCreateFileFailed, 1);
}

TEST_F(SqlBackendImplTest, InitWithFailedToCreateDirectory) {
  base::HistogramTester histogram_tester;
  base::FilePath cache_dir =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("cache"));
  // Create a file where the cache directory is supposed to be, to simulate a
  // directory creation failure.
  ASSERT_TRUE(base::WriteFile(cache_dir, ""));

  auto backend = std::make_unique<SqlBackendImpl>(cache_dir, kDefaultMaxBytes,
                                                  net::CacheType::DISK_CACHE,
                                                  /*cleanup_tracker=*/nullptr);
  base::test::TestFuture<int> future;
  backend->Init(future.GetCallback());
  ASSERT_EQ(future.Get(), net::ERR_FAILED);
  histogram_tester.ExpectUniqueSample(
      "Net.SqlDiskCache.FakeIndexFileError",
      FakeIndexFileError::kFailedToCreateDirectory, 1);
}

TEST_F(SqlBackendImplTest, MaxFileSizeSmallMax) {
  const int64_t kMaxBytes = 10 * 1024 * 1024;
  auto backend = CreateBackendAndInit(kMaxBytes);
  EXPECT_EQ(backend->MaxFileSize(), kSqlBackendMinFileSizeLimit);
}

TEST_F(SqlBackendImplTest, MaxFileSizeCalculation) {
  const int64_t kLargeMaxBytes = 100 * 1024 * 1024;
  auto backend = CreateBackendAndInit(kLargeMaxBytes);
  EXPECT_EQ(backend->MaxFileSize(),
            kLargeMaxBytes / kSqlBackendMaxFileRatioDenominator);
}

TEST_F(SqlBackendImplTest, GetStats) {
  auto backend = CreateBackendAndInit();
  base::StringPairs stats;
  backend->GetStats(&stats);
  EXPECT_THAT(stats, ElementsAre(Pair("Cache type", "SQL Cache")));
}

// Tests a race condition where an entry is doomed via `SqlEntryImpl::Doom()`
// while an iterator is in the process of opening it. The iterator should still
// successfully open the entry, but the entry should be marked as doomed. This
// works because `OpenNextEntry` is an exclusive operation that runs before the
// normal `Doom` operation.
TEST_F(SqlBackendImplTest, IteratorParallelEntryDoom) {
  auto backend = CreateBackendAndInit();
  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result = cb_create.GetResult(
      backend->CreateEntry("key", net::HIGHEST, cb_create.callback()));
  auto* entry1 = create_result.ReleaseEntry();

  auto iter = backend->CreateIterator();
  TestEntryResultCompletionCallback cb;
  EntryResult result_iter = iter->OpenNextEntry(cb.callback());

  entry1->Doom();

  result_iter = cb.GetResult(std::move(result_iter));
  ASSERT_THAT(result_iter.net_error(), IsOk());
  auto* entry2 = result_iter.ReleaseEntry();

  EXPECT_EQ(entry1, entry2);
  EXPECT_TRUE((static_cast<SqlEntryImpl*>(entry1))->doomed());

  entry1->Close();
  entry2->Close();
}

// Tests a race condition where an entry is doomed and closed while an iterator
// is opening it. The iterator should still get a handle to the doomed entry.
// This verifies that the backend correctly manages the lifecycle of an entry
// that is being operated on by multiple asynchronous calls.
TEST_F(SqlBackendImplTest, IteratorParallelEntryDoomAndClose) {
  auto backend = CreateBackendAndInit();
  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result = cb_create.GetResult(
      backend->CreateEntry("key", net::HIGHEST, cb_create.callback()));
  auto* entry = create_result.ReleaseEntry();

  auto iter = backend->CreateIterator();
  TestEntryResultCompletionCallback cb;
  EntryResult result_iter = iter->OpenNextEntry(cb.callback());

  entry->Doom();
  // Doom() is asynchronous. The entry is not marked as doomed until the
  // callback of OpenNextEntry is called.
  EXPECT_FALSE((static_cast<SqlEntryImpl*>(entry))->doomed());
  entry->Close();

  result_iter = cb.GetResult(std::move(result_iter));
  ASSERT_THAT(result_iter.net_error(), IsOk());

  entry = result_iter.ReleaseEntry();

  EXPECT_TRUE((static_cast<SqlEntryImpl*>(entry))->doomed());
  entry->Close();
}

// Tests that the iterator correctly skips over an entry that is doomed before
// the `OpenNextEntry` operation is executed.
TEST_F(SqlBackendImplTest, IteratorParallelEntryDoomOpenNext) {
  auto backend = CreateBackendAndInit();

  // Create the first entry.
  TestEntryResultCompletionCallback cb_create1;
  disk_cache::EntryResult create_result1 = cb_create1.GetResult(
      backend->CreateEntry("key1", net::HIGHEST, cb_create1.callback()));
  ASSERT_THAT(create_result1.net_error(), IsOk());
  create_result1.ReleaseEntry()->Close();

  // Create the second entry.
  TestEntryResultCompletionCallback cb_create2;
  disk_cache::EntryResult create_result2 = cb_create2.GetResult(
      backend->CreateEntry("key2", net::HIGHEST, cb_create2.callback()));
  ASSERT_THAT(create_result2.net_error(), IsOk());
  auto* entry = create_result2.ReleaseEntry();

  auto iter = backend->CreateIterator();
  TestEntryResultCompletionCallback cb_iter;

  entry->Doom();
  entry->Close();

  // `entry->Doom()` is called before `OpenNextEntry()` is initiated.
  // The iterator starts from the newest entry, which is `key2`. However, `key2`
  // is doomed before the iterator's `OpenNextEntry` operation is posted. The
  // iterator should detect that `key2` is doomed in the database and skip it,
  // returning `key1` instead.
  EntryResult result =
      cb_iter.GetResult(iter->OpenNextEntry(cb_iter.callback()));
  ASSERT_THAT(result.net_error(), IsOk());
  entry = result.ReleaseEntry();
  EXPECT_EQ(entry->GetKey(), "key1");
  entry->Close();

  // There should be no more entries.
  EntryResult result2 =
      cb_iter.GetResult(iter->OpenNextEntry(cb_iter.callback()));
  ASSERT_THAT(result2.net_error(), IsError(net::ERR_FAILED));
}

// Tests a race condition between an iterator opening an entry and a direct call
// to `Backend::DoomEntry`.
TEST_F(SqlBackendImplTest, IteratorParallelDoom) {
  auto backend = CreateBackendAndInit();

  // 1. Create an entry and write some data to it.
  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result = cb_create.GetResult(
      backend->CreateEntry("key", net::HIGHEST, cb_create.callback()));
  auto* entry1 = create_result.ReleaseEntry();
  const std::string kData = "some data";
  auto buffer = base::MakeRefCounted<net::StringIOBuffer>(kData);
  net::TestCompletionCallback cb_write;
  int rv_write = entry1->WriteData(1, 0, buffer.get(), buffer->size(),
                                   cb_write.callback(), false);
  EXPECT_EQ(cb_write.GetResult(rv_write), static_cast<int>(buffer->size()));

  entry1->Close();

  // 2. Start opening the entry via an iterator. This is an async operation.
  auto iter = backend->CreateIterator();
  TestEntryResultCompletionCallback cb;
  EntryResult result_iter = iter->OpenNextEntry(cb.callback());

  // 3. Immediately call `DoomEntry` for the same key. This is also async.
  net::TestCompletionCallback cb_doom;
  int rv_doom = backend->DoomEntry("key", net::HIGHEST, cb_doom.callback());
  EXPECT_EQ(net::OK, cb_doom.GetResult(rv_doom));

  // `OpenNextEntry()` is an exclusive operation, while `DoomEntry()` is a
  // normal operation. Since `OpenNextEntry()` is posted first, it will run
  // before the `DoomEntry()` operation, which gets queued. After the iterator
  // returns the entry, the `DoomEntry()` operation runs and marks the entry as
  // doomed.
  // 4. Wait for the iterator to finish opening the entry.
  result_iter = cb.GetResult(std::move(result_iter));
  ASSERT_THAT(result_iter.net_error(), IsOk());
  auto* entry = result_iter.ReleaseEntry();
  EXPECT_TRUE(static_cast<SqlEntryImpl*>(entry)->doomed());

  // 5. Verify that the data can still be read from the doomed entry.
  auto read_buffer = base::MakeRefCounted<net::IOBufferWithSize>(kData.size());
  net::TestCompletionCallback cb_read;
  int rv_read = entry->ReadData(1, 0, read_buffer.get(), read_buffer->size(),
                                cb_read.callback());
  EXPECT_EQ(cb_read.GetResult(rv_read), static_cast<int>(kData.size()));
  EXPECT_EQ(std::string_view(read_buffer->data(), kData.size()), kData);
  entry->Close();
}

// Tests a race condition between an iterator opening an entry and a call to
// `Backend::DoomAllEntries`.
TEST_F(SqlBackendImplTest, IteratorParallelDoomAll) {
  auto backend = CreateBackendAndInit();

  // 1. Create an entry and write some data to it.
  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result = cb_create.GetResult(
      backend->CreateEntry("key", net::HIGHEST, cb_create.callback()));
  auto* entry1 = create_result.ReleaseEntry();
  const std::string kData = "some data";
  auto buffer = base::MakeRefCounted<net::StringIOBuffer>(kData);
  net::TestCompletionCallback cb_write;
  int rv_write = entry1->WriteData(1, 0, buffer.get(), buffer->size(),
                                   cb_write.callback(), false);
  EXPECT_EQ(cb_write.GetResult(rv_write), static_cast<int>(buffer->size()));
  entry1->Close();

  // 2. Start opening the entry via an iterator. This is an async operation.
  auto iter = backend->CreateIterator();
  TestEntryResultCompletionCallback cb;
  EntryResult result_iter = iter->OpenNextEntry(cb.callback());

  // 3. Immediately call `DoomAllEntries`. This is also an async operation.
  net::TestCompletionCallback cb_doom;
  int rv_doom = backend->DoomAllEntries(cb_doom.callback());
  EXPECT_EQ(net::OK, cb_doom.GetResult(rv_doom));

  // Both `DoomAllEntries()` and `OpenNextEntry()` are exclusive operations and
  // are serialized. Since `OpenNextEntry()` is posted first, it will run
  // first, retrieving the entry. Then, `DoomAllEntries()` will run and doom all
  // entries, including the one just opened.
  // 4. Wait for the iterator to finish opening the entry.
  result_iter = cb.GetResult(std::move(result_iter));
  ASSERT_THAT(result_iter.net_error(), IsOk());
  auto* entry = result_iter.ReleaseEntry();
  EXPECT_TRUE(static_cast<SqlEntryImpl*>(entry)->doomed());

  // 5. Verify that the data can still be read from the doomed entry.
  auto read_buffer = base::MakeRefCounted<net::IOBufferWithSize>(kData.size());
  net::TestCompletionCallback cb_read;
  int rv_read = entry->ReadData(1, 0, read_buffer.get(), read_buffer->size(),
                                cb_read.callback());
  EXPECT_EQ(cb_read.GetResult(rv_read), static_cast<int>(kData.size()));
  EXPECT_EQ(std::string_view(read_buffer->data(), kData.size()), kData);
  entry->Close();
}

// Tests that an entry's `last_used` time is updated correctly when data is
// written and the entry is closed, even if an iterator is concurrently active.
// Also verifies the written data can be read back.
TEST_F(SqlBackendImplTest, IteratorParallelWriteDataAndClose) {
  auto backend = CreateBackendAndInit();
  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result = cb_create.GetResult(
      backend->CreateEntry("key", net::HIGHEST, cb_create.callback()));
  auto* entry = create_result.ReleaseEntry();

  // Advance clock to ensure `last_used` time is distinct from creation.
  AdvanceClock(base::Minutes(1));

  // Create an iterator and attempt to open the entry concurrently.
  auto iter = backend->CreateIterator();
  TestEntryResultCompletionCallback cb;
  EntryResult result_iter = iter->OpenNextEntry(cb.callback());

  // Record the time when data is written. This should be the new `last_used`
  // time.
  const base::Time kWriteTime = base::Time::Now();

  // Write data to stream 0 and close the entry.
  const std::string kHeadData = "header_data";
  auto buffer = base::MakeRefCounted<net::StringIOBuffer>(kHeadData);
  net::TestCompletionCallback cb_write;
  int rv_write = entry->WriteData(0, 0, buffer.get(), buffer->size(),
                                  cb_write.callback(), false);
  entry->Close();
  EXPECT_EQ(cb_write.GetResult(rv_write), buffer->size());

  // Get the result from the iterator's open operation.
  result_iter = cb.GetResult(std::move(result_iter));
  ASSERT_THAT(result_iter.net_error(), IsOk());
  entry = result_iter.ReleaseEntry();
  // Verify that the `last_used` time of the opened entry reflects the write
  // time.
  EXPECT_THAT(entry->GetLastUsed(), kWriteTime);

  // Read the data back from the entry opened via the iterator.
  auto read_buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(kHeadData.size() * 2);
  net::TestCompletionCallback cb_read;
  int rv_read = entry->ReadData(0, 0, read_buffer.get(), read_buffer->size(),
                                cb_read.callback());
  EXPECT_EQ(cb_read.GetResult(rv_read), kHeadData.size());
  entry->Close();
}

// Tests that an entry's `body_end` is updated correctly when data is written to
// stream 1 and the entry is closed, even if an iterator is concurrently active.
// Also verifies the written data can be read back.
TEST_F(SqlBackendImplTest, IteratorParallelWriteBodyDataAndClose) {
  auto backend = CreateBackendAndInit();
  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result = cb_create.GetResult(
      backend->CreateEntry("key", net::HIGHEST, cb_create.callback()));
  auto* entry = create_result.ReleaseEntry();

  // Create an iterator and attempt to open the entry concurrently.
  auto iter = backend->CreateIterator();
  TestEntryResultCompletionCallback cb;
  EntryResult result_iter = iter->OpenNextEntry(cb.callback());

  // Write data to stream 1 and close the entry.
  const std::string kBodyData = "body_data";
  auto buffer = base::MakeRefCounted<net::StringIOBuffer>(kBodyData);
  net::TestCompletionCallback cb_write;
  int rv_write = entry->WriteData(1, 0, buffer.get(), buffer->size(),
                                  cb_write.callback(), false);
  entry->Close();
  EXPECT_EQ(cb_write.GetResult(rv_write), static_cast<int>(buffer->size()));

  // Get the result from the iterator's open operation.
  result_iter = cb.GetResult(std::move(result_iter));
  ASSERT_THAT(result_iter.net_error(), IsOk());
  entry = result_iter.ReleaseEntry();
  // Verify that the `body_end` of the opened entry reflects the write.
  EXPECT_EQ(entry->GetDataSize(1), static_cast<int32_t>(kBodyData.size()));

  // Read the data back from the entry opened via the iterator.
  auto read_buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(kBodyData.size() * 2);
  net::TestCompletionCallback cb_read;
  int rv_read = entry->ReadData(1, 0, read_buffer.get(), read_buffer->size(),
                                cb_read.callback());
  EXPECT_EQ(cb_read.GetResult(rv_read), static_cast<int>(kBodyData.size()));
  EXPECT_EQ(std::string_view(read_buffer->data(), kBodyData.size()), kBodyData);
  entry->Close();
}

// Tests that an entry's `last_used` time is updated correctly when data is read
// and the entry is closed, even if an iterator is concurrently active.
TEST_F(SqlBackendImplTest, IteratorParallelReadDataAndClose) {
  auto backend = CreateBackendAndInit();
  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result = cb_create.GetResult(
      backend->CreateEntry("key", net::HIGHEST, cb_create.callback()));
  auto* entry = create_result.ReleaseEntry();

  // Advance clock to ensure `last_used` time is distinct from creation.
  AdvanceClock(base::Minutes(1));

  // Create an iterator and attempt to open the entry concurrently.
  auto iter = backend->CreateIterator();
  TestEntryResultCompletionCallback cb;
  EntryResult result_iter = iter->OpenNextEntry(cb.callback());

  // Record the time when data is read. This should be the new `last_used` time.
  const base::Time kReadTime = base::Time::Now();

  // Read data from stream 0 and close the entry.
  auto read_buffer = base::MakeRefCounted<net::IOBufferWithSize>(1);
  net::TestCompletionCallback cb_read;
  int rv_read = entry->ReadData(0, 0, read_buffer.get(), read_buffer->size(),
                                cb_read.callback());
  EXPECT_EQ(cb_read.GetResult(rv_read), 0);
  entry->Close();

  // Get the result from the iterator's open operation.
  result_iter = cb.GetResult(std::move(result_iter));
  ASSERT_THAT(result_iter.net_error(), IsOk());
  entry = result_iter.ReleaseEntry();

  // Verify that the `last_used` time of the opened entry reflects the read
  // time.
  EXPECT_THAT(entry->GetLastUsed(), kReadTime);
  entry->Close();
}

// Tests a race condition where an entry is opened simultaneously by an iterator
// and a direct `OpenEntry` call. The backend should correctly handle this by
// returning the same `SqlEntryImpl` instance for both operations, preventing
// duplicate in-memory representations of the same cache entry.
TEST_F(SqlBackendImplTest, IteratorAndOpenEntryParallelRace) {
  auto backend = CreateBackendAndInit();

  // Create an entry.
  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result = cb_create.GetResult(
      backend->CreateEntry("key", net::HIGHEST, cb_create.callback()));
  ASSERT_THAT(create_result.net_error(), IsOk());
  create_result.ReleaseEntry()->Close();

  base::test::TestFuture<EntryResult> future_iter;
  base::test::TestFuture<EntryResult> future_open;

  auto iter = backend->CreateIterator();
  // Start opening the entry via the iterator. This posts an async task.
  ASSERT_EQ(iter->OpenNextEntry(future_iter.GetCallback()).net_error(),
            net::ERR_IO_PENDING);

  // Immediately try to open the same entry directly. This also posts an async
  // task.
  ASSERT_EQ(backend->OpenEntry("key", net::HIGHEST, future_open.GetCallback())
                .net_error(),
            net::ERR_IO_PENDING);

  // 3. Wait for both operations to complete. This test is designed to expose a
  // race condition. The backend should handle this race correctly by ensuring
  // only one `SqlEntryImpl` is created for the same key.
  EntryResult iter_res = future_iter.Take();
  EntryResult open_res = future_open.Take();

  ASSERT_THAT(iter_res.net_error(), IsOk());
  ASSERT_THAT(open_res.net_error(), IsOk());
  auto* entry1 = iter_res.ReleaseEntry();
  auto* entry2 = open_res.ReleaseEntry();

  // Both the iterator and the direct open operation should resolve to the same
  // underlying `SqlEntryImpl` instance. The backend's logic for managing
  // active entries should prevent the creation of a second instance for the
  // same key.
  EXPECT_EQ(entry1, entry2);
  entry1->Close();
  entry2->Close();
}

// Tests a race condition where an entry is opened by an iterator, opened by a
// direct call, and doomed, all in parallel.
// The exclusive `OpenNextEntry` operation runs first. The normal `OpenEntry`
// and `DoomEntry` operations are queued and serialized by key. `OpenEntry`
// runs next, getting a reference to the active entry. Finally, `DoomEntry`
// runs and marks that same entry instance as doomed.
TEST_F(SqlBackendImplTest, IteratorAndOpenEntryAndDoomParallelRace) {
  auto backend = CreateBackendAndInit();

  // Create an entry.
  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result = cb_create.GetResult(
      backend->CreateEntry("key", net::HIGHEST, cb_create.callback()));
  ASSERT_THAT(create_result.net_error(), IsOk());
  create_result.ReleaseEntry()->Close();

  base::test::TestFuture<EntryResult> future_iter;
  base::test::TestFuture<EntryResult> future_open;
  base::test::TestFuture<int> future_doom;

  auto iter = backend->CreateIterator();
  ASSERT_EQ(iter->OpenNextEntry(future_iter.GetCallback()).net_error(),
            net::ERR_IO_PENDING);
  ASSERT_EQ(backend->OpenEntry("key", net::HIGHEST, future_open.GetCallback())
                .net_error(),
            net::ERR_IO_PENDING);
  ASSERT_EQ(backend->DoomEntry("key", net::HIGHEST, future_doom.GetCallback()),
            net::ERR_IO_PENDING);

  EntryResult iter_res = future_iter.Take();
  ASSERT_THAT(iter_res.net_error(), IsOk());
  auto* entry1 = iter_res.ReleaseEntry();

  EntryResult open_res = future_open.Take();
  ASSERT_THAT(open_res.net_error(), IsOk());
  auto* entry2 = open_res.ReleaseEntry();

  EXPECT_EQ(entry1, entry2);
  EXPECT_EQ(future_doom.Take(), net::OK);

  EXPECT_TRUE(static_cast<SqlEntryImpl*>(entry1)->doomed());

  entry1->Close();
  entry2->Close();
}

// Tests a race condition where an entry is opened via `OpenEntry` while it is
// simultaneously being opened and then doomed by an iterator.
TEST_F(SqlBackendImplTest, OpenEntryRacesWithIteratorAndDoom) {
  // This test simulates a race condition to verify the interaction between
  // opening an entry directly and an iterator that dooms the same entry in its
  // callback. The exclusive `OpenNextEntry` operation runs first. Its callback
  // then posts a normal `Doom` operation. The `OpenEntry` call (also a normal
  // operation) was posted before the `Doom` operation. Due to serialization by
  // key, `OpenEntry` gets a reference to the active entry first, and then the
  // `Doom` operation marks that same instance as doomed.
  auto backend = CreateBackendAndInit();

  // 1. Create an entry.
  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result = cb_create.GetResult(
      backend->CreateEntry("key", net::HIGHEST, cb_create.callback()));
  ASSERT_THAT(create_result.net_error(), IsOk());
  create_result.ReleaseEntry()->Close();

  auto iter = backend->CreateIterator();
  // 2. Start opening the entry via an iterator. This is an async operation.
  ASSERT_EQ(iter->OpenNextEntry(
                    base::BindLambdaForTesting([&](EntryResult entry_result) {
                      // 4. Once the iterator gets the entry, doom and close it.
                      auto* entry = entry_result.ReleaseEntry();
                      entry->Doom();
                      entry->Close();
                    }))
                .net_error(),
            net::ERR_IO_PENDING);

  // 3. While the iterator operation is in-flight, attempt to open the same
  //    entry directly. This is also an async operation.
  base::test::TestFuture<EntryResult> open_future;
  ASSERT_EQ(backend->OpenEntry("key", net::HIGHEST, open_future.GetCallback())
                .net_error(),
            net::ERR_IO_PENDING);

  // 5. Wait for the direct `OpenEntry` call to complete.
  EntryResult open_result = open_future.Take();
  ASSERT_THAT(open_result.net_error(), IsOk());
  auto* entry = open_result.ReleaseEntry();
  EXPECT_TRUE(static_cast<SqlEntryImpl*>(entry)->doomed());
  entry->Close();
}

// Tests a race condition where an entry is opened via `OpenOrCreateEntry` while
// it is simultaneously being opened and then doomed by an iterator.
// `OpenOrCreateEntry` should find the existing entry (which is being opened by
// the iterator) and not create a new one. The test verifies that the returned
// entry is correctly marked as doomed, demonstrating proper serialization and
// state management.
TEST_F(SqlBackendImplTest, OpenOrCreateEntryEntryRacesWithIteratorAndDoom) {
  auto backend = CreateBackendAndInit();

  // 1. Create an entry and record its creation time.
  base::Time first_entry_creation_time = base::Time::Now();
  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result = cb_create.GetResult(
      backend->CreateEntry("key", net::HIGHEST, cb_create.callback()));
  ASSERT_THAT(create_result.net_error(), IsOk());
  auto* entry = create_result.ReleaseEntry();
  EXPECT_EQ(entry->GetLastUsed(), first_entry_creation_time);
  entry->Close();

  AdvanceClock(base::Minutes(1));

  auto iter = backend->CreateIterator();
  // 2. Start opening the entry via an iterator. This is an async operation.
  ASSERT_EQ(iter->OpenNextEntry(
                    base::BindLambdaForTesting([&](EntryResult entry_result) {
                      // 4. Once the iterator gets the entry, doom and close it.
                      auto* entry = entry_result.ReleaseEntry();
                      EXPECT_EQ(entry->GetLastUsed(),
                                first_entry_creation_time);
                      entry->Doom();
                      entry->Close();
                    }))
                .net_error(),
            net::ERR_IO_PENDING);

  base::test::TestFuture<EntryResult> open_or_create_future;
  // 3. While the iterator operation is in-flight, call `OpenOrCreateEntry` for
  //    the same key.
  ASSERT_EQ(backend
                ->OpenOrCreateEntry("key", net::HIGHEST,
                                    open_or_create_future.GetCallback())
                .net_error(),
            net::ERR_IO_PENDING);

  EntryResult open_or_create_result = open_or_create_future.Take();
  ASSERT_THAT(open_or_create_result.net_error(), IsOk());
  entry = open_or_create_result.ReleaseEntry();
  EXPECT_EQ(entry->GetLastUsed(), first_entry_creation_time);
  EXPECT_TRUE(static_cast<SqlEntryImpl*>(entry)->doomed());
  entry->Close();
}

// Tests a race condition where an entry is opened via `OpenEntry` while it is
// simultaneously being opened, written to, and closed by an iterator.
// This test verifies that in-flight modifications (like `last_used`
// time and header data updates) that are queued while an entry is not active
// are correctly applied to the entry's in-memory representation when it is
// next opened. This ensures that subsequent operations on the entry see the
// most up-to-date state.
TEST_F(SqlBackendImplTest, OpenEntryRacesWithIteratorAndWriteData) {
  auto backend = CreateBackendAndInit();

  // 1. Create an entry and record its creation time.
  base::Time first_entry_creation_time = base::Time::Now();
  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result = cb_create.GetResult(
      backend->CreateEntry("key", net::HIGHEST, cb_create.callback()));
  ASSERT_THAT(create_result.net_error(), IsOk());
  auto* entry = create_result.ReleaseEntry();
  EXPECT_EQ(entry->GetLastUsed(), first_entry_creation_time);
  entry->Close();

  AdvanceClock(base::Minutes(1));

  const std::string kHeadData = "header_data";
  // 2. Start opening the entry via an iterator. This is an async operation.
  auto iter = backend->CreateIterator();
  ASSERT_EQ(iter->OpenNextEntry(
                    base::BindLambdaForTesting([&](EntryResult entry_result) {
                      // 4. Once the iterator gets the entry, write data to it
                      //    and close it. This updates the entry's `last_used`
                      //    time and header data in memory, and queues a write
                      //    to the persistent store.
                      auto* entry = entry_result.ReleaseEntry();
                      EXPECT_EQ(entry->GetLastUsed(),
                                first_entry_creation_time);
                      auto buffer =
                          base::MakeRefCounted<net::StringIOBuffer>(kHeadData);
                      entry->WriteData(0, 0, buffer.get(), buffer->size(),
                                       base::DoNothing(), false);
                      entry->Close();
                    }))
                .net_error(),
            net::ERR_IO_PENDING);

  // 3. While the iterator operation is in-flight, attempt to open the same
  //    entry directly.
  base::test::TestFuture<EntryResult> open_future;
  ASSERT_EQ(backend->OpenEntry("key", net::HIGHEST, open_future.GetCallback())
                .net_error(),
            net::ERR_IO_PENDING);

  // 5. The `OpenEntry` operation should succeed. The backend should handle the
  //    race by applying the in-flight modifications (from the iterator's
  //    write and close) to the entry data before returning the new entry
  //    handle.
  EntryResult open_result = open_future.Take();
  ASSERT_THAT(open_result.net_error(), IsOk());
  entry = open_result.ReleaseEntry();
  // The `last_used` time should reflect the time of the write.
  EXPECT_EQ(entry->GetLastUsed(), first_entry_creation_time + base::Minutes(1));
  // The data written by the iterator should be readable.
  auto buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(kHeadData.size() * 2);
  ASSERT_EQ(
      entry->ReadData(0, 0, buffer.get(), buffer->size(), base::DoNothing()),
      kHeadData.size());
  EXPECT_EQ(buffer->first(kHeadData.size()), base::as_byte_span(kHeadData));
  entry->Close();
}

// Tests that OnExternalCacheHit correctly updates the last_used time, even when
// an OpenEntry operation is in-flight.
TEST_F(SqlBackendImplTest, OnExternalCacheHitRacesWithOpen) {
  auto backend = CreateBackendAndInit();

  // 1. Create an entry and close it.
  const std::string kKey = "my-key";
  TestEntryResultCompletionCallback create_cb;
  disk_cache::EntryResult create_result = create_cb.GetResult(
      backend->CreateEntry(kKey, net::HIGHEST, create_cb.callback()));
  ASSERT_THAT(create_result.net_error(), IsOk());
  auto* created_entry = create_result.ReleaseEntry();
  base::Time create_time = created_entry->GetLastUsed();
  created_entry->Close();

  // 2. Advance time.
  AdvanceClock(base::Minutes(1));

  // 3. Start opening the entry. This is an async operation.
  base::test::TestFuture<EntryResult> open_future;
  ASSERT_EQ(backend->OpenEntry(kKey, net::HIGHEST, open_future.GetCallback())
                .net_error(),
            net::ERR_IO_PENDING);

  // 4. Call OnExternalCacheHit.
  base::Time hit_time = base::Time::Now();
  EXPECT_NE(create_time, hit_time);
  backend->OnExternalCacheHit(kKey);

  // 5. Wait for OpenEntry to complete.
  EntryResult open_result = open_future.Take();
  ASSERT_THAT(open_result.net_error(), IsOk());
  auto* entry = open_result.ReleaseEntry();

  // 6. The entry's last_used time should be the time of the external hit.
  EXPECT_EQ(entry->GetLastUsed(), hit_time);
  entry->Close();
}

TEST_F(SqlBackendImplTest, DoomEntryNonExistent) {
  auto backend = CreateBackendAndInit();
  EXPECT_TRUE(LoadInMemoryIndex(*backend));
  backend->GetSqlStoreForTest()->SetSimulateDbFailureForTesting(true);
  const std::string kNonExistentKey = "non-existent-key";
  net::TestCompletionCallback cb_doom;
  int rv_doom =
      backend->DoomEntry(kNonExistentKey, net::HIGHEST,
                         base::BindOnce([](int rv) { NOTREACHED(); }));
  // The operation should complete synchronously due to the in-memory index
  // check, so the callback should not be reached if the DB operation were to
  // be attempted.
  EXPECT_EQ(net::OK, rv_doom);
}

// Tests that calling Doom() multiple times on the same entry is safe and
// idempotent.
TEST_F(SqlBackendImplTest, MultipleDoomsOnSameEntry) {
  auto backend = CreateBackendAndInit();

  // Create an entry.
  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result = cb_create.GetResult(
      backend->CreateEntry("key", net::HIGHEST, cb_create.callback()));
  ASSERT_THAT(create_result.net_error(), IsOk());
  auto* entry = create_result.ReleaseEntry();

  // Doom the entry multiple times. This should not cause any issues.
  entry->Doom();
  entry->Doom();

  // When the entry was created speculatively, the doomed flag is updated
  // asynchronously. So need to flush the pending database operations.
  backend->RunUntilAllTasksCompleteForTest();

  EXPECT_TRUE(static_cast<SqlEntryImpl*>(entry)->doomed());
  entry->Close();

  // Verify that the entry is gone after being doomed and closed.
  TestEntryResultCompletionCallback cb_open;
  disk_cache::EntryResult open_result = cb_open.GetResult(
      backend->OpenEntry("key", net::HIGHEST, cb_open.callback()));
  EXPECT_THAT(open_result.net_error(), IsError(net::ERR_FAILED));
}

// Tests that recursive calls to OpenNextEntry from within its callback do not
// starve normal operations. The ExclusiveOperationCoordinator's sequence-based
// scheduling ensures that the older normal operation (CreateEntry) is executed
// before the newer exclusive operation (the second OpenNextEntry).
TEST_F(SqlBackendImplTest, RecursiveOpenNextEntry) {
  auto backend = CreateBackendAndInit();

  // Create two entries to iterate over.
  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result1 = cb_create.GetResult(
      backend->CreateEntry("key1", net::HIGHEST, cb_create.callback()));
  ASSERT_THAT(create_result1.net_error(), IsOk());
  create_result1.ReleaseEntry()->Close();

  disk_cache::EntryResult create_result2 = cb_create.GetResult(
      backend->CreateEntry("key2", net::HIGHEST, cb_create.callback()));
  ASSERT_THAT(create_result2.net_error(), IsOk());
  create_result2.ReleaseEntry()->Close();

  auto iter = backend->CreateIterator();

  base::RunLoop run_loop;
  Entry* entry3 = nullptr;

  bool key_1_found = false;
  bool key_2_found = false;

  // The first call to OpenNextEntry. Inside its callback, we'll trigger the
  // second call.
  ASSERT_THAT(
      iter->OpenNextEntry(base::BindLambdaForTesting([&](EntryResult result1) {
            ASSERT_THAT(result1.net_error(), IsOk());
            auto* entry1_itr = result1.ReleaseEntry();
            key_1_found = entry1_itr->GetKey() == "key1";
            key_2_found = entry1_itr->GetKey() == "key2";
            EXPECT_TRUE(key_1_found || key_2_found);
            entry1_itr->Close();
            // Now, make the recursive call to OpenNextEntry.
            ASSERT_THAT(
                iter->OpenNextEntry(
                        base::BindLambdaForTesting([&](EntryResult result2) {
                          ASSERT_THAT(result2.net_error(), IsOk());
                          // By this point, the CreateEntry for "key3" should
                          // have completed, proving that the normal operation
                          // was not starved.
                          CHECK(entry3);

                          auto* entry2_itr = result2.ReleaseEntry();
                          if (entry2_itr->GetKey() == "key3") {
                            EXPECT_EQ(entry2_itr, entry3);
                          } else {
                            if (key_1_found) {
                              EXPECT_EQ(entry2_itr->GetKey(), "key2");
                            } else {
                              EXPECT_EQ(entry2_itr->GetKey(), "key1");
                            }
                          }
                          entry2_itr->Close();
                          entry3->Close();
                          run_loop.Quit();
                        }))
                    .net_error(),
                IsError(net::ERR_IO_PENDING));
          }))
          .net_error(),
      IsError(net::ERR_IO_PENDING));

  // While the first OpenNextEntry is in flight, post a normal operation to
  // create a third entry. This tests that the recursive exclusive operations
  // do not starve the normal one.
  ASSERT_THAT(
      backend
          ->CreateEntry("key3", net::HIGHEST,
                        base::BindLambdaForTesting([&](EntryResult result3) {
                          ASSERT_THAT(result3.net_error(), IsOk());
                          entry3 = result3.ReleaseEntry();
                        }))
          .net_error(),
      IsError(net::ERR_IO_PENDING));
  run_loop.Run();
}

// Tests that recursive calls to OpenNextEntry from within its callback do not
// starve normal operations, even when one of the iterated entries is already
// active.
TEST_F(SqlBackendImplTest, RecursiveOpenNextEntryWithActiveEntry) {
  auto backend = CreateBackendAndInit();

  // Create one entry and close it.
  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result1 = cb_create.GetResult(
      backend->CreateEntry("key1", net::HIGHEST, cb_create.callback()));
  ASSERT_THAT(create_result1.net_error(), IsOk());
  create_result1.ReleaseEntry()->Close();

  // Create a second entry and keep it active.
  disk_cache::EntryResult create_result2 = cb_create.GetResult(
      backend->CreateEntry("key2", net::HIGHEST, cb_create.callback()));
  ASSERT_THAT(create_result2.net_error(), IsOk());
  auto* entry2_active = create_result2.ReleaseEntry();

  auto iter = backend->CreateIterator();

  base::RunLoop run_loop;
  Entry* entry3 = nullptr;

  bool key_1_found = false;
  bool key_2_found = false;

  // The first call to OpenNextEntry. Inside its callback, we'll trigger the
  // second call.
  ASSERT_THAT(
      iter->OpenNextEntry(base::BindLambdaForTesting([&](EntryResult result1) {
            ASSERT_THAT(result1.net_error(), IsOk());
            auto* entry1_iter = result1.ReleaseEntry();
            key_1_found = entry1_iter->GetKey() == "key1";
            key_2_found = entry1_iter->GetKey() == "key2";
            EXPECT_TRUE(key_1_found || key_2_found);
            if (key_2_found) {
              EXPECT_EQ(entry1_iter, entry2_active);
            }
            entry1_iter->Close();
            // Now, make the recursive call to OpenNextEntry.
            ASSERT_THAT(
                iter->OpenNextEntry(
                        base::BindLambdaForTesting([&](EntryResult result2) {
                          ASSERT_THAT(result2.net_error(), IsOk());
                          // By this point, the CreateEntry for "key3" should
                          // have completed, proving that the normal operation
                          // was not starved.
                          CHECK(entry3);

                          auto* entry2_itr = result2.ReleaseEntry();
                          if (entry2_itr->GetKey() == "key3") {
                            EXPECT_EQ(entry2_itr, entry3);
                          } else {
                            if (key_1_found) {
                              EXPECT_EQ(entry2_itr->GetKey(), "key2");
                            } else {
                              EXPECT_EQ(entry2_itr->GetKey(), "key1");
                            }
                          }
                          entry2_itr->Close();
                          entry3->Close();
                          run_loop.Quit();
                        }))
                    .net_error(),
                IsError(net::ERR_IO_PENDING));
          }))
          .net_error(),
      IsError(net::ERR_IO_PENDING));

  // While the first OpenNextEntry is in flight, post a normal operation to
  // create a third entry.
  ASSERT_THAT(
      backend
          ->CreateEntry("key3", net::HIGHEST,
                        base::BindLambdaForTesting([&](EntryResult result3) {
                          ASSERT_THAT(result3.net_error(), IsOk());
                          entry3 = result3.ReleaseEntry();
                        }))
          .net_error(),
      IsError(net::ERR_IO_PENDING));
  run_loop.Run();

  // Close the initially active entry.
  entry2_active->Close();
}

// Tests that if a pending ReadData operation is aborted (e.g., due to backend
// destruction), the callback is invoked with net::ERR_ABORTED.
TEST_F(SqlBackendImplTest, AbortPendingReadData) {
  auto backend = CreateBackendAndInit();

  // Create an entry.
  TestEntryResultCompletionCallback create_cb;
  disk_cache::EntryResult create_result = create_cb.GetResult(
      backend->CreateEntry("key", net::HIGHEST, create_cb.callback()));
  ASSERT_THAT(create_result.net_error(), IsOk());
  auto* entry = create_result.ReleaseEntry();

  // Write some data to stream 1 so that a subsequent read will be pending.
  const std::string kBodyData = "body_data";
  auto write_buffer = base::MakeRefCounted<net::StringIOBuffer>(kBodyData);
  net::TestCompletionCallback write_cb;
  int write_rv =
      entry->WriteData(1, 0, write_buffer.get(), write_buffer->size(),
                       write_cb.callback(), false);
  ASSERT_EQ(write_cb.GetResult(write_rv),
            static_cast<int>(write_buffer->size()));

  // Initiate a ReadData operation, which will be pending.
  auto read_buffer = base::MakeRefCounted<net::IOBufferWithSize>(10);
  base::test::TestFuture<int> read_future;
  int rv = entry->ReadData(1, 0, read_buffer.get(), read_buffer->size(),
                           read_future.GetCallback());
  ASSERT_THAT(rv, IsError(net::ERR_IO_PENDING));

  // Destroy the backend while the read is in flight.
  backend.reset();

  // The callback should be aborted.
  EXPECT_EQ(read_future.Get(), net::ERR_ABORTED);

  entry->Close();
}

// Tests that if a pending WriteData operation is aborted (e.g., due to backend
// destruction), the callback is invoked with net::ERR_ABORTED.
TEST_F(SqlBackendImplTest, AbortPendingWriteData) {
  AddScopedFeatureList().InitAndEnableFeatureWithParameters(
      net::features::kDiskCacheBackendExperiment,
      {{net::features::kSqlDiskCacheMaxWriteBufferTotalSize.name, "0"}});

  auto backend = CreateBackendAndInit();

  // Create an entry.
  TestEntryResultCompletionCallback create_cb;
  disk_cache::EntryResult create_result = create_cb.GetResult(
      backend->CreateEntry("key", net::HIGHEST, create_cb.callback()));
  ASSERT_THAT(create_result.net_error(), IsOk());
  auto* entry = create_result.ReleaseEntry();

  backend->RunUntilAllTasksCompleteForTest();

  // Initiate a WriteData operation, which will be pending.
  auto write_buffer = base::MakeRefCounted<net::StringIOBuffer>("data");
  base::test::TestFuture<int> write_future;
  int rv = entry->WriteData(1, 0, write_buffer.get(), write_buffer->size(),
                            write_future.GetCallback(), false);
  ASSERT_THAT(rv, write_buffer->size());

  auto task_runners = backend->GetBackgroundTaskRunnersForTest();

  // Destroy the backend while the write is in flight.
  backend.reset();

  auto db_handle = static_cast<SqlEntryImpl*>(entry)->db_handle();
  while (!db_handle->GetError().has_value()) {
    FlushQueueInTaskRunners(task_runners);
  }

  // The db_handle should have been set to aborted.
  EXPECT_EQ(db_handle->GetError(), SqlPersistentStore::Error::kAborted);

  entry->Close();
}

// Tests that if a pending GetAvailableRange operation is aborted (e.g., due to
// backend destruction), the callback is invoked with net::ERR_ABORTED.
TEST_F(SqlBackendImplTest, AbortPendingGetAvailableRange) {
  auto backend = CreateBackendAndInit();

  // Create an entry.
  TestEntryResultCompletionCallback create_cb;
  disk_cache::EntryResult create_result = create_cb.GetResult(
      backend->CreateEntry("key", net::HIGHEST, create_cb.callback()));
  ASSERT_THAT(create_result.net_error(), IsOk());
  auto* entry = create_result.ReleaseEntry();

  // Initiate a GetAvailableRange operation, which will be pending.
  base::test::TestFuture<const RangeResult&> range_future;
  RangeResult result =
      entry->GetAvailableRange(0, 100, range_future.GetCallback());
  ASSERT_THAT(result.net_error, IsError(net::ERR_IO_PENDING));

  // Destroy the backend while the operation is in flight.
  backend.reset();

  // The callback should be aborted.
  const RangeResult& aborted_result = range_future.Get();
  EXPECT_THAT(aborted_result.net_error, IsError(net::ERR_ABORTED));

  entry->Close();
}

TEST_F(SqlBackendImplTest, DoomedEntriesCleanup) {
  // 1. Create a backend and add three entries with data.
  auto backend = CreateBackendAndInit();
  auto task_runners = backend->GetBackgroundTaskRunnersForTest();

  const std::string kKey1 = "key1";
  const std::string kKey2 = "key2";
  const std::string kKey3 = "key3";
  const std::string kData = "some data";

  auto* entry1 = CreateEntryAndWriteData(backend.get(), kKey1, kData);
  auto* entry2 = CreateEntryAndWriteData(backend.get(), kKey2, kData);
  auto* entry3 = CreateEntryAndWriteData(backend.get(), kKey3, kData);
  WaitUntilInitialized(*backend,
                       static_cast<SqlEntryImpl*>(entry3)->db_handle());
  auto res_id =
      static_cast<SqlEntryImpl*>(entry3)->db_handle()->GetResId().value();
  entry1->Close();
  entry2->Close();
  entry3->Close();

  backend.reset();

  FlushQueueInTaskRunners(task_runners);

  // 2. Open the database directly via SqlPersistentStore and doom the third
  // entry.
  {
    SqlAsyncTaskManager async_task_manager;
    auto store = std::make_unique<SqlPersistentStore>(
        temp_dir_.GetPath(), kDefaultMaxBytes, net::CacheType::DISK_CACHE,
        task_runners, async_task_manager, /*cleanup_tracker=*/nullptr);

    base::test::TestFuture<disk_cache::SqlPersistentStore::Error> future_init;
    store->Initialize(future_init.GetCallback());
    ASSERT_EQ(future_init.Get(), disk_cache::SqlPersistentStore::Error::kOk);

    base::test::TestFuture<SqlPersistentStore::Error> future_doom;
    store->DoomEntry(CacheEntryKey(kKey3), res_id,
                     /*accept_index_mismatch=*/false,
                     future_doom.GetCallback());
    EXPECT_EQ(future_doom.Get(), SqlPersistentStore::Error::kOk);

    store.reset();
  }

  FlushQueueInTaskRunners(task_runners);

  // 3. Recreate the backend
  backend = CreateBackendAndInit();
  EXPECT_TRUE(LoadInMemoryIndex(*backend));

  // 4. Open and doom the first and the second entries and let them as active.
  TestEntryResultCompletionCallback cb_open1;
  disk_cache::EntryResult open_result1 = cb_open1.GetResult(
      backend->OpenEntry(kKey1, net::HIGHEST, cb_open1.callback()));
  entry1 = open_result1.ReleaseEntry();
  entry1->Doom();

  TestEntryResultCompletionCallback cb_open2;
  disk_cache::EntryResult open_result2 = cb_open2.GetResult(
      backend->OpenEntry(kKey2, net::HIGHEST, cb_open2.callback()));
  entry2 = open_result2.ReleaseEntry();
  entry2->Doom();

  base::HistogramTester histogram_tester;
  backend->OnBrowserIdle();

  // Flush the queue to ensure that cleanup task is completed.
  backend->RunUntilAllTasksCompleteForTest();

  // Verify that `DeleteDoomedEntriesCount` UMA was recorded in the histogram.
  histogram_tester.ExpectUniqueSample(
      "Net.SqlDiskCache.DeleteDoomedEntriesCount", 1, 1);

  // 5. Verify that the data can still be read from the doomed entry.
  ReadAndVerifyData(entry1, kData);
  ReadAndVerifyData(entry2, kData);

  entry1->Close();
  entry2->Close();
}

TEST_F(SqlBackendImplTest, SpeculativeCreateEntry) {
  auto backend = CreateBackendAndInit();
  EXPECT_TRUE(LoadInMemoryIndex(*backend));

  const std::string kKey = "my-key";

  // Create an entry. This should return immediately with a speculatively
  // created entry.
  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result =
      backend->CreateEntry(kKey, net::HIGHEST, cb_create.callback());
  ASSERT_THAT(create_result.net_error(), IsOk());
  auto* entry = create_result.ReleaseEntry();
  ASSERT_TRUE(entry);

  auto db_handle = static_cast<SqlEntryImpl*>(entry)->db_handle();

  // The db_handle should be in the initial state
  EXPECT_TRUE(db_handle->IsInitialState());

  // Even after flushing all DB tasks, it should still be in the initial state.
  backend->RunUntilAllTasksCompleteForTest();
  EXPECT_TRUE(db_handle->IsInitialState());

  entry->Close();

  // Once closed, the DB side entry creation process starts.
  EXPECT_TRUE(db_handle->IsCreatingState());

  // After flushing all DB tasks, it enters the finished (created) state and the
  // ResID is set.
  backend->RunUntilAllTasksCompleteForTest();
  EXPECT_TRUE(db_handle->IsFinished());
  // Now the res_id should be available.
  EXPECT_TRUE(db_handle->GetResId().has_value());

  // The entry must be available
  TestEntryResultCompletionCallback cb_open;
  disk_cache::EntryResult open_result = cb_open.GetResult(
      backend->OpenEntry(kKey, net::HIGHEST, cb_open.callback()));
  ASSERT_THAT(open_result.net_error(), IsOk());
  entry = open_result.ReleaseEntry();
  ASSERT_TRUE(entry);
  entry->Close();
}

TEST_F(SqlBackendImplTest, SpeculativeCreateEntryDoomClose) {
  auto backend = CreateBackendAndInit();
  EXPECT_TRUE(LoadInMemoryIndex(*backend));
  const std::string kKey = "my-key";

  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result =
      backend->CreateEntry(kKey, net::HIGHEST, cb_create.callback());
  ASSERT_THAT(create_result.net_error(), IsOk());
  auto* entry = create_result.ReleaseEntry();
  ASSERT_TRUE(entry);
  auto db_handle = static_cast<SqlEntryImpl*>(entry)->db_handle();

  entry->Doom();
  entry->Close();

  // Verify that the entry is gone.
  TestEntryResultCompletionCallback cb_open;
  disk_cache::EntryResult open_result = cb_open.GetResult(
      backend->OpenEntry(kKey, net::HIGHEST, cb_open.callback()));
  EXPECT_THAT(open_result.net_error(), IsError(net::ERR_FAILED));

  EXPECT_TRUE(db_handle->IsInitialState());
}

TEST_F(SqlBackendImplTest, SpeculativeCreateEntryClose) {
  auto backend = CreateBackendAndInit();
  EXPECT_TRUE(LoadInMemoryIndex(*backend));
  const std::string kKey = "my-key";

  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result =
      backend->CreateEntry(kKey, net::HIGHEST, cb_create.callback());
  ASSERT_THAT(create_result.net_error(), IsOk());
  auto* entry = create_result.ReleaseEntry();
  ASSERT_TRUE(entry);
  auto db_handle = static_cast<SqlEntryImpl*>(entry)->db_handle();

  entry->Close();

  TestEntryResultCompletionCallback cb_open;
  disk_cache::EntryResult open_result = cb_open.GetResult(
      backend->OpenEntry(kKey, net::HIGHEST, cb_open.callback()));
  ASSERT_THAT(open_result.net_error(), IsOk());
  entry = open_result.ReleaseEntry();
  ASSERT_TRUE(entry);
  entry->Close();

  // The res_id should be available.
  EXPECT_TRUE(db_handle->GetResId().has_value());
}

TEST_F(SqlBackendImplTest, SpeculativeCreateEntryAndRead) {
  auto backend = CreateBackendAndInit();
  EXPECT_TRUE(LoadInMemoryIndex(*backend));
  const std::string kKey = "my-key";

  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result =
      backend->CreateEntry(kKey, net::HIGHEST, cb_create.callback());
  ASSERT_THAT(create_result.net_error(), IsOk());
  auto* entry = create_result.ReleaseEntry();
  ASSERT_TRUE(entry);
  auto db_handle = static_cast<SqlEntryImpl*>(entry)->db_handle();

  auto read_buffer = base::MakeRefCounted<net::IOBufferWithSize>(10);
  EXPECT_EQ(entry->ReadData(1, 0, read_buffer.get(), read_buffer->size(),
                            base::DoNothing()),
            0);

  EXPECT_TRUE(db_handle->IsInitialState());
  entry->Close();
  EXPECT_TRUE(db_handle->IsCreatingState());
  backend->RunUntilAllTasksCompleteForTest();
  // The res_id should be available now.
  EXPECT_TRUE(db_handle->GetResId().has_value());
}

TEST_F(SqlBackendImplTest, SpeculativeCreateEntryWriteWithinBufferLimit) {
  auto backend = CreateBackendAndInit();
  EXPECT_TRUE(LoadInMemoryIndex(*backend));
  const std::string kKey = "my-key";
  const std::string kData = "some data";

  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result =
      backend->CreateEntry(kKey, net::HIGHEST, cb_create.callback());
  ASSERT_THAT(create_result.net_error(), IsOk());
  auto* entry = create_result.ReleaseEntry();
  ASSERT_TRUE(entry);
  auto db_handle = static_cast<SqlEntryImpl*>(entry)->db_handle();

  net::TestCompletionCallback write_cb;
  auto write_buffer = base::MakeRefCounted<net::StringIOBuffer>(kData);
  ASSERT_LT(write_buffer->size(),
            net::features::kSqlDiskCacheMaxWriteBufferSizePerEntry.Get());
  EXPECT_EQ(write_cb.GetResult(entry->WriteData(1, 0, write_buffer.get(),
                                                write_buffer->size(),
                                                write_cb.callback(), false)),
            static_cast<int>(write_buffer->size()));

  // If the written data is less than kSqlDiskCacheMaxWriteBufferSizePerEntry,
  // the DB side write process only starts when the entry is closed.
  EXPECT_TRUE(db_handle->IsInitialState());
  entry->Close();
  EXPECT_TRUE(db_handle->IsCreatingState());

  TestEntryResultCompletionCallback cb_open;
  disk_cache::EntryResult open_result = cb_open.GetResult(
      backend->OpenEntry(kKey, net::HIGHEST, cb_open.callback()));
  ASSERT_THAT(open_result.net_error(), IsOk());

  // The res_id should be available.
  EXPECT_TRUE(db_handle->GetResId().has_value());

  entry = open_result.ReleaseEntry();
  ASSERT_TRUE(entry);

  auto read_buffer = base::MakeRefCounted<net::IOBufferWithSize>(10);

  base::test::TestFuture<int> read_future;
  int rv = entry->ReadData(1, 0, read_buffer.get(), read_buffer->size(),
                           read_future.GetCallback());
  ASSERT_THAT(rv, IsError(net::ERR_IO_PENDING));
  ASSERT_EQ(read_future.Get(), write_buffer->size());

  EXPECT_EQ(std::string_view(read_buffer->data(), kData.size()), kData);
  entry->Close();
}

TEST_F(SqlBackendImplTest,
       SpeculativeCreateEntryWriteWithinBufferLimitAndDoom) {
  auto backend = CreateBackendAndInit();
  EXPECT_TRUE(LoadInMemoryIndex(*backend));
  const std::string kKey = "my-key";
  const std::string kData = "some data";

  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result =
      backend->CreateEntry(kKey, net::HIGHEST, cb_create.callback());
  ASSERT_THAT(create_result.net_error(), IsOk());
  auto* entry = create_result.ReleaseEntry();
  ASSERT_TRUE(entry);
  auto db_handle = static_cast<SqlEntryImpl*>(entry)->db_handle();

  net::TestCompletionCallback write_cb;
  auto write_buffer = base::MakeRefCounted<net::StringIOBuffer>(kData);
  ASSERT_LT(write_buffer->size(),
            net::features::kSqlDiskCacheMaxWriteBufferSizePerEntry.Get());
  EXPECT_EQ(write_cb.GetResult(entry->WriteData(1, 0, write_buffer.get(),
                                                write_buffer->size(),
                                                write_cb.callback(), false)),
            static_cast<int>(write_buffer->size()));

  entry->Doom();
  entry->Close();

  // Verify that the entry is gone.
  TestEntryResultCompletionCallback cb_open;
  disk_cache::EntryResult open_result = cb_open.GetResult(
      backend->OpenEntry(kKey, net::HIGHEST, cb_open.callback()));
  EXPECT_THAT(open_result.net_error(), IsError(net::ERR_FAILED));

  // If the written data is less than kSqlDiskCacheMaxWriteBufferSizePerEntry,
  // the DB side write process does not start if the entry is doomed before
  // being closed.
  EXPECT_TRUE(db_handle->IsInitialState());
}

TEST_F(SqlBackendImplTest, SpeculativeCreateEntryOptimisticWriteOnBufferFlush) {
  AddScopedFeatureList().InitAndEnableFeatureWithParameters(
      net::features::kDiskCacheBackendExperiment,
      {{net::features::kSqlDiskCacheMaxWriteBufferSizePerEntry.name, "10"}});
  auto backend = CreateBackendAndInit();
  EXPECT_TRUE(LoadInMemoryIndex(*backend));

  const std::string kKey = "my-key";

  // Create an entry. This should return immediately with a speculatively
  // created entry.
  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result =
      backend->CreateEntry(kKey, net::HIGHEST, cb_create.callback());
  ASSERT_THAT(create_result.net_error(), IsOk());
  auto* entry = create_result.ReleaseEntry();
  ASSERT_TRUE(entry);
  auto db_handle = static_cast<SqlEntryImpl*>(entry)->db_handle();

  const std::string k1ByteData = "X";
  auto buffer = base::MakeRefCounted<net::StringIOBuffer>(k1ByteData);

  // Write 10 bytes. These are all buffered in memory within the entry.
  for (int64_t i = 0; i < 10; ++i) {
    EXPECT_EQ(entry->WriteData(1, i, buffer.get(), buffer->size(),
                               base::DoNothing(), false),
              1);
  }
  // Even after flushing all DB tasks, it should still be in the initial state.
  backend->RunUntilAllTasksCompleteForTest();
  EXPECT_TRUE(db_handle->IsInitialState());

  // Writing one more byte triggers the buffered content to be passed to
  // SqlBackendImpl, starting the optimistic write process. Then, `db_handle`
  // enters the creating state.
  EXPECT_EQ(entry->WriteData(1, 10, buffer.get(), buffer->size(),
                             base::DoNothing(), false),
            1);
  EXPECT_TRUE(db_handle->IsCreatingState());

  // After flushing all DB tasks, it enters the finished (created) state and the
  // ResID is set.
  backend->RunUntilAllTasksCompleteForTest();
  EXPECT_TRUE(db_handle->IsFinished());
  EXPECT_TRUE(db_handle->GetResId().has_value());

  // Doom the entry.
  entry->Doom();
  entry->Close();

  // Verify that the entry is gone.
  TestEntryResultCompletionCallback cb_open;
  disk_cache::EntryResult open_result = cb_open.GetResult(
      backend->OpenEntry(kKey, net::HIGHEST, cb_open.callback()));
  EXPECT_THAT(open_result.net_error(), IsError(net::ERR_FAILED));
}

TEST_F(SqlBackendImplTest, SpeculativeCreateEntryNonOptmisticWrite) {
  AddScopedFeatureList().InitAndEnableFeatureWithParameters(
      net::features::kDiskCacheBackendExperiment,
      {{net::features::kSqlDiskCacheOptimisticWriteBufferSize.name, "10"},
       {net::features::kSqlDiskCacheMaxWriteBufferTotalSize.name, "10"}});
  auto backend = CreateBackendAndInit();
  const std::string kKey = "my-key";
  EXPECT_TRUE(LoadInMemoryIndex(*backend));
  TestEntryResultCompletionCallback cb;
  disk_cache::EntryResult entry_result =
      backend->CreateEntry(kKey, net::HIGHEST, cb.callback());
  ASSERT_THAT(entry_result.net_error(), IsOk());
  auto* entry = entry_result.ReleaseEntry();
  ASSERT_TRUE(entry);
  auto db_handle = static_cast<SqlEntryImpl*>(entry)->db_handle();

  // When WriteData is called with data larger than
  // kSqlDiskCacheMaxWriteBufferTotalSize, and
  // kSqlDiskCacheOptimisticWriteBufferSize, SqlBackendImpl starts the
  // non-optimistic write process, and the `db_handle` enters the creating
  // state.
  const std::string kData = "1234567890A";
  auto write_buffer = base::MakeRefCounted<net::StringIOBuffer>(kData);
  ASSERT_GT(write_buffer->size(),
            net::features::kSqlDiskCacheOptimisticWriteBufferSize.Get());
  ASSERT_GT(write_buffer->size(),
            net::features::kSqlDiskCacheMaxWriteBufferTotalSize.Get());
  EXPECT_TRUE(db_handle->IsInitialState());

  net::TestCompletionCallback write_cb;
  EXPECT_EQ(entry->WriteData(1, 0, write_buffer.get(), write_buffer->size(),
                             write_cb.callback(), false),
            net::ERR_IO_PENDING);
  EXPECT_TRUE(db_handle->IsCreatingState());
  EXPECT_EQ(write_cb.WaitForResult(), write_buffer->size());
  EXPECT_TRUE(db_handle->IsFinished());
  EXPECT_TRUE(db_handle->GetResId().has_value());
  entry->Close();

  TestEntryResultCompletionCallback cb_open;
  disk_cache::EntryResult open_result = cb_open.GetResult(
      backend->OpenEntry(kKey, net::HIGHEST, cb_open.callback()));
  ASSERT_THAT(open_result.net_error(), IsOk());

  entry = open_result.ReleaseEntry();
  ASSERT_TRUE(entry);

  auto read_buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(kData.size() + 1);

  base::test::TestFuture<int> read_future;
  int rv = entry->ReadData(1, 0, read_buffer.get(), read_buffer->size(),
                           read_future.GetCallback());
  ASSERT_THAT(rv, IsError(net::ERR_IO_PENDING));
  ASSERT_EQ(read_future.Get(), write_buffer->size());

  EXPECT_EQ(std::string_view(read_buffer->data(), kData.size()), kData);
  entry->Close();
}

TEST_F(SqlBackendImplTest, SpeculativeCreateEntryWithDbFailure) {
  auto backend = CreateBackendAndInit();
  EXPECT_TRUE(LoadInMemoryIndex(*backend));
  backend->GetSqlStoreForTest()->SetSimulateDbFailureForTesting(true);
  const std::string kKey = "my-key";

  // Create an entry. This should return immediately with a speculatively
  // created entry.
  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result =
      backend->CreateEntry(kKey, net::HIGHEST, cb_create.callback());
  ASSERT_THAT(create_result.net_error(), IsOk());
  auto* entry = create_result.ReleaseEntry();
  ASSERT_TRUE(entry);

  auto db_handle = static_cast<SqlEntryImpl*>(entry)->db_handle();

  // The db_handle should be in the initial state
  EXPECT_TRUE(db_handle->IsInitialState());

  entry->Close();

  // Once closed, the DB side entry creation process starts.
  EXPECT_TRUE(db_handle->IsCreatingState());
  backend->RunUntilAllTasksCompleteForTest();

  // After flushing all DB tasks, the db_handle should hold a kFailedForTesting
  // error.
  EXPECT_THAT(db_handle->GetError(),
              SqlPersistentStore::Error::kFailedForTesting);

  // 6. Verify that the entry is not found.
  TestEntryResultCompletionCallback cb_open;
  disk_cache::EntryResult open_result = cb_open.GetResult(
      backend->OpenEntry(kKey, net::HIGHEST, cb_open.callback()));
  EXPECT_THAT(open_result.net_error(), IsError(net::ERR_FAILED));
}

TEST_F(SqlBackendImplTest, SpeculativeCreateEntryDbFailureOnOptmisticWrite) {
  AddScopedFeatureList().InitAndEnableFeatureWithParameters(
      net::features::kDiskCacheBackendExperiment,
      {{net::features::kSqlDiskCacheOptimisticWriteBufferSize.name, "20"},
       {net::features::kSqlDiskCacheMaxWriteBufferTotalSize.name, "10"}});
  auto backend = CreateBackendAndInit();
  EXPECT_TRUE(LoadInMemoryIndex(*backend));
  backend->GetSqlStoreForTest()->SetSimulateDbFailureForTesting(true);
  TestEntryResultCompletionCallback cb;
  disk_cache::EntryResult entry_result =
      backend->CreateEntry("key", net::HIGHEST, cb.callback());
  ASSERT_THAT(entry_result.net_error(), IsOk());
  auto* entry = entry_result.ReleaseEntry();
  ASSERT_TRUE(entry);
  auto db_handle = static_cast<SqlEntryImpl*>(entry)->db_handle();

  // When WriteData is called with data larger than
  // kSqlDiskCacheMaxWriteBufferTotalSize, and smaller than
  // kSqlDiskCacheOptimisticWriteBufferSize, SqlBackendImpl starts the
  // optimistic write process, and the `db_handle` enters the creating state.
  auto write_buffer = base::MakeRefCounted<net::StringIOBuffer>("1234567890A");
  ASSERT_LE(write_buffer->size(),
            net::features::kSqlDiskCacheOptimisticWriteBufferSize.Get());
  ASSERT_GT(write_buffer->size(),
            net::features::kSqlDiskCacheMaxWriteBufferTotalSize.Get());
  EXPECT_TRUE(db_handle->IsInitialState());
  EXPECT_EQ(entry->WriteData(1, 0, write_buffer.get(), write_buffer->size(),
                             base::DoNothing(), false),
            write_buffer->size());
  EXPECT_TRUE(db_handle->IsCreatingState());

  // The second write exceeds the optimistic write limit, so it becomes async.
  ASSERT_GT(write_buffer->size() * 2,
            net::features::kSqlDiskCacheOptimisticWriteBufferSize.Get());
  net::TestCompletionCallback write_cb;
  EXPECT_EQ(entry->WriteData(1, write_buffer->size(), write_buffer.get(),
                             write_buffer->size(), write_cb.callback(), false),
            net::ERR_IO_PENDING);

  net::TestCompletionCallback read_cb;
  auto read_buffer = base::MakeRefCounted<net::IOBufferWithSize>(10);
  EXPECT_EQ(entry->ReadData(1, 0, read_buffer.get(), read_buffer->size(),
                            read_cb.callback()),
            net::ERR_IO_PENDING);

  base::test::TestFuture<const RangeResult&> range_future;
  EXPECT_EQ(
      entry->GetAvailableRange(0, 10, range_future.GetCallback()).net_error,
      net::ERR_IO_PENDING);

  EXPECT_THAT(write_cb.WaitForResult(), IsError(net::ERR_FAILED));
  EXPECT_THAT(read_cb.WaitForResult(), IsError(net::ERR_FAILED));
  EXPECT_THAT(range_future.Get().net_error, IsError(net::ERR_FAILED));

  // After finishing all DB tasks, the db_handle should hold a kFailedForTesting
  // error.
  EXPECT_THAT(db_handle->GetError(),
              SqlPersistentStore::Error::kFailedForTesting);

  entry->Close();
}

TEST_F(SqlBackendImplTest, SpeculativeCreateEntryDbFailureOnNonOptmisticWrite) {
  AddScopedFeatureList().InitAndEnableFeatureWithParameters(
      net::features::kDiskCacheBackendExperiment,
      {{net::features::kSqlDiskCacheOptimisticWriteBufferSize.name, "10"},
       {net::features::kSqlDiskCacheMaxWriteBufferTotalSize.name, "10"}});
  auto backend = CreateBackendAndInit();
  EXPECT_TRUE(LoadInMemoryIndex(*backend));
  backend->GetSqlStoreForTest()->SetSimulateDbFailureForTesting(true);
  TestEntryResultCompletionCallback cb;
  disk_cache::EntryResult entry_result =
      backend->CreateEntry("key", net::HIGHEST, cb.callback());
  ASSERT_THAT(entry_result.net_error(), IsOk());
  auto* entry = entry_result.ReleaseEntry();
  ASSERT_TRUE(entry);
  auto db_handle = static_cast<SqlEntryImpl*>(entry)->db_handle();

  // When WriteData is called with data larger than
  // kSqlDiskCacheMaxWriteBufferTotalSize, and
  // kSqlDiskCacheOptimisticWriteBufferSize, SqlBackendImpl starts the
  // non-optimistic write process, and the `db_handle` enters the creating
  // state.
  auto write_buffer = base::MakeRefCounted<net::StringIOBuffer>("1234567890A");
  ASSERT_GT(write_buffer->size(),
            net::features::kSqlDiskCacheOptimisticWriteBufferSize.Get());
  ASSERT_GT(write_buffer->size(),
            net::features::kSqlDiskCacheMaxWriteBufferTotalSize.Get());
  EXPECT_TRUE(db_handle->IsInitialState());

  net::TestCompletionCallback write_cb;
  EXPECT_EQ(entry->WriteData(1, 0, write_buffer.get(), write_buffer->size(),
                             write_cb.callback(), false),
            net::ERR_IO_PENDING);
  EXPECT_TRUE(db_handle->IsCreatingState());
  EXPECT_EQ(write_cb.WaitForResult(), net::ERR_FAILED);
  EXPECT_THAT(db_handle->GetError(),
              SqlPersistentStore::Error::kFailedForTesting);
  entry->Close();
}

TEST_F(SqlBackendImplTest, SpeculativeCreateEntryDbFailureDoom) {
  AddScopedFeatureList().InitAndEnableFeatureWithParameters(
      net::features::kDiskCacheBackendExperiment,
      {{net::features::kSqlDiskCacheMaxWriteBufferTotalSize.name, "10"}});
  auto backend = CreateBackendAndInit();
  EXPECT_TRUE(LoadInMemoryIndex(*backend));
  backend->GetSqlStoreForTest()->SetSimulateDbFailureForTesting(true);
  TestEntryResultCompletionCallback cb;
  disk_cache::EntryResult entry_result =
      backend->CreateEntry("key", net::HIGHEST, cb.callback());
  ASSERT_THAT(entry_result.net_error(), IsOk());
  auto* entry = entry_result.ReleaseEntry();
  ASSERT_TRUE(entry);
  auto db_handle = static_cast<SqlEntryImpl*>(entry)->db_handle();

  // When WriteData is called with data larger than
  // kSqlDiskCacheMaxWriteBufferTotalSize, SqlBackendImpl starts the optimistic
  // write process, and the `db_handle` enters the creating state.
  auto write_buffer = base::MakeRefCounted<net::StringIOBuffer>("1234567890A");
  ASSERT_GT(write_buffer->size(),
            net::features::kSqlDiskCacheMaxWriteBufferTotalSize.Get());
  EXPECT_TRUE(db_handle->IsInitialState());
  EXPECT_EQ(
      entry->WriteData(1, 0, write_buffer.get(), write_buffer->size(),
                       base::BindOnce([](int rv) { NOTREACHED(); }), false),
      write_buffer->size());
  EXPECT_TRUE(db_handle->IsCreatingState());
  // After flushing all DB tasks, the db_handle should hold a kFailedForTesting
  // error.
  backend->RunUntilAllTasksCompleteForTest();
  EXPECT_THAT(db_handle->GetError(),
              SqlPersistentStore::Error::kFailedForTesting);

  entry->Doom();
  EXPECT_TRUE(static_cast<SqlEntryImpl*>(entry)->doomed());
  entry->Close();
  entry = nullptr;

  TestEntryResultCompletionCallback cb_open;
  disk_cache::EntryResult open_result = cb_open.GetResult(
      backend->OpenEntry("key", net::HIGHEST, cb_open.callback()));
  EXPECT_THAT(open_result.net_error(), IsError(net::ERR_FAILED));
}

TEST_F(SqlBackendImplTest, OptimisticWriteBufferSize) {
  AddScopedFeatureList().InitAndEnableFeatureWithParameters(
      net::features::kDiskCacheBackendExperiment,
      {{net::features::kSqlDiskCacheOptimisticWriteBufferSize.name, "100"},
       {net::features::kSqlDiskCacheMaxWriteBufferTotalSize.name, "0"}});

  auto backend = CreateBackendAndInit();
  EXPECT_TRUE(LoadInMemoryIndex(*backend));
  TestEntryResultCompletionCallback cb;
  disk_cache::EntryResult entry_result =
      backend->CreateEntry("key", net::HIGHEST, cb.callback());
  ASSERT_THAT(entry_result.net_error(), IsOk());
  auto* entry = entry_result.ReleaseEntry();
  ASSERT_TRUE(entry);

  // First write is smaller than the buffer size, should be optimistic.
  auto write_buffer1 = base::MakeRefCounted<net::StringIOBuffer>("data1");
  EXPECT_EQ(entry->WriteData(1, 0, write_buffer1.get(), write_buffer1->size(),
                             base::DoNothing(), false),
            static_cast<int>(write_buffer1->size()));
  EXPECT_EQ(backend->GetOptimisticWriteBufferTotalSizeForTesting(),
            write_buffer1->size());

  // Second write exceeds the buffer size, should be pending.
  auto write_buffer2 =
      base::MakeRefCounted<net::StringIOBuffer>(std::string(100, 'a'));
  net::TestCompletionCallback write_cb;
  EXPECT_EQ(entry->WriteData(1, write_buffer1->size(), write_buffer2.get(),
                             write_buffer2->size(), write_cb.callback(), false),
            net::ERR_IO_PENDING);
  EXPECT_EQ(backend->GetOptimisticWriteBufferTotalSizeForTesting(),
            write_buffer1->size());

  EXPECT_EQ(write_cb.WaitForResult(), static_cast<int>(write_buffer2->size()));
  EXPECT_EQ(backend->GetOptimisticWriteBufferTotalSizeForTesting(), 0);

  entry->Close();
}

TEST_F(SqlBackendImplTest, OptimisticWriteBufferLifecycle) {
  AddScopedFeatureList().InitAndEnableFeatureWithParameters(
      net::features::kDiskCacheBackendExperiment,
      {{net::features::kSqlDiskCacheOptimisticWriteBufferSize.name, "100"},
       {net::features::kSqlDiskCacheMaxWriteBufferTotalSize.name, "0"}});

  auto backend = CreateBackendAndInit();
  EXPECT_TRUE(LoadInMemoryIndex(*backend));
  TestEntryResultCompletionCallback cb;
  disk_cache::EntryResult entry_result =
      backend->CreateEntry("key", net::HIGHEST, cb.callback());
  ASSERT_THAT(entry_result.net_error(), IsOk());
  auto* entry = entry_result.ReleaseEntry();
  ASSERT_TRUE(entry);

  // This write should be optimistic.
  auto write_buffer1 =
      base::MakeRefCounted<net::StringIOBuffer>(std::string(50, 'a'));
  EXPECT_EQ(entry->WriteData(1, 0, write_buffer1.get(), write_buffer1->size(),
                             base::DoNothing(), false),
            static_cast<int>(write_buffer1->size()));
  EXPECT_EQ(backend->GetOptimisticWriteBufferTotalSizeForTesting(),
            write_buffer1->size());

  // This write should also be optimistic, filling the buffer.
  auto write_buffer2 =
      base::MakeRefCounted<net::StringIOBuffer>(std::string(50, 'b'));
  EXPECT_EQ(entry->WriteData(1, 50, write_buffer2.get(), write_buffer2->size(),
                             base::DoNothing(), false),
            static_cast<int>(write_buffer2->size()));
  EXPECT_EQ(backend->GetOptimisticWriteBufferTotalSizeForTesting(),
            write_buffer1->size() + write_buffer2->size());

  // This write should be pending as the buffer is full.
  auto write_buffer3 = base::MakeRefCounted<net::StringIOBuffer>("c");
  net::TestCompletionCallback write_cb3;
  EXPECT_EQ(entry->WriteData(1, 100, write_buffer3.get(), write_buffer3->size(),
                             write_cb3.callback(), false),
            net::ERR_IO_PENDING);
  EXPECT_EQ(backend->GetOptimisticWriteBufferTotalSizeForTesting(),
            write_buffer1->size() + write_buffer2->size());

  // Flush the queue. This will ensure the first two optimistic writes complete
  // on the background thread, which will free up the buffer and allow the
  // pending write to proceed.
  backend->RunUntilAllTasksCompleteForTest();

  EXPECT_EQ(backend->GetOptimisticWriteBufferTotalSizeForTesting(), 0);

  // Now that the queue is flushed, the pending write should have completed.
  EXPECT_EQ(write_cb3.WaitForResult(), static_cast<int>(write_buffer3->size()));

  EXPECT_EQ(backend->GetOptimisticWriteBufferTotalSizeForTesting(), 0);
  // The buffer should be free again, so this write should be optimistic.
  auto write_buffer4 =
      base::MakeRefCounted<net::StringIOBuffer>(std::string(50, 'd'));
  EXPECT_EQ(entry->WriteData(1, 101, write_buffer4.get(), write_buffer4->size(),
                             base::DoNothing(), false),
            static_cast<int>(write_buffer4->size()));
  EXPECT_EQ(backend->GetOptimisticWriteBufferTotalSizeForTesting(),
            write_buffer4->size());

  entry->Close();

  backend->RunUntilAllTasksCompleteForTest();

  EXPECT_EQ(backend->GetOptimisticWriteBufferTotalSizeForTesting(), 0);
}

TEST_F(SqlBackendImplTest, OptimisticWriteFailure) {
  AddScopedFeatureList().InitAndEnableFeatureWithParameters(
      net::features::kDiskCacheBackendExperiment,
      {{net::features::kSqlDiskCacheOptimisticWriteBufferSize.name, "100"},
       {net::features::kSqlDiskCacheMaxWriteBufferTotalSize.name, "0"}});

  auto backend = CreateBackendAndInit();
  EXPECT_TRUE(LoadInMemoryIndex(*backend));
  const std::string kKey = "my-key";
  const std::string kInitialData = "initial data";

  // 1. Create an entry with some data.
  auto* entry = CreateEntryAndWriteData(backend.get(), kKey, kInitialData);
  entry->Close();

  // 2. Re-open the entry.
  TestEntryResultCompletionCallback open_cb;
  disk_cache::EntryResult open_result = open_cb.GetResult(
      backend->OpenEntry(kKey, net::HIGHEST, open_cb.callback()));
  ASSERT_THAT(open_result.net_error(), IsOk());
  entry = open_result.ReleaseEntry();
  auto* sql_entry = static_cast<SqlEntryImpl*>(entry);

  // 3. Simulate a database failure for subsequent operations.
  backend->GetSqlStoreForTest()->SetSimulateDbFailureForTesting(true);

  // 4. Perform an optimistic write, which should fail in the background.
  auto write_buffer = base::MakeRefCounted<net::StringIOBuffer>("new data");
  EXPECT_EQ(entry->WriteData(1, kInitialData.size(), write_buffer.get(),
                             write_buffer->size(), base::DoNothing(), false),
            static_cast<int>(write_buffer->size()));
  EXPECT_EQ(backend->GetOptimisticWriteBufferTotalSizeForTesting(),
            write_buffer->size());

  // 5. Disable failure simulation.
  backend->GetSqlStoreForTest()->SetSimulateDbFailureForTesting(false);

  // 6. Wait for the background write to fail and update the entry's state.
  backend->RunUntilAllTasksCompleteForTest();

  // 7. Verify that the entry is now in an error state.
  EXPECT_TRUE(sql_entry->db_handle()->IsFinished());
  EXPECT_THAT(sql_entry->db_handle()->GetError(),
              SqlPersistentStore::Error::kFailedForTesting);
  EXPECT_EQ(backend->GetOptimisticWriteBufferTotalSizeForTesting(), 0);

  // 8. Subsequent writes should fail immediately.
  EXPECT_EQ(entry->WriteData(1, 0, write_buffer.get(), write_buffer->size(),
                             base::DoNothing(), false),
            net::ERR_FAILED);
  EXPECT_EQ(backend->GetOptimisticWriteBufferTotalSizeForTesting(), 0);

  entry->Close();

  // 9. Since the entry should have been deleted from storage, OpenEntry will
  //    fail.
  TestEntryResultCompletionCallback open_cb2;
  open_result = open_cb2.GetResult(
      backend->OpenEntry(kKey, net::HIGHEST, open_cb2.callback()));
  ASSERT_THAT(open_result.net_error(), IsError(net::ERR_FAILED));
}

TEST_F(SqlBackendImplTest, IdleTimeEviction) {
  const int64_t kMaxBytes = 100000;
  const int64_t kIdleTimeHighWatermark =
      kMaxBytes * kSqlBackendIdleTimeEvictionHighWaterMarkPermille /
      1000;  // 92500
  auto buffer =
      base::MakeRefCounted<net::StringIOBuffer>(std::string(1000, 'x'));

  auto backend = CreateBackendAndInit(kMaxBytes);

  // Add entries to be above idle time watermark.
  int i = 0;
  while (GetSizeOfAllEntries(*backend) <= kIdleTimeHighWatermark) {
    TestEntryResultCompletionCallback cb;
    EntryResult result = cb.GetResult(backend->CreateEntry(
        base::StringPrintf("key%d", i++), net::HIGHEST, cb.callback()));
    ASSERT_THAT(result.net_error(), IsOk());
    auto* entry = result.ReleaseEntry();
    net::TestCompletionCallback write_cb;
    EXPECT_EQ(
        write_cb.GetResult(entry->WriteData(1, 0, buffer.get(), buffer->size(),
                                            write_cb.callback(), false)),
        buffer->size());
    entry->Close();
    backend->RunUntilAllTasksCompleteForTest();
  }

  auto test_helper = PerformanceScenarioTestHelper::Create();
  // Set the state to idle.
  test_helper->SetLoadingScenario(ScenarioScope::kGlobal,
                                  LoadingScenario::kNoPageLoading);
  test_helper->SetInputScenario(ScenarioScope::kGlobal,
                                InputScenario::kNoInput);

  // Trigger idle time eviction.
  backend->OnBrowserIdle();

  // The eviction process involves multiple asynchronous steps across different
  // shards and the EvictionCandidateAggregator. Since all these steps are
  // tracked by SqlAsyncTaskManager, a single RunUntilAllTasksCompleteForTest()
  // is sufficient to wait for the entire process to complete.
  backend->RunUntilAllTasksCompleteForTest();

  // Eviction should have run and reduced the size.
  const int64_t kLowWatermark =
      kMaxBytes * kSqlBackendEvictionLowWaterMarkPermille / 1000;  // 9000
  EXPECT_LE(GetSizeOfAllEntries(*backend), kLowWatermark);
}

void SqlBackendImplTest::RunDelayedPostInitializationTasksTest() {
  auto backend = CreateBackendAndInit();
  auto* sql_store = backend->GetSqlStoreForTest();
  auto task_runners = backend->GetBackgroundTaskRunnersForTest();

  const auto kKey1 = CacheEntryKey("key1");
  const auto kKey2 = CacheEntryKey("key2");
  const std::string kData = "some data";
  const auto shard_id1 = sql_store->GetShardIdForHash(kKey1.hash());
  const auto shard_id2 = sql_store->GetShardIdForHash(kKey2.hash());

  // Create two entries and write some data to them.
  auto* entry1 = CreateEntryAndWriteData(backend.get(), kKey1.string(), kData);
  auto* entry2 = CreateEntryAndWriteData(backend.get(), kKey2.string(), kData);
  auto db_handle1 = static_cast<SqlEntryImpl*>(entry1)->db_handle();
  auto db_handle2 = static_cast<SqlEntryImpl*>(entry1)->db_handle();
  entry1->Close();
  entry2->Close();
  backend->RunUntilAllTasksCompleteForTest();
  ASSERT_TRUE(db_handle1->GetResId().has_value());
  ASSERT_TRUE(db_handle2->GetResId().has_value());
  auto res_id1 = db_handle1->GetResId().value();
  auto res_id2 = db_handle2->GetResId().value();

  // Close the backend to ensure everything is written to disk.
  backend.reset();

  FlushQueueInTaskRunners(task_runners);

  // This block simulates a previous session where an entry was doomed but not
  // fully cleaned up.
  {
    SqlAsyncTaskManager async_task_manager;
    auto store = std::make_unique<SqlPersistentStore>(
        temp_dir_.GetPath(), kDefaultMaxBytes, net::CacheType::DISK_CACHE,
        task_runners, async_task_manager, /*cleanup_tracker=*/nullptr);

    base::test::TestFuture<disk_cache::SqlPersistentStore::Error> future_init;
    store->Initialize(future_init.GetCallback());
    ASSERT_EQ(future_init.Get(), disk_cache::SqlPersistentStore::Error::kOk);

    // Doom one of the entries.
    base::test::TestFuture<SqlPersistentStore::Error> future_doom;
    store->DoomEntry(kKey1, res_id1, /*accept_index_mismatch=*/false,
                     future_doom.GetCallback());
    EXPECT_EQ(future_doom.Get(), SqlPersistentStore::Error::kOk);

    store.reset();

    FlushQueueInTaskRunners(task_runners);
  }

  // Verify directly in the database that the blobs for the entries still exist.
  EXPECT_EQ(OpenDatabaseAndGetBlobsCount(shard_id1, res_id1), 1);
  EXPECT_EQ(OpenDatabaseAndGetBlobsCount(shard_id2, res_id2), 1);

  // Create and initialize a new backend.
  backend = CreateBackend();
  sql_store = backend->GetSqlStoreForTest();
  base::test::TestFuture<int> future;
  backend->Init(future.GetCallback());
  ASSERT_EQ(future.Get(), net::OK);

  backend->RunUntilAllTasksCompleteForTest();

  if (net::features::kSqlDiskCacheLoadIndexOnInit.Get()) {
    // When the SqlDiskCacheLoadIndexOnInit is enabled, the index should have
    // been loaded. The doomed entry should be gone, and the other entry should
    // be present.
    EXPECT_EQ(sql_store->GetIndexStateForHash(kKey1.hash()),
              SqlPersistentStore::IndexState::kHashNotFound);
    EXPECT_EQ(sql_store->GetIndexStateForHash(kKey2.hash()),
              SqlPersistentStore::IndexState::kHashFound);
  } else {
    // At this point, the in-memory index should not be loaded yet.
    EXPECT_EQ(sql_store->GetIndexStateForHash(kKey1.hash()),
              SqlPersistentStore::IndexState::kNotReady);
    EXPECT_EQ(sql_store->GetIndexStateForHash(kKey2.hash()),
              SqlPersistentStore::IndexState::kNotReady);
  }

  // Fast forward time to trigger the delayed post-initialization tasks.
  FastForwardBy(kSqlBackendPostInitializationTasksDelay);

  backend->RunUntilAllTasksCompleteForTest();

  // Now, the index should be loaded even if SqlDiskCacheLoadIndexOnInit is
  // disabled. The doomed entry should be gone, and the other entry should be
  // present.
  EXPECT_EQ(sql_store->GetIndexStateForHash(kKey1.hash()),
            SqlPersistentStore::IndexState::kHashNotFound);
  EXPECT_EQ(sql_store->GetIndexStateForHash(kKey2.hash()),
            SqlPersistentStore::IndexState::kHashFound);

  task_runners = backend->GetBackgroundTaskRunnersForTest();
  backend.reset();

  FlushQueueInTaskRunners(task_runners);

  // Verify directly in the database that the blob for the doomed entry has been
  // deleted, while the other one still exists.
  EXPECT_EQ(OpenDatabaseAndGetBlobsCount(shard_id1, res_id1), 0);
  EXPECT_EQ(OpenDatabaseAndGetBlobsCount(shard_id2, res_id2), 1);
}

TEST_F(SqlBackendImplTest, DelayedPostInitializationTasks) {
  RunDelayedPostInitializationTasksTest();
}

TEST_F(SqlBackendImplTest,
       DelayedPostInitializationTasksWithLoadIndexOnInitFeature) {
  AddScopedFeatureList().InitWithFeaturesAndParameters(
      {{net::features::kDiskCacheBackendExperiment,
        {{net::features::kDiskCacheBackendParam.name, "sql"},
         {net::features::kSqlDiskCacheLoadIndexOnInit.name, "true"}}}},
      {});
  RunDelayedPostInitializationTasksTest();
}

// Regression test for https://crbug.com/456384561
// Tests that the dangling pointer warning does not occur when the backend is
// destroyed with a pending operation that holds the last reference to an entry.
// This test reproduces the scenario where the destruction order of
// `SqlBackendImpl` members (`exclusive_operation_coordinator_` before
// `active_entries_`) could lead to a dangling `raw_ref` in `active_entries_`.
TEST_F(SqlBackendImplTest, DestructionWithPendingOperationOnEntry) {
  auto backend = CreateBackendAndInit();

  // 1. Create an entry.
  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result = cb_create.GetResult(
      backend->CreateEntry("key", net::HIGHEST, cb_create.callback()));
  ASSERT_THAT(create_result.net_error(), IsOk());
  auto* entry = create_result.ReleaseEntry();

  // 2. Post a separate async exclusive operation. This ensures that the
  //    subsequent `Doom()` call will be queued and not run synchronously.
  backend->CalculateSizeOfAllEntries(base::DoNothing());

  // 3. Call `entry->Doom()`. This queues a `HandleDoomActiveEntryOperation`
  //    task in the `ExclusiveOperationCoordinator`. The task's callback
  //    captures a `scoped_refptr` to the `SqlEntryImpl`.
  entry->Doom();

  // 4. Close the entry. The last owning `scoped_refptr` is now held by the
  //    pending `Doom` operation inside the coordinator. The `active_entries_`
  //    map only holds a non-owning `raw_ref`.
  entry->Close();
  entry = nullptr;

  // 5. Destroy the backend. This triggers the destruction sequence that could
  //    cause the bug if member declaration order is incorrect.
  //    a. `weak_factory_` is destroyed, invalidating the entry's `backend_`
  //       WeakPtr.
  //    b. `exclusive_operation_coordinator_` is destroyed, which destroys the
  //       pending `Doom` task. This releases the last `scoped_refptr`.
  //    c. `~SqlEntryImpl()` is called.
  //    d. Inside `~SqlEntryImpl()`, the `if (!backend_)` check now passes,
  //       causing `ReleaseActiveEntry()` to be skipped.
  //    e. `active_entries_` is destroyed, but it still contains a `raw_ref` to
  //       the now-deleted entry, causing a dangling pointer issue.
  // If the bug exists, this test will crash here.
  auto task_runners = backend->GetBackgroundTaskRunnersForTest();
  backend.reset();

  // 6. If the bug is fixed, destruction completes safely. Run any remaining
  //    tasks to ensure clean shutdown and prevent leaks.
  FlushQueueInTaskRunners(task_runners);
}

TEST_F(SqlBackendImplTest, DoomEntryWithInMemoryIndex) {
  auto backend = CreateBackendAndInit();
  const std::string kKey = "my-key";
  const CacheEntryKey kEntryKey(kKey);

  // 1. Create an entry and close it.
  TestEntryResultCompletionCallback create_cb;
  disk_cache::EntryResult create_result = create_cb.GetResult(
      backend->CreateEntry(kKey, net::HIGHEST, create_cb.callback()));
  ASSERT_THAT(create_result.net_error(), IsOk());
  create_result.ReleaseEntry()->Close();

  // 2. Load in-memory index.
  ASSERT_TRUE(LoadInMemoryIndex(*backend));

  // 3. Verify that the entry is in the index.
  EXPECT_EQ(
      backend->GetSqlStoreForTest()->GetIndexStateForHash(kEntryKey.hash()),
      SqlPersistentStore::IndexState::kHashFound);

  // 4. Doom the entry.
  net::TestCompletionCallback cb_doom;
  int rv_doom = backend->DoomEntry(kKey, net::HIGHEST, cb_doom.callback());

  EXPECT_THAT(cb_doom.GetResult(rv_doom), IsOk());

  // 5. Verify that the entry is removed from the in-memory index.
  EXPECT_EQ(
      backend->GetSqlStoreForTest()->GetIndexStateForHash(kEntryKey.hash()),
      SqlPersistentStore::IndexState::kHashNotFound);

  // 6. Verify that the entry is gone.
  TestEntryResultCompletionCallback cb_open;
  disk_cache::EntryResult open_result = cb_open.GetResult(
      backend->OpenEntry(kKey, net::HIGHEST, cb_open.callback()));
  EXPECT_THAT(open_result.net_error(), IsError(net::ERR_FAILED));
}

// Tests that dooming a non-existent key whose hash collides with an existing
// entry's key does not affect the existing entry. The in-memory index is keyed
// by hash only, so a single-entry hash bucket may resolve to a different key's
// `res_id`; the backend must still keep the existing entry openable and avoid
// creating duplicate rows for it.
TEST_F(SqlBackendImplTest, DoomEntryWithInMemoryIndexHashCollision) {
  // Two distinct keys with the same `CacheEntryKey::hash()`.
  const std::string kExistingKey = "colliding-key-2018";
  const std::string kCollidingKey = "colliding-key-3000";
  const CacheEntryKey kExistingEntryKey(kExistingKey);
  ASSERT_EQ(kExistingEntryKey.hash(), CacheEntryKey(kCollidingKey).hash());

  auto backend = CreateBackendAndInit();

  // 1. Create an entry for `kExistingKey` and close it so it is no longer
  //    active.
  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result = cb_create.GetResult(
      backend->CreateEntry(kExistingKey, net::HIGHEST, cb_create.callback()));
  ASSERT_THAT(create_result.net_error(), IsOk());
  create_result.ReleaseEntry()->Close();
  backend->RunUntilAllTasksCompleteForTest();

  // 2. Load the in-memory index. `kExistingKey` is the sole occupant of its
  //    hash bucket.
  ASSERT_TRUE(LoadInMemoryIndex(*backend));
  ASSERT_EQ(backend->GetSqlStoreForTest()->GetIndexStateForHash(
                kExistingEntryKey.hash()),
            SqlPersistentStore::IndexState::kHashFound);

  // 3. Doom `kCollidingKey`, which does not exist but shares its hash with
  //    `kExistingKey`.
  net::TestCompletionCallback cb_doom;
  EXPECT_THAT(cb_doom.GetResult(backend->DoomEntry(kCollidingKey, net::HIGHEST,
                                                   cb_doom.callback())),
              IsOk());
  backend->RunUntilAllTasksCompleteForTest();

  // 4. The existing entry must remain in the in-memory index.
  EXPECT_EQ(backend->GetSqlStoreForTest()->GetIndexStateForHash(
                kExistingEntryKey.hash()),
            SqlPersistentStore::IndexState::kHashFound);

  // 5. Opening `kExistingKey` must still succeed.
  TestEntryResultCompletionCallback cb_open;
  disk_cache::EntryResult open_result = cb_open.GetResult(
      backend->OpenEntry(kExistingKey, net::HIGHEST, cb_open.callback()));
  ASSERT_THAT(open_result.net_error(), IsOk());
  open_result.ReleaseEntry()->Close();

  // 6. OpenOrCreateEntry must open the existing entry rather than creating a
  //    duplicate row.
  TestEntryResultCompletionCallback cb_ooc;
  disk_cache::EntryResult ooc_result =
      cb_ooc.GetResult(backend->OpenOrCreateEntry(kExistingKey, net::HIGHEST,
                                                  cb_ooc.callback()));
  ASSERT_THAT(ooc_result.net_error(), IsOk());
  EXPECT_TRUE(ooc_result.opened());
  ooc_result.ReleaseEntry()->Close();
  backend->RunUntilAllTasksCompleteForTest();

  base::test::TestFuture<int32_t> count_future;
  EXPECT_EQ(backend->GetEntryCount(count_future.GetCallback()),
            base::unexpected(net::ERR_IO_PENDING));
  EXPECT_EQ(count_future.Get(), 1);
}

TEST_F(SqlBackendImplTest, SetDataHintsAndDoomAndWriteOptimistically) {
  auto backend = CreateBackendAndInit();
  const std::string kKey = "my-key";
  const uint8_t kUnusableHint = 1;

  // 1. Create an entry.
  TestEntryResultCompletionCallback cb_create;
  EntryResult create_result = cb_create.GetResult(
      backend->CreateEntry(kKey, net::HIGHEST, cb_create.callback()));
  ASSERT_THAT(create_result.net_error(), IsOk());
  auto* entry = create_result.ReleaseEntry();

  // 2. Set an in-memory hint.
  entry->SetEntryInMemoryData(kUnusableHint);
  EXPECT_EQ(backend->GetEntryInMemoryData(kKey), kUnusableHint);
  entry->Close();

  // 3. While write is in flight, it should still be returned from
  //    `in_flight_entry_modifications_`.
  EXPECT_EQ(backend->GetEntryInMemoryData(kKey), kUnusableHint);

  // 4. Call OnBrowserIdle() to trigger in-memory index loading.
  backend->OnBrowserIdle();
  backend->RunUntilAllTasksCompleteForTest();

  // 5. Verify the hint is set in the backend.
  EXPECT_EQ(backend->GetEntryInMemoryData(kKey), kUnusableHint);

  // 6. Doom the entry.
  base::test::TestFuture<int> doom_future;
  int doom_rv =
      backend->DoomEntry(kKey, net::HIGHEST, doom_future.GetCallback());
  EXPECT_EQ(doom_rv, net::ERR_IO_PENDING);

  // 7. OpenOrCreateEntry should create a new entry.
  TestEntryResultCompletionCallback cb_open_or_create;
  EntryResult open_or_create_result =
      cb_open_or_create.GetResult(backend->OpenOrCreateEntry(
          kKey, net::HIGHEST, cb_open_or_create.callback()));
  ASSERT_THAT(open_or_create_result.net_error(), IsOk());
  EXPECT_FALSE(open_or_create_result.opened());

  open_or_create_result.ReleaseEntry()->Close();
  EXPECT_EQ(doom_future.Get(), net::OK);
}

TEST_F(SqlBackendImplTest, SetEntryDataHintsWithSpeculativeCreateEntryFailure) {
  AddScopedFeatureList().InitAndEnableFeatureWithParameters(
      net::features::kDiskCacheBackendExperiment,
      {{net::features::kSqlDiskCacheMaxWriteBufferTotalSize.name, "10"}});
  auto backend = CreateBackendAndInit();
  EXPECT_TRUE(LoadInMemoryIndex(*backend));
  backend->GetSqlStoreForTest()->SetSimulateDbFailureForTesting(true);
  const std::string kKey = "my-key";

  // Create an entry. This should return immediately with a speculatively
  // created entry.
  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result =
      backend->CreateEntry(kKey, net::HIGHEST, cb_create.callback());
  ASSERT_THAT(create_result.net_error(), IsOk());
  auto* entry = create_result.ReleaseEntry();
  ASSERT_TRUE(entry);
  auto db_handle = static_cast<SqlEntryImpl*>(entry)->db_handle();

  // When WriteData is called with data larger than
  // kSqlDiskCacheMaxWriteBufferTotalSize, SqlBackendImpl starts the optimistic
  // write process, and the `db_handle` enters the creating state.
  auto write_buffer = base::MakeRefCounted<net::StringIOBuffer>("1234567890A");
  ASSERT_GT(write_buffer->size(),
            net::features::kSqlDiskCacheMaxWriteBufferTotalSize.Get());
  EXPECT_TRUE(db_handle->IsInitialState());
  EXPECT_EQ(entry->WriteData(1, 0, write_buffer.get(), write_buffer->size(),
                             base::DoNothing(), false),
            write_buffer->size());
  EXPECT_TRUE(db_handle->IsCreatingState());
  // After flushing all DB tasks, the db_handle should hold a kFailedForTesting
  // error.
  backend->RunUntilAllTasksCompleteForTest();
  EXPECT_THAT(db_handle->GetError(),
              SqlPersistentStore::Error::kFailedForTesting);

  // Set an in-memory hint. This should fail silently because the entry has an
  // error.
  const uint8_t kUnusableHint = 1;
  entry->SetEntryInMemoryData(kUnusableHint);
  entry->Close();

  // Flush the queue to make sure the SetEntryInMemoryData operation is
  // processed.
  backend->RunUntilAllTasksCompleteForTest();

  // Verify the hint is not set in the backend.
  EXPECT_EQ(backend->GetEntryInMemoryData(kKey), 0);
}

// Regression test for https://crbug.com/473912285.
// Tests that an optimistic write failure does not cause an index mismatch error
// (which can lead to a CHECK failure in strict mode) if the entry has already
// been doomed by DoomAllEntries.
TEST_F(SqlBackendImplTest, OptimisticWriteIndexMismatchAfterDoomAllEntries) {
  auto backend = CreateBackendAndInit();
  EXPECT_TRUE(LoadInMemoryIndex(*backend));
  backend->EnableStrictCorruptionCheckForTesting();

  // 1. Create a speculative entry.
  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result =
      backend->CreateEntry("key", net::HIGHEST, cb_create.callback());
  ASSERT_THAT(create_result.net_error(), IsOk());
  auto* entry = create_result.ReleaseEntry();
  ASSERT_TRUE(entry);

  // 2. Doom all entries.
  net::TestCompletionCallback doom_cb;
  int rv_doom = backend->DoomAllEntries(doom_cb.callback());
  EXPECT_EQ(rv_doom, net::ERR_IO_PENDING);
  EXPECT_THAT(doom_cb.WaitForResult(), IsOk());

  // 3. Set DB failure to force OptimisticWrite to fail.
  backend->GetSqlStoreForTest()->SetSimulateDbFailureForTesting(true);

  // 4. Write data optimistically.
  auto buffer = base::MakeRefCounted<net::StringIOBuffer>("data");
  int rv_write =
      entry->WriteData(1, 100, buffer.get(), 4, base::DoNothing(), false);
  EXPECT_EQ(rv_write, 4);

  // 4. Wait for operations to complete.
  // Previously, this would trigger an index mismatch error (and a CHECK failure
  // in RecordIndexMismatch).
  backend->RunUntilAllTasksCompleteForTest();

  entry->Close();
}

TEST_F(SqlBackendImplTest, WriteBuffering) {
  AddScopedFeatureList().InitAndEnableFeatureWithParameters(
      net::features::kDiskCacheBackendExperiment,
      {{net::features::kSqlDiskCacheMaxWriteBufferTotalSize.name, "10240"},
       {net::features::kSqlDiskCacheMaxWriteBufferSizePerEntry.name, "1024"},
       {net::features::kSqlDiskCacheOptimisticWriteBufferSize.name, "250"}});

  auto backend = CreateBackendAndInit();

  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result = cb_create.GetResult(
      backend->CreateEntry("key", net::HIGHEST, cb_create.callback()));
  ASSERT_THAT(create_result.net_error(), IsOk());
  auto* entry = create_result.ReleaseEntry();

  // Write small chunk, should be buffered.
  auto buffer1 =
      base::MakeRefCounted<net::StringIOBuffer>(std::string(100, 'a'));
  EXPECT_EQ(entry->WriteData(1, 0, buffer1.get(), buffer1->size(),
                             base::DoNothing(), false),
            buffer1->size());
  EXPECT_EQ(backend->GetWriteBufferTotalSizeForTesting(), buffer1->size());

  // Write another small chunk, should be buffered.
  auto buffer2 =
      base::MakeRefCounted<net::StringIOBuffer>(std::string(100, 'b'));
  EXPECT_EQ(entry->WriteData(1, 100, buffer2.get(), buffer2->size(),
                             base::DoNothing(), false),
            buffer2->size());
  EXPECT_EQ(backend->GetWriteBufferTotalSizeForTesting(),
            buffer1->size() + buffer2->size());
  EXPECT_EQ(backend->GetOptimisticWriteBufferTotalSizeForTesting(), 0);

  // Write exceeding per-entry limit, should trigger flush of previous buffer.
  // The new data is too large to buffer, so it should be written directly.
  std::string large_data(2000, 'c');
  auto buffer3 = base::MakeRefCounted<net::StringIOBuffer>(large_data);
  net::TestCompletionCallback cb_write;
  int rv = entry->WriteData(1, 200, buffer3.get(), buffer3->size(),
                            cb_write.callback(), false);
  EXPECT_EQ(rv, net::ERR_IO_PENDING);
  EXPECT_EQ(backend->GetWriteBufferTotalSizeForTesting(), 0);
  EXPECT_EQ(backend->GetOptimisticWriteBufferTotalSizeForTesting(),
            buffer1->size() + buffer2->size());

  entry->Close();

  backend->RunUntilAllTasksCompleteForTest();
  EXPECT_EQ(backend->GetWriteBufferTotalSizeForTesting(), 0);
  EXPECT_EQ(backend->GetOptimisticWriteBufferTotalSizeForTesting(), 0);
}

TEST_F(SqlBackendImplTest, WriteBufferingReadFromBuffer) {
  AddScopedFeatureList().InitAndEnableFeatureWithParameters(
      net::features::kDiskCacheBackendExperiment,
      {{net::features::kSqlDiskCacheMaxWriteBufferTotalSize.name, "10240"},
       {net::features::kSqlDiskCacheMaxWriteBufferSizePerEntry.name, "1024"}});

  auto backend = CreateBackendAndInit();

  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result = cb_create.GetResult(
      backend->CreateEntry("key", net::HIGHEST, cb_create.callback()));
  ASSERT_THAT(create_result.net_error(), IsOk());
  auto* entry = create_result.ReleaseEntry();

  // Buffer some data.
  std::string data = "hello world";
  auto buffer = base::MakeRefCounted<net::StringIOBuffer>(data);
  entry->WriteData(1, 0, buffer.get(), buffer->size(), base::DoNothing(),
                   false);
  EXPECT_EQ(backend->GetWriteBufferTotalSizeForTesting(), data.size());

  // Read it back.
  auto read_buffer = base::MakeRefCounted<net::IOBufferWithSize>(data.size());
  net::TestCompletionCallback cb_read;
  int rv_read = entry->ReadData(1, 0, read_buffer.get(), read_buffer->size(),
                                cb_read.callback());
  // Should be synchronous
  EXPECT_EQ(rv_read, static_cast<int>(data.size()));
  EXPECT_EQ(std::string_view(read_buffer->data(), data.size()), data);

  // Buffer should still be there.
  EXPECT_EQ(backend->GetWriteBufferTotalSizeForTesting(), data.size());

  entry->Close();

  backend->RunUntilAllTasksCompleteForTest();
  EXPECT_EQ(backend->GetWriteBufferTotalSizeForTesting(), 0);
}

TEST_F(SqlBackendImplTest, WriteBufferingReadOverlapFlush) {
  AddScopedFeatureList().InitAndEnableFeatureWithParameters(
      net::features::kDiskCacheBackendExperiment,
      {{net::features::kSqlDiskCacheMaxWriteBufferTotalSize.name, "10240"},
       {net::features::kSqlDiskCacheMaxWriteBufferSizePerEntry.name, "1024"}});

  auto backend = CreateBackendAndInit();

  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result = cb_create.GetResult(
      backend->CreateEntry("key", net::HIGHEST, cb_create.callback()));
  ASSERT_THAT(create_result.net_error(), IsOk());
  auto* entry = create_result.ReleaseEntry();

  // Buffer some data.
  std::string data = "hello world";
  auto buffer = base::MakeRefCounted<net::StringIOBuffer>(data);
  // Write at 0 to ensure buffering (sequential).
  entry->WriteData(1, 0, buffer.get(), buffer->size(), base::DoNothing(),
                   false);
  EXPECT_EQ(backend->GetWriteBufferTotalSizeForTesting(), data.size());

  // Read range that overlaps but is larger than buffer (e.g. from 0 to 20)
  // This should force flush.
  auto read_buffer = base::MakeRefCounted<net::IOBufferWithSize>(20);
  net::TestCompletionCallback cb_read;
  int rv_read = entry->ReadData(1, 0, read_buffer.get(), read_buffer->size(),
                                cb_read.callback());
  // Should return data.size() (11).
  EXPECT_EQ(cb_read.GetResult(rv_read), static_cast<int>(data.size()));
  EXPECT_EQ(std::string_view(read_buffer->data(), data.size()), data);

  // Buffer should be flushed.
  EXPECT_EQ(backend->GetWriteBufferTotalSizeForTesting(), 0);

  entry->Close();
}

TEST_F(SqlBackendImplTest, WriteBufferingGlobalLimit) {
  AddScopedFeatureList().InitAndEnableFeatureWithParameters(
      net::features::kDiskCacheBackendExperiment,
      {{net::features::kSqlDiskCacheMaxWriteBufferTotalSize.name, "100"},
       {net::features::kSqlDiskCacheMaxWriteBufferSizePerEntry.name, "1000"}});

  auto backend = CreateBackendAndInit();

  // Create entry 1
  TestEntryResultCompletionCallback cb_create1;
  disk_cache::EntryResult create_result1 = cb_create1.GetResult(
      backend->CreateEntry("key1", net::HIGHEST, cb_create1.callback()));
  ASSERT_THAT(create_result1.net_error(), IsOk());
  auto* entry1 = create_result1.ReleaseEntry();

  // Create entry 2
  TestEntryResultCompletionCallback cb_create2;
  disk_cache::EntryResult create_result2 = cb_create2.GetResult(
      backend->CreateEntry("key2", net::HIGHEST, cb_create2.callback()));
  ASSERT_THAT(create_result2.net_error(), IsOk());
  auto* entry2 = create_result2.ReleaseEntry();

  // Write 60 bytes to entry 1. Buffered.
  auto buffer1 =
      base::MakeRefCounted<net::StringIOBuffer>(std::string(60, 'a'));
  entry1->WriteData(1, 0, buffer1.get(), buffer1->size(), base::DoNothing(),
                    false);
  EXPECT_EQ(backend->GetWriteBufferTotalSizeForTesting(), 60);

  // Write 60 bytes to entry 2. Should flush entry 2 immediately because global
  // limit (100) would be exceeded (60+60=120).
  auto buffer2 =
      base::MakeRefCounted<net::StringIOBuffer>(std::string(60, 'b'));
  net::TestCompletionCallback cb_write;
  int rv = entry2->WriteData(1, 0, buffer2.get(), buffer2->size(),
                             cb_write.callback(), false);
  EXPECT_EQ(cb_write.GetResult(rv), 60);

  EXPECT_EQ(backend->GetWriteBufferTotalSizeForTesting(),
            60);  // Entry 1 still buffered.

  entry1->Close();
  entry2->Close();

  EXPECT_EQ(backend->GetWriteBufferTotalSizeForTesting(), 60);
  backend->RunUntilAllTasksCompleteForTest();
  EXPECT_EQ(backend->GetWriteBufferTotalSizeForTesting(), 0);
}

TEST_F(SqlBackendImplTest, WriteBufferingFlushOnClose) {
  AddScopedFeatureList().InitAndEnableFeatureWithParameters(
      net::features::kDiskCacheBackendExperiment,
      {{net::features::kSqlDiskCacheMaxWriteBufferTotalSize.name, "10240"},
       {net::features::kSqlDiskCacheMaxWriteBufferSizePerEntry.name, "1024"}});

  auto backend = CreateBackendAndInit();

  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result = cb_create.GetResult(
      backend->CreateEntry("key", net::HIGHEST, cb_create.callback()));
  ASSERT_THAT(create_result.net_error(), IsOk());
  auto* entry = create_result.ReleaseEntry();

  // Buffer some data.
  std::string data = "hello world";
  auto buffer = base::MakeRefCounted<net::StringIOBuffer>(data);
  entry->WriteData(1, 0, buffer.get(), buffer->size(), base::DoNothing(),
                   false);
  EXPECT_EQ(backend->GetWriteBufferTotalSizeForTesting(), data.size());

  entry->Close();

  // Closing should asynchronously flush buffer.
  EXPECT_EQ(backend->GetWriteBufferTotalSizeForTesting(), data.size());
  backend->RunUntilAllTasksCompleteForTest();
  EXPECT_EQ(backend->GetWriteBufferTotalSizeForTesting(), 0);

  // Verify data on disk by opening again.
  TestEntryResultCompletionCallback cb_open;
  disk_cache::EntryResult open_result = cb_open.GetResult(
      backend->OpenEntry("key", net::HIGHEST, cb_open.callback()));
  ASSERT_THAT(open_result.net_error(), IsOk());
  entry = open_result.ReleaseEntry();

  auto read_buffer = base::MakeRefCounted<net::IOBufferWithSize>(data.size());
  net::TestCompletionCallback cb_read;
  int rv_read = entry->ReadData(1, 0, read_buffer.get(), read_buffer->size(),
                                cb_read.callback());
  EXPECT_EQ(cb_read.GetResult(rv_read), static_cast<int>(data.size()));
  EXPECT_EQ(std::string_view(read_buffer->data(), data.size()), data);

  entry->Close();
}

TEST_F(SqlBackendImplTest, WriteBufferingOptimisticBoundary) {
  AddScopedFeatureList().InitAndEnableFeatureWithParameters(
      net::features::kDiskCacheBackendExperiment,
      {{net::features::kSqlDiskCacheMaxWriteBufferTotalSize.name, "10240"},
       {net::features::kSqlDiskCacheMaxWriteBufferSizePerEntry.name, "1000"},
       {net::features::kSqlDiskCacheOptimisticWriteBufferSize.name, "3000"}});

  auto backend = CreateBackendAndInit();

  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result = cb_create.GetResult(
      backend->CreateEntry("key", net::HIGHEST, cb_create.callback()));
  ASSERT_THAT(create_result.net_error(), IsOk());
  auto* entry = create_result.ReleaseEntry();

  // 1. Write 500 bytes. Should be buffered.
  auto buffer1 =
      base::MakeRefCounted<net::StringIOBuffer>(std::string(500, 'a'));
  EXPECT_EQ(entry->WriteData(1, 0, buffer1.get(), buffer1->size(),
                             base::DoNothing(), false),
            buffer1->size());
  EXPECT_EQ(backend->GetWriteBufferTotalSizeForTesting(), 500);
  // Optimistic buffer usage: 0 (buffered in entry, not sent to backend)
  EXPECT_EQ(backend->GetOptimisticWriteBufferTotalSizeForTesting(), 0);

  // 2. Write 600 bytes. 500 + 600 > 1000.
  // Should flush buffer (500) -> Optimistic (size 500).
  // Then write new data (600) to the new buffer.
  auto buffer2 =
      base::MakeRefCounted<net::StringIOBuffer>(std::string(600, 'b'));
  EXPECT_EQ(entry->WriteData(1, 500, buffer2.get(), buffer2->size(),
                             base::DoNothing(), false),
            buffer2->size());
  EXPECT_EQ(backend->GetWriteBufferTotalSizeForTesting(), 600);
  EXPECT_EQ(backend->GetOptimisticWriteBufferTotalSizeForTesting(), 500);

  // 3. Write 2000 bytes. 2000 > 1000.
  // Since 2000 > 1000 (entry limit), it CANNOT be buffered.
  // It flushes the previous buffer (600) -> Optimistic: 500 + 600 = 1100 <=
  // 3000. Then writes 2000 bytes directly (async).
  auto buffer3 =
      base::MakeRefCounted<net::StringIOBuffer>(std::string(2000, 'c'));
  net::TestCompletionCallback cb_write3;
  EXPECT_EQ(entry->WriteData(1, 1100, buffer3.get(), buffer3->size(),
                             cb_write3.callback(), false),
            net::ERR_IO_PENDING);
  EXPECT_EQ(backend->GetWriteBufferTotalSizeForTesting(), 0);
  EXPECT_EQ(backend->GetOptimisticWriteBufferTotalSizeForTesting(), 1100);
  EXPECT_EQ(cb_write3.WaitForResult(), 2000);
  EXPECT_EQ(backend->GetOptimisticWriteBufferTotalSizeForTesting(), 0);

  // 4. Write 9000 bytes.
  // Total buffer usage: 0 + 9000.
  // Cannot buffer because 9000 > 1000 (entry limit).
  // Writes 9000 bytes directly.
  // Optimistic check for new write: 0 + 9000 > 3000. Non-optimistic write.
  // Should be pending.
  auto buffer4 =
      base::MakeRefCounted<net::StringIOBuffer>(std::string(9000, 'd'));
  net::TestCompletionCallback cb_write4;
  EXPECT_EQ(entry->WriteData(1, 3100, buffer4.get(), buffer4->size(),
                             cb_write4.callback(), false),
            net::ERR_IO_PENDING);
  EXPECT_EQ(cb_write4.WaitForResult(), static_cast<int>(buffer4->size()));
  EXPECT_EQ(backend->GetWriteBufferTotalSizeForTesting(), 0);
  EXPECT_EQ(backend->GetOptimisticWriteBufferTotalSizeForTesting(), 0);

  entry->Close();
}

TEST_F(SqlBackendImplTest, WriteBufferingReadAcrossChunks) {
  AddScopedFeatureList().InitAndEnableFeatureWithParameters(
      net::features::kDiskCacheBackendExperiment,
      {{net::features::kSqlDiskCacheMaxWriteBufferTotalSize.name, "10240"},
       {net::features::kSqlDiskCacheMaxWriteBufferSizePerEntry.name, "1024"}});

  auto backend = CreateBackendAndInit();

  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result = cb_create.GetResult(
      backend->CreateEntry("key", net::HIGHEST, cb_create.callback()));
  ASSERT_THAT(create_result.net_error(), IsOk());
  auto* entry = create_result.ReleaseEntry();

  // Write chunk 1: "AAAAA"
  std::string data1 = "AAAAA";
  auto buffer1 = base::MakeRefCounted<net::StringIOBuffer>(data1);
  entry->WriteData(1, 0, buffer1.get(), buffer1->size(), base::DoNothing(),
                   false);

  // Write chunk 2: "BBBBB"
  std::string data2 = "BBBBB";
  auto buffer2 = base::MakeRefCounted<net::StringIOBuffer>(data2);
  entry->WriteData(1, 5, buffer2.get(), buffer2->size(), base::DoNothing(),
                   false);

  EXPECT_EQ(backend->GetWriteBufferTotalSizeForTesting(),
            data1.size() + data2.size());

  // Read across chunks: Offset 3, Length 4. Should get "AABB".
  auto read_buffer = base::MakeRefCounted<net::IOBufferWithSize>(4);
  net::TestCompletionCallback cb_read;
  int rv_read = entry->ReadData(1, 3, read_buffer.get(), read_buffer->size(),
                                cb_read.callback());
  EXPECT_EQ(cb_read.GetResult(rv_read), 4);
  EXPECT_EQ(std::string_view(read_buffer->data(), 4), "AABB");

  // Buffer should still be there.
  EXPECT_EQ(backend->GetWriteBufferTotalSizeForTesting(),
            data1.size() + data2.size());

  entry->Close();

  backend->RunUntilAllTasksCompleteForTest();
  EXPECT_EQ(backend->GetWriteBufferTotalSizeForTesting(), 0);
}

TEST_F(SqlBackendImplTest, CombinedWriteAndMetadataUpdate) {
  auto backend = CreateBackendAndInit();

  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result = cb_create.GetResult(
      backend->CreateEntry("key", net::HIGHEST, cb_create.callback()));
  ASSERT_THAT(create_result.net_error(), IsOk());
  auto* entry = create_result.ReleaseEntry();

  // Write to stream 0 (header) - this will be buffered in SqlEntryImpl.
  std::string header_data = "header";
  auto header_buffer = base::MakeRefCounted<net::StringIOBuffer>(header_data);
  entry->WriteData(0, 0, header_buffer.get(), header_buffer->size(),
                   base::DoNothing(), false);

  // Write to stream 1 (body) - this will be buffered in write buffer.
  std::string body_data = "body";
  auto body_buffer = base::MakeRefCounted<net::StringIOBuffer>(body_data);
  entry->WriteData(1, 0, body_buffer.get(), body_buffer->size(),
                   base::DoNothing(), false);

  // Close the entry. This should trigger a single WriteEntryDataAndMetadata
  // call that persists both header and body.
  entry->Close();

  // Flush background tasks.
  backend->RunUntilAllTasksCompleteForTest();

  // Re-open and verify.
  TestEntryResultCompletionCallback cb_open;
  disk_cache::EntryResult open_result = cb_open.GetResult(
      backend->OpenEntry("key", net::HIGHEST, cb_open.callback()));
  ASSERT_THAT(open_result.net_error(), IsOk());
  entry = open_result.ReleaseEntry();

  // Check header.
  auto read_header_buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(header_data.size());
  net::TestCompletionCallback cb_read_header;
  int rv_read_header =
      entry->ReadData(0, 0, read_header_buffer.get(),
                      read_header_buffer->size(), cb_read_header.callback());
  EXPECT_EQ(cb_read_header.GetResult(rv_read_header),
            static_cast<int>(header_data.size()));
  EXPECT_EQ(std::string_view(read_header_buffer->data(), header_data.size()),
            header_data);

  // Check body.
  auto read_body_buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(body_data.size());
  net::TestCompletionCallback cb_read_body;
  int rv_read_body =
      entry->ReadData(1, 0, read_body_buffer.get(), read_body_buffer->size(),
                      cb_read_body.callback());
  EXPECT_EQ(cb_read_body.GetResult(rv_read_body),
            static_cast<int>(body_data.size()));
  EXPECT_EQ(std::string_view(read_body_buffer->data(), body_data.size()),
            body_data);

  entry->Close();
}

TEST_F(SqlBackendImplTest, ReadCaching) {
  auto backend = CreateBackendAndInit();

  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result = cb_create.GetResult(
      backend->CreateEntry("key", net::HIGHEST, cb_create.callback()));
  ASSERT_THAT(create_result.net_error(), IsOk());
  auto* entry = create_result.ReleaseEntry();

  // 1. Write some data to stream 1.
  std::string data = "0123456789ABCDEF";
  auto buffer = base::MakeRefCounted<net::StringIOBuffer>(data);
  net::TestCompletionCallback cb_write;
  int rv_write = entry->WriteData(1, 0, buffer.get(), buffer->size(),
                                  cb_write.callback(), false);
  EXPECT_EQ(cb_write.GetResult(rv_write), static_cast<int>(data.size()));

  // Close and re-open the entry to ensure data is written to the DB and we
  // don't read from the write buffer.
  entry->Close();
  TestEntryResultCompletionCallback cb_open;
  disk_cache::EntryResult open_result = cb_open.GetResult(
      backend->OpenEntry("key", net::HIGHEST, cb_open.callback()));
  ASSERT_THAT(open_result.net_error(), IsOk());
  entry = open_result.ReleaseEntry();

  // 2. Read only the first 5 bytes.
  // The backend should read the whole blob (since it's a single blob in DB)
  // and cache the remaining 11 bytes.
  auto read_buffer1 = base::MakeRefCounted<net::IOBufferWithSize>(5);
  net::TestCompletionCallback cb_read1;
  int rv_read1 =
      entry->ReadData(1, 0, read_buffer1.get(), 5, cb_read1.callback());
  EXPECT_EQ(cb_read1.GetResult(rv_read1), 5);
  EXPECT_EQ(std::string_view(read_buffer1->data(), 5), "01234");

  // Verify that the read_cache_buffer is populated.
  EXPECT_TRUE(static_cast<SqlEntryImpl*>(entry)->read_cache_buffer_for_test());
  EXPECT_EQ(
      static_cast<SqlEntryImpl*>(entry)->read_cache_buffer_offset_for_test(),
      5);

  // 3. Read the next 5 bytes.
  // This should be fulfilled from the cache synchronously (no IO pending).
  auto read_buffer2 = base::MakeRefCounted<net::IOBufferWithSize>(5);
  int rv_read2 =
      entry->ReadData(1, 5, read_buffer2.get(), 5, base::DoNothing());
  // If it's cached, it returns immediately.
  EXPECT_EQ(rv_read2, 5);
  EXPECT_EQ(std::string_view(read_buffer2->data(), 5), "56789");

  // 4. Read crossing the cache boundary.
  // The cache has "56789ABCDEF" (offset 5 to 16).
  // Request 5 bytes from offset 14: "EF" + 3 more.
  // It should return 2 bytes synchronously if it uses partial cache,
  // or return the whole thing if it triggers a new read.
  // Current implementation returns copy_size = min(buf_len, cache_end -
  // offset). So it should return 2 bytes synchronously.
  auto read_buffer3 = base::MakeRefCounted<net::IOBufferWithSize>(5);
  int rv_read3 =
      entry->ReadData(1, 14, read_buffer3.get(), 5, base::DoNothing());
  EXPECT_EQ(rv_read3, 2);
  EXPECT_EQ(std::string_view(read_buffer3->data(), 2), "EF");

  // 5. Write data should invalidate read cache.
  entry->WriteData(1, 16, buffer.get(), 1, base::DoNothing(), false);
  auto read_buffer4 = base::MakeRefCounted<net::IOBufferWithSize>(5);
  int rv_read4 =
      entry->ReadData(1, 5, read_buffer4.get(), 5, base::DoNothing());
  // Now it should be pending because cache was invalidated.
  EXPECT_EQ(rv_read4, net::ERR_IO_PENDING);

  entry->Close();
}

TEST_F(SqlBackendImplTest, ReadCachingSparse) {
  auto backend = CreateBackendAndInit();

  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result = cb_create.GetResult(
      backend->CreateEntry("key", net::HIGHEST, cb_create.callback()));
  ASSERT_THAT(create_result.net_error(), IsOk());
  auto* entry = create_result.ReleaseEntry();

  // 1. Write some data to stream 1.
  std::string data = "0123456789ABCDEF";
  auto buffer = base::MakeRefCounted<net::StringIOBuffer>(data);
  net::TestCompletionCallback cb_write;
  int rv_write = entry->WriteData(1, 0, buffer.get(), buffer->size(),
                                  cb_write.callback(), false);
  EXPECT_EQ(cb_write.GetResult(rv_write), static_cast<int>(data.size()));

  // Close and re-open the entry to ensure data is written to the DB and we
  // don't read from the write buffer.
  entry->Close();
  TestEntryResultCompletionCallback cb_open;
  disk_cache::EntryResult open_result = cb_open.GetResult(
      backend->OpenEntry("key", net::HIGHEST, cb_open.callback()));
  ASSERT_THAT(open_result.net_error(), IsOk());
  entry = open_result.ReleaseEntry();

  // 2. Read sparse data.
  auto read_buffer1 = base::MakeRefCounted<net::IOBufferWithSize>(5);
  net::TestCompletionCallback cb_read1;
  int rv_read1 =
      entry->ReadSparseData(0, read_buffer1.get(), 5, cb_read1.callback());
  EXPECT_EQ(cb_read1.GetResult(rv_read1), 5);
  EXPECT_EQ(std::string_view(read_buffer1->data(), 5), "01234");

  // Verify that the read_cache_buffer IS populated even for sparse reads.
  EXPECT_TRUE(static_cast<SqlEntryImpl*>(entry)->read_cache_buffer_for_test());
  EXPECT_EQ(
      static_cast<SqlEntryImpl*>(entry)->read_cache_buffer_offset_for_test(),
      5);

  // Subsequent read (could be normal or sparse) should use the cache.
  auto read_buffer2 = base::MakeRefCounted<net::IOBufferWithSize>(5);
  int rv_read2 =
      entry->ReadData(1, 5, read_buffer2.get(), 5, base::DoNothing());
  EXPECT_EQ(rv_read2, 5);
  EXPECT_EQ(std::string_view(read_buffer2->data(), 5), "56789");

  entry->Close();
}

TEST_F(SqlBackendImplTest, ReadCachingGlobalLimit) {
  AddScopedFeatureList().InitAndEnableFeatureWithParameters(
      net::features::kDiskCacheBackendExperiment,
      {{net::features::kSqlDiskCacheMaxReadBufferTotalSize.name, "90"}});

  auto backend = CreateBackendAndInit();

  std::string data(60, 'a');
  auto buffer = base::MakeRefCounted<net::StringIOBuffer>(data);

  // Create entry 1
  TestEntryResultCompletionCallback cb_create1;
  disk_cache::EntryResult create_result1 = cb_create1.GetResult(
      backend->CreateEntry("key1", net::HIGHEST, cb_create1.callback()));
  ASSERT_THAT(create_result1.net_error(), IsOk());
  auto* entry1 = create_result1.ReleaseEntry();
  entry1->WriteData(1, 0, buffer.get(), buffer->size(), base::DoNothing(),
                    false);
  entry1->Close();

  // Create entry 2
  TestEntryResultCompletionCallback cb_create2;
  disk_cache::EntryResult create_result2 = cb_create2.GetResult(
      backend->CreateEntry("key2", net::HIGHEST, cb_create2.callback()));
  ASSERT_THAT(create_result2.net_error(), IsOk());
  auto* entry2 = create_result2.ReleaseEntry();
  entry2->WriteData(1, 0, buffer.get(), buffer->size(), base::DoNothing(),
                    false);
  entry2->Close();

  // Open both
  TestEntryResultCompletionCallback cb_open1;
  auto* entry1_open = cb_open1
                          .GetResult(backend->OpenEntry("key1", net::HIGHEST,
                                                        cb_open1.callback()))
                          .ReleaseEntry();
  TestEntryResultCompletionCallback cb_open2;
  auto* entry2_open = cb_open2
                          .GetResult(backend->OpenEntry("key2", net::HIGHEST,
                                                        cb_open2.callback()))
                          .ReleaseEntry();

  // Read from entry 1. 10 bytes. 50 bytes cached. Total 50.
  auto read_buf = base::MakeRefCounted<net::IOBufferWithSize>(10);
  net::TestCompletionCallback cb_read1;
  int rv1 =
      entry1_open->ReadData(1, 0, read_buf.get(), 10, cb_read1.callback());
  EXPECT_EQ(cb_read1.GetResult(rv1), 10);
  EXPECT_TRUE(
      static_cast<SqlEntryImpl*>(entry1_open)->read_cache_buffer_for_test());

  // Read from entry 2. 10 bytes. 50 bytes to cache. Total would be 100 > 90.
  // Should NOT be cached.
  net::TestCompletionCallback cb_read2;
  int rv2 =
      entry2_open->ReadData(1, 0, read_buf.get(), 10, cb_read2.callback());
  EXPECT_EQ(cb_read2.GetResult(rv2), 10);
  EXPECT_FALSE(
      static_cast<SqlEntryImpl*>(entry2_open)->read_cache_buffer_for_test());

  entry1_open->Close();  // Releases 50 bytes. Total 0.

  // Read again from entry 2 (offset 10).
  // This will trigger a new read from DB. Since total is 0, the remaining 40
  // bytes (60-10-10) should be cached?
  net::TestCompletionCallback cb_read3;
  int rv3 =
      entry2_open->ReadData(1, 10, read_buf.get(), 10, cb_read3.callback());
  EXPECT_EQ(cb_read3.GetResult(rv3), 10);
  EXPECT_TRUE(
      static_cast<SqlEntryImpl*>(entry2_open)->read_cache_buffer_for_test());

  entry2_open->Close();
}

TEST_F(SqlBackendImplTest, GetAvailableRangeWithBufferedWrite) {
  AddScopedFeatureList().InitAndEnableFeatureWithParameters(
      net::features::kDiskCacheBackendExperiment,
      {{net::features::kSqlDiskCacheOptimisticWriteBufferSize.name, "1024"},
       {net::features::kSqlDiskCacheMaxWriteBufferTotalSize.name, "1024"}});

  auto backend = CreateBackendAndInit();
  const std::string kKey = "my-key";
  const std::string kData = "some data";

  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result = cb_create.GetResult(
      backend->CreateEntry(kKey, net::HIGHEST, cb_create.callback()));
  ASSERT_THAT(create_result.net_error(), IsOk());
  auto* entry = create_result.ReleaseEntry();
  ASSERT_TRUE(entry);

  // Write data. This should be buffered because it's small and write buffering
  // is enabled.
  auto write_buffer = base::MakeRefCounted<net::StringIOBuffer>(kData);
  EXPECT_EQ(entry->WriteData(1, 0, write_buffer.get(), write_buffer->size(),
                             base::DoNothing(), false),
            static_cast<int>(write_buffer->size()));

  // Verify it is buffered.
  EXPECT_EQ(backend->GetWriteBufferTotalSizeForTesting(), write_buffer->size());

  // Check GetAvailableRange.
  base::test::TestFuture<const RangeResult&> range_future;
  RangeResult result =
      entry->GetAvailableRange(0, kData.size(), range_future.GetCallback());

  ASSERT_THAT(result.net_error, IsError(net::ERR_IO_PENDING));

  result = range_future.Get();
  EXPECT_THAT(result.net_error, IsOk());
  EXPECT_EQ(result.start, 0);
  EXPECT_EQ(result.available_len, static_cast<int>(kData.size()));

  entry->Close();
}

TEST_F(SqlBackendImplTest, CreateIteratorFlushesBuffers) {
  auto backend = CreateBackendAndInit();
  EXPECT_TRUE(LoadInMemoryIndex(*backend));
  const std::string kKey = "my-key";
  const std::string kData = "data";

  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result = cb_create.GetResult(
      backend->CreateEntry(kKey, net::HIGHEST, cb_create.callback()));
  ASSERT_THAT(create_result.net_error(), IsOk());
  auto* entry = create_result.ReleaseEntry();
  ASSERT_TRUE(entry);
  auto db_handle = static_cast<SqlEntryImpl*>(entry)->db_handle();

  auto buffer = base::MakeRefCounted<net::StringIOBuffer>(kData);
  EXPECT_EQ(entry->WriteData(1, 0, buffer.get(), buffer->size(),
                             base::DoNothing(), false),
            static_cast<int>(buffer->size()));

  // The entry is in kInitial state and has buffered data.
  // It is NOT in the DB yet.
  EXPECT_TRUE(db_handle->IsInitialState());

  auto iter = backend->CreateIterator();

  // CreateIterator should have triggered FlushBuffer(true).
  // Which starts the creation in DB.
  EXPECT_TRUE(db_handle->IsCreatingState());

  TestEntryResultCompletionCallback cb_iter;
  EntryResult iter_res = iter->OpenNextEntry(cb_iter.callback());

  iter_res = cb_iter.GetResult(std::move(iter_res));
  ASSERT_THAT(iter_res.net_error(), IsOk());
  auto* entry_from_iter = iter_res.ReleaseEntry();
  EXPECT_EQ(entry_from_iter->GetKey(), kKey);
  EXPECT_EQ(entry_from_iter->GetDataSize(1), static_cast<int>(kData.size()));

  entry->Close();
  entry_from_iter->Close();
}

TEST_F(SqlBackendImplTest, GetEntryCountFlushesBuffers) {
  auto backend = CreateBackendAndInit();
  EXPECT_TRUE(LoadInMemoryIndex(*backend));
  const std::string kKey = "my-key";

  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result = cb_create.GetResult(
      backend->CreateEntry(kKey, net::HIGHEST, cb_create.callback()));
  ASSERT_THAT(create_result.net_error(), IsOk());
  auto* entry = create_result.ReleaseEntry();
  ASSERT_TRUE(entry);
  auto db_handle = static_cast<SqlEntryImpl*>(entry)->db_handle();

  // The entry is in kInitial state and has buffered data.
  // It is NOT in the DB yet.
  EXPECT_TRUE(db_handle->IsInitialState());
  base::test::TestFuture<int32_t> future;

  EXPECT_EQ(backend->GetEntryCount(future.GetCallback()),
            base::unexpected(net::ERR_IO_PENDING));

  // GetEntryCount should have triggered FlushBuffer(true).
  // Which starts the creation in DB.
  EXPECT_TRUE(db_handle->IsCreatingState());

  CHECK_EQ(future.Get(), 1);
  EXPECT_TRUE(db_handle->IsFinished());

  entry->Close();
}

TEST_F(SqlBackendImplTest, CalculateSizeOfEntriesBetweenFlushesBuffers) {
  auto backend = CreateBackendAndInit();
  EXPECT_TRUE(LoadInMemoryIndex(*backend));
  const std::string kKey = "my-key";
  const std::string kData = "data";

  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result = cb_create.GetResult(
      backend->CreateEntry(kKey, net::HIGHEST, cb_create.callback()));
  ASSERT_THAT(create_result.net_error(), IsOk());
  auto* entry = create_result.ReleaseEntry();
  ASSERT_TRUE(entry);
  auto db_handle = static_cast<SqlEntryImpl*>(entry)->db_handle();

  auto buffer = base::MakeRefCounted<net::StringIOBuffer>(kData);
  EXPECT_EQ(entry->WriteData(1, 0, buffer.get(), buffer->size(),
                             base::DoNothing(), false),
            static_cast<int>(buffer->size()));

  // The entry is in kInitial state and has buffered data.
  // It is NOT in the DB yet.
  EXPECT_TRUE(db_handle->IsInitialState());
  base::test::TestFuture<int64_t> future;

  EXPECT_EQ(backend->CalculateSizeOfEntriesBetween(
                base::Time::Min(), base::Time::Max(), future.GetCallback()),
            net::ERR_IO_PENDING);

  // CalculateSizeOfEntriesBetween should have triggered FlushBuffer(true).
  // Which starts the creation in DB.
  EXPECT_TRUE(db_handle->IsCreatingState());

  EXPECT_EQ(future.Get(),
            kKey.length() + kData.length() + kSqlBackendStaticResourceSize);
  EXPECT_TRUE(db_handle->IsFinished());

  entry->Close();
}

// Tests a race condition where Doom runs while a WriteData operation
// is pending (blocked by another operation) and the entry is in 'Creating'
// state.
TEST_F(SqlBackendImplTest, AsyncDoomEntryAndWrite) {
  AddScopedFeatureList().InitAndEnableFeatureWithParameters(
      net::features::kDiskCacheBackendExperiment,
      {{net::features::kSqlDiskCacheMaxWriteBufferSizePerEntry.name, "250"}});
  auto backend = CreateBackendAndInit();
  EXPECT_TRUE(LoadInMemoryIndex(*backend));

  const std::string kKey = "my-key";

  // Create an entry.
  disk_cache::EntryResult create_result =
      backend->CreateEntry(kKey, net::HIGHEST, base::DoNothing());
  ASSERT_THAT(create_result.net_error(), IsOk());
  auto* entry = create_result.ReleaseEntry();
  auto db_handle = static_cast<SqlEntryImpl*>(entry)->db_handle();

  // Start an exclusive operation (CalculateSizeOfAllEntries).
  // This will block subsequent operations.
  net::TestInt64CompletionCallback cb_calculate;
  EXPECT_EQ(backend->CalculateSizeOfAllEntries(cb_calculate.callback()),
            net::ERR_IO_PENDING);

  // Doom the entry.
  entry->Doom();

  // Write data to the entry.
  // Since it is larger than kSqlDiskCacheMaxWriteBufferSizePerEntry, the task
  // for writing to the DB (WriteData) is queued.
  const int kDataSize = 1024;
  auto kData = std::string(kDataSize, 'a');
  auto buffer = base::MakeRefCounted<net::StringIOBuffer>(kData);
  EXPECT_EQ(
      entry->WriteData(1, 0, buffer.get(), kDataSize, base::DoNothing(), false),
      kDataSize);
  EXPECT_TRUE(db_handle->IsCreatingState());

  // The CalculateSizeOfAllEntries should complete.
  EXPECT_GE(cb_calculate.WaitForResult(), 0);

  // Verify that the data is readable.
  ReadAndVerifyData(entry, kData);

  entry->Close();

  // Verify that the entry is not found.
  TestEntryResultCompletionCallback cb_open;
  disk_cache::EntryResult open_result = cb_open.GetResult(
      backend->OpenEntry(kKey, net::HIGHEST, cb_open.callback()));
  EXPECT_THAT(open_result.net_error(), IsError(net::ERR_FAILED));
}

// Tests a race condition where Doom runs while a WriteEntryDataAndMetadata
// operation is pending (blocked by another operation) and the entry is in
// 'Creating' state.
TEST_F(SqlBackendImplTest, AsyncDoomEntryAndFlushBuffer) {
  AddScopedFeatureList().InitAndEnableFeatureWithParameters(
      net::features::kDiskCacheBackendExperiment,
      {{net::features::kSqlDiskCacheMaxWriteBufferSizePerEntry.name, "250"}});
  auto backend = CreateBackendAndInit();
  EXPECT_TRUE(LoadInMemoryIndex(*backend));

  const std::string kKey = "my-key";

  // Create an entry.
  disk_cache::EntryResult create_result =
      backend->CreateEntry(kKey, net::HIGHEST, base::DoNothing());
  ASSERT_THAT(create_result.net_error(), IsOk());
  auto* entry = create_result.ReleaseEntry();
  auto db_handle = static_cast<SqlEntryImpl*>(entry)->db_handle();

  // Start an exclusive operation (CalculateSizeOfAllEntries).
  // This will block subsequent operations.
  net::TestInt64CompletionCallback cb_calculate;
  EXPECT_EQ(backend->CalculateSizeOfAllEntries(cb_calculate.callback()),
            net::ERR_IO_PENDING);

  // Doom the entry.
  entry->Doom();

  // Calling CreateIterator() queues the task for writing to the DB
  // (WriteEntryDataAndMetadata).
  auto iter = backend->CreateIterator();
  EXPECT_TRUE(db_handle->IsCreatingState());

  // The CalculateSizeOfAllEntries should complete.
  EXPECT_GE(cb_calculate.WaitForResult(), 0);

  TestEntryResultCompletionCallback cb_iter;
  EntryResult iter_res = iter->OpenNextEntry(cb_iter.callback());

  // The iterator shouldn't see the entry.
  iter_res = cb_iter.GetResult(std::move(iter_res));
  ASSERT_THAT(iter_res.net_error(), IsError(net::ERR_FAILED));

  entry->Close();

  // Verify that the entry is not found.
  TestEntryResultCompletionCallback cb_open;
  disk_cache::EntryResult open_result = cb_open.GetResult(
      backend->OpenEntry(kKey, net::HIGHEST, cb_open.callback()));
  EXPECT_THAT(open_result.net_error(), IsError(net::ERR_FAILED));
}

void SqlBackendImplTest::RunSparseDataExceedsMaxFileSizeTest(bool doom_entry) {
  auto backend = CreateBackendAndInit();
  EXPECT_TRUE(LoadInMemoryIndex(*backend));
  backend->EnableStrictCorruptionCheckForTesting();
  const std::string kKey = "my-key";

  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result = cb_create.GetResult(
      backend->CreateEntry(kKey, net::HIGHEST, cb_create.callback()));
  ASSERT_THAT(create_result.net_error(), IsOk());
  auto* entry = create_result.ReleaseEntry();
  ASSERT_TRUE(entry);

  if (doom_entry) {
    entry->Doom();
  }

  const int chunk_size = 32 * 1024;
  const int num_chunks = backend->GetSqlStoreForTest()->MaxSize() / chunk_size;
  auto buf = base::MakeRefCounted<net::IOBufferWithSize>(chunk_size);
  std::ranges::fill(buf->span(), 'a');

  base::HistogramTester histogram_tester;

  for (int i = 0; i < num_chunks; ++i) {
    net::TestCompletionCallback cb;
    EXPECT_EQ(cb.GetResult(entry->WriteSparseData(i * chunk_size, buf.get(),
                                                  chunk_size, cb.callback())),
              chunk_size);
    backend->RunUntilAllTasksCompleteForTest();
  }

  // Truncating older sparse data prevents a single entry from exceeding the
  // cache size limit and causing excessive evictions on every write. Ensure
  // eviction was not triggered.
  histogram_tester.ExpectTotalCount(
      "Net.SqlDiskCache.Backend.RunEviction.ScannedEntriesCount.Success", 0);

  // Check if the first chunk is truncated.
  TestRangeResultCompletionCallback cb_range;
  EXPECT_EQ(cb_range
                .GetResult(entry->GetAvailableRange(0, chunk_size,
                                                    cb_range.callback()))
                .available_len,
            0);

  entry->Close();
}

TEST_F(SqlBackendImplTest, SparseDataExceedsMaxFileSize) {
  RunSparseDataExceedsMaxFileSizeTest(/*doom_entry=*/false);
}

TEST_F(SqlBackendImplTest, SparseDataExceedsMaxFileSizeDoomedEntry) {
  RunSparseDataExceedsMaxFileSizeTest(/*doom_entry=*/true);
}

TEST_F(SqlBackendImplTest, SparseDataExceedsMaxFileSizeBackwards) {
  auto backend = CreateBackendAndInit();
  EXPECT_TRUE(LoadInMemoryIndex(*backend));
  backend->EnableStrictCorruptionCheckForTesting();
  const std::string kKey = "my-key";

  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result = cb_create.GetResult(
      backend->CreateEntry(kKey, net::HIGHEST, cb_create.callback()));
  ASSERT_THAT(create_result.net_error(), IsOk());
  auto* entry = create_result.ReleaseEntry();
  ASSERT_TRUE(entry);

  const int chunk_size = 32 * 1024;
  const int num_chunks = backend->GetSqlStoreForTest()->MaxSize() / chunk_size;
  auto buf = base::MakeRefCounted<net::IOBufferWithSize>(chunk_size);
  std::ranges::fill(buf->span(), 'a');

  base::HistogramTester histogram_tester;

  // Write chunks in reverse order.
  for (int i = num_chunks - 1; i >= 0; --i) {
    net::TestCompletionCallback cb;
    EXPECT_EQ(cb.GetResult(entry->WriteSparseData(i * chunk_size, buf.get(),
                                                  chunk_size, cb.callback())),
              chunk_size);
    backend->RunUntilAllTasksCompleteForTest();
  }

  histogram_tester.ExpectTotalCount(
      "Net.SqlDiskCache.Backend.RunEviction.ScannedEntriesCount.Success", 0);

  // The truncation logic trims data before and after the current write when
  // the size limit is exceeded. Since we wrote backwards, the last written
  // chunk was at offset 0. Thus, the previously written chunks at higher
  // offsets were truncated to keep the total size within the limit.
  TestRangeResultCompletionCallback cb_range;
  EXPECT_EQ(
      cb_range
          .GetResult(entry->GetAvailableRange((num_chunks - 1) * chunk_size,
                                              chunk_size, cb_range.callback()))
          .available_len,
      0);

  entry->Close();
}

TEST_F(SqlBackendImplTest, ReadFromSharedCacheViaOpenEntry) {
  AddScopedFeatureList().InitAndEnableFeature(
      net::features::kRendererAccessibleHttpCache);
  auto backend = CreateBackendAndInit();
  EXPECT_TRUE(LoadInMemoryIndex(*backend));

  const std::string kKey = "shared-cache-key";
  const std::string kData = "Data stored in shared cache";

  SqlSharedCacheResourceId expected_resource_id =
      CreateEntryInSharedCache(*backend, kKey, kData);
  ASSERT_TRUE(expected_resource_id.db_id.value());

  // Now open the entry using OpenEntry.
  TestEntryResultCompletionCallback cb_open;
  disk_cache::EntryResult open_result = cb_open.GetResult(
      backend->OpenEntry(kKey, net::HIGHEST, cb_open.callback()));
  ASSERT_THAT(open_result.net_error(), IsOk());
  auto* opened_entry = open_result.ReleaseEntry();
  ASSERT_TRUE(opened_entry);

  // Check db_handle has shared_cache_resource_id.
  auto* opened_sql_entry = static_cast<SqlEntryImpl*>(opened_entry);
  EXPECT_TRUE(
      opened_sql_entry->db_handle()->shared_cache_resource_id().has_value());
  EXPECT_EQ(opened_sql_entry->db_handle()->shared_cache_resource_id()->db_id,
            expected_resource_id.db_id);
  EXPECT_EQ(opened_sql_entry->db_handle()->shared_cache_resource_id()->row_id,
            expected_resource_id.row_id);

  // Read data stream 1 from opened entry and verify content matches kData.
  auto read_buf = base::MakeRefCounted<net::IOBufferWithSize>(kData.size());
  net::TestCompletionCallback cb_read;
  EXPECT_EQ(cb_read.GetResult(opened_entry->ReadData(
                1, 0, read_buf.get(), kData.size(), cb_read.callback())),
            static_cast<int>(kData.size()));
  EXPECT_EQ(std::string_view(read_buf->data(), kData.size()), kData);

  opened_entry->Close();
  backend->RunUntilAllTasksCompleteForTest();
}

TEST_F(SqlBackendImplTest, ReadFromSharedCacheViaOpenNextEntry) {
  AddScopedFeatureList().InitAndEnableFeature(
      net::features::kRendererAccessibleHttpCache);
  auto backend = CreateBackendAndInit();
  EXPECT_TRUE(LoadInMemoryIndex(*backend));

  const std::string kKey = "shared-cache-iter-key";
  const std::string kData = "Data stored in shared cache for iterator";

  SqlSharedCacheResourceId expected_resource_id =
      CreateEntryInSharedCache(*backend, kKey, kData);
  ASSERT_TRUE(expected_resource_id.db_id.value());

  // Open entry via iterator (CreateIterator & OpenNextEntry).
  auto iterator = backend->CreateIterator();
  ASSERT_TRUE(iterator);

  TestEntryResultCompletionCallback cb_next;
  disk_cache::EntryResult next_result =
      cb_next.GetResult(iterator->OpenNextEntry(cb_next.callback()));
  ASSERT_THAT(next_result.net_error(), IsOk());
  auto* next_entry = next_result.ReleaseEntry();
  ASSERT_TRUE(next_entry);
  EXPECT_EQ(next_entry->GetKey(), kKey);

  // Check db_handle has shared_cache_resource_id.
  auto* next_sql_entry = static_cast<SqlEntryImpl*>(next_entry);
  EXPECT_TRUE(
      next_sql_entry->db_handle()->shared_cache_resource_id().has_value());
  EXPECT_EQ(next_sql_entry->db_handle()->shared_cache_resource_id()->db_id,
            expected_resource_id.db_id);
  EXPECT_EQ(next_sql_entry->db_handle()->shared_cache_resource_id()->row_id,
            expected_resource_id.row_id);

  // Read data stream 1 from iterator-opened entry and verify content matches
  // kData.
  auto read_buf = base::MakeRefCounted<net::IOBufferWithSize>(kData.size());
  net::TestCompletionCallback cb_read;
  EXPECT_EQ(cb_read.GetResult(next_entry->ReadData(
                1, 0, read_buf.get(), kData.size(), cb_read.callback())),
            static_cast<int>(kData.size()));
  EXPECT_EQ(std::string_view(read_buf->data(), kData.size()), kData);

  next_entry->Close();
  backend->RunUntilAllTasksCompleteForTest();
}

TEST_F(SqlBackendImplTest, ReadFromSharedCacheWithLargerBuffer) {
  AddScopedFeatureList().InitAndEnableFeature(
      net::features::kRendererAccessibleHttpCache);
  auto backend = CreateBackendAndInit();
  EXPECT_TRUE(LoadInMemoryIndex(*backend));

  const std::string kKey = "shared-cache-drainable-key";
  const std::string kData = "0123456789";

  SqlSharedCacheResourceId expected_resource_id =
      CreateEntryInSharedCache(*backend, kKey, kData);
  ASSERT_TRUE(expected_resource_id.db_id.value());

  TestEntryResultCompletionCallback cb_open;
  disk_cache::EntryResult open_result = cb_open.GetResult(
      backend->OpenEntry(kKey, net::HIGHEST, cb_open.callback()));
  ASSERT_THAT(open_result.net_error(), IsOk());
  auto* opened_entry = open_result.ReleaseEntry();
  ASSERT_TRUE(opened_entry);

  // Pass a buffer larger than kData.size() (20 bytes > 10 bytes) to ensure
  // buffer->size() != bytes_to_read branch is taken.
  auto read_buf = base::MakeRefCounted<net::IOBufferWithSize>(20);
  net::TestCompletionCallback cb_read;
  EXPECT_EQ(cb_read.GetResult(opened_entry->ReadData(1, 0, read_buf.get(), 20,
                                                     cb_read.callback())),
            static_cast<int>(kData.size()));
  EXPECT_EQ(std::string_view(read_buf->data(), kData.size()), kData);

  opened_entry->Close();
  backend->RunUntilAllTasksCompleteForTest();
}

TEST_F(SqlBackendImplTest, ReadFromSharedCacheHandleNotFound) {
  AddScopedFeatureList().InitAndEnableFeature(
      net::features::kRendererAccessibleHttpCache);
  auto backend = CreateBackendAndInit();
  EXPECT_TRUE(LoadInMemoryIndex(*backend));

  const std::string kKey = "shared-cache-not-found-key";
  const std::string kData = "Data in shared cache";

  SqlSharedCacheResourceId expected_resource_id =
      CreateEntryInSharedCache(*backend, kKey, kData);
  ASSERT_TRUE(expected_resource_id.db_id.value());

  TestEntryResultCompletionCallback cb_open;
  disk_cache::EntryResult open_result = cb_open.GetResult(
      backend->OpenEntry(kKey, net::HIGHEST, cb_open.callback()));
  ASSERT_THAT(open_result.net_error(), IsOk());
  auto* opened_entry = open_result.ReleaseEntry();
  ASSERT_TRUE(opened_entry);

  // Set a non-existent shared_cache_resource_id so GetCacheByDbId will fail and
  // pass a null handle.
  auto* opened_sql_entry = static_cast<SqlEntryImpl*>(opened_entry);
  opened_sql_entry->db_handle()->set_shared_cache_resource_id(
      SqlSharedCacheResourceId{SqlSharedCacheDbId(9999),
                               SqlSharedCacheRowId(1)});

  auto read_buf = base::MakeRefCounted<net::IOBufferWithSize>(kData.size());
  net::TestCompletionCallback cb_read;
  int read_res = opened_entry->ReadData(1, 0, read_buf.get(), kData.size(),
                                        cb_read.callback());
  if (read_res == net::ERR_IO_PENDING) {
    backend->RunUntilAllTasksCompleteForTest();
    read_res = cb_read.WaitForResult();
  }
  EXPECT_EQ(read_res, net::ERR_FAILED);

  opened_entry->Close();
  backend->RunUntilAllTasksCompleteForTest();
}

TEST_F(SqlBackendImplTest, BackendInterfaceSupportsSharedCacheDisabled) {
  auto backend = CreateBackend();
  base::test::TestFuture<int> future;
  backend->Init(future.GetCallback());
  ASSERT_EQ(future.Get(), net::OK);

  disk_cache::Backend* base_backend = backend.get();
  EXPECT_FALSE(base_backend->SupportsSharedCache());

  EXPECT_CHECK_DEATH(base_backend->RegisterSharedCacheClientRemote(
      net::NetworkIsolationKey(), nullptr));

  EXPECT_CHECK_DEATH(base_backend->OnEntryEligibleForSharedCache(
      "key", GURL("https://example.com"),
      std::make_unique<net::HttpResponseInfo>(), net::NetworkIsolationKey()));

  EXPECT_CHECK_DEATH(base_backend->ProcessAllSharedCacheEligibleEntriesForTest(
      base::ScopedClosureRunner()));
}

class SqlBackendImplSharedCacheTest : public SqlBackendImplTest {
 public:
  SqlBackendImplSharedCacheTest() {
    AddScopedFeatureList().InitAndEnableFeature(
        net::features::kRendererAccessibleHttpCache);
  }

 protected:
  void WriteResponseInfoToEntry(disk_cache::Entry* entry,
                                const net::HttpResponseInfo& info) {
    auto pickle = info.MakePickle(false, false);
    std::string pickle_data(reinterpret_cast<const char*>(pickle->data()),
                            pickle->size());
    auto buffer = base::MakeRefCounted<net::StringIOBuffer>(pickle_data);
    net::TestCompletionCallback cb;
    EXPECT_EQ(cb.GetResult(entry->WriteData(0, 0, buffer.get(),
                                            static_cast<int>(pickle->size()),
                                            cb.callback(), false)),
              static_cast<int>(pickle->size()));
  }

  // Helper to create an entry in store, write data and response info,
  // optionally close it, and register it to shared cache.
  disk_cache::Entry* CreateAndRegisterSharedCacheEntry(
      SqlBackendImpl* backend,
      const std::string& key,
      const std::string& data,
      const net::NetworkIsolationKey& nik,
      base::Time response_time = base::Time::Now(),
      bool close_entry = true) {
    auto* entry = CreateEntryAndWriteData(backend, key, data);
    net::HttpResponseInfo info;
    info.response_time = response_time;
    info.headers =
        base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\0\0");
    WriteResponseInfoToEntry(entry, info);
    if (close_entry) {
      entry->Close();
    }
    GURL url{net::HttpCache::GetResourceURLFromHttpCacheKey(key)};
    backend->OnEntryEligibleForSharedCache(
        key, url, std::make_unique<net::HttpResponseInfo>(info), nik);
    return close_entry ? nullptr : entry;
  }

  void VerifySharedCacheResourceIdExists(SqlBackendImpl* backend,
                                         const std::string& key) {
    base::test::TestFuture<SqlPersistentStore::EntryInfoOrError> future;
    backend->GetSqlStoreForTest()->OpenEntry(CacheEntryKey(key),
                                             future.GetCallback());
    auto result = future.Take();
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->shared_cache_resource_id.has_value());
  }
};

TEST_F(SqlBackendImplSharedCacheTest, ProcessSharedCacheEligibleEntries) {
  auto backend = CreateBackendAndInit();
  const std::string kKey = "0/0/https://example.com/";
  const GURL kUrl("https://example.com");
  const net::SchemefulSite kSite(GURL("https://example.com"));
  const net::NetworkIsolationKey kNik(kSite, kSite);
  const std::string kData = "Hello Shared Cache";

  // 1. Save entry
  auto* entry = CreateEntryAndWriteData(backend.get(), kKey, kData);

  base::Time response_time = base::Time::Now();
  net::HttpResponseInfo response_info_for_pickle;
  response_info_for_pickle.response_time = response_time;
  response_info_for_pickle.headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\0\0");
  std::unique_ptr<base::Pickle> pickle =
      response_info_for_pickle.MakePickle(false, false);
  std::string pickle_data(reinterpret_cast<const char*>(pickle->data()),
                          pickle->size());
  auto pickle_buffer = base::MakeRefCounted<net::StringIOBuffer>(pickle_data);
  net::TestCompletionCallback cb_write_pickle;
  EXPECT_EQ(cb_write_pickle.GetResult(
                entry->WriteData(0, 0, pickle_buffer.get(), pickle->size(),
                                 cb_write_pickle.callback(), false)),
            static_cast<int>(pickle->size()));

  entry->Close();

  // 2. Call OnEntryEligibleForSharedCache
  auto response_info =
      std::make_unique<net::HttpResponseInfo>(response_info_for_pickle);
  backend->OnEntryEligibleForSharedCache(kKey, kUrl, std::move(response_info),
                                         kNik);
  EXPECT_EQ(backend->GetSharedCacheEligibleEntriesCountForTest(), 1u);

  // 3. Register MockSharedCacheClientRemote
  auto client = std::make_unique<MockSharedCacheClientRemote>();
  auto* client_ptr = client.get();
  backend->RegisterSharedCacheClientRemote(kNik, std::move(client));

  // 4. Process entries
  base::RunLoop process_run_loop;
  backend->ProcessSharedCacheEligibleEntriesForTest(
      base::ScopedClosureRunner(process_run_loop.QuitClosure()),
      /*on_entry_copied_callback=*/base::NullCallback());

  // 5. Wait for Initialize and OnResourcesAdded
  client_ptr->WaitUntilInitialized();
  client_ptr->WaitUntilOnResourcesAdded();
  process_run_loop.Run();

  EXPECT_TRUE(client_ptr->initialize_called());
  EXPECT_TRUE(client_ptr->on_resources_added_called());
  EXPECT_TRUE(client_ptr->has_disconnect_handler());

  // 6. Use pending_file_set_ to read
  SqlSharedCacheIsolatedDatabaseReader reader(client_ptr->TakePendingFileSet());

  std::optional<SqlSharedCacheIsolatedDatabaseReader::Response> response =
      reader.ReadResponse(kUrl.spec());
  ASSERT_TRUE(response);
  EXPECT_GT(response->GetBodySize(), 0);

  std::vector<uint8_t> buffer(kData.size());
  EXPECT_TRUE(response->ReadBody(buffer));
  EXPECT_EQ(std::string(buffer.begin(), buffer.end()), kData);

  // 7. Call disconnect_handler
  auto* manager =
      backend->GetSqlStoreForTest()->shared_cache_manager_for_testing();
  EXPECT_EQ(manager->GetSharedCachesSizeForTest(), 1u);
  EXPECT_EQ(manager->GetSharedCachesByDbIdSizeForTest(), 1u);
  EXPECT_EQ(manager->GetSharedCachesByNikSizeForTest(), 1u);

  client_ptr->RunDisconnectHandler();

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return manager->GetSharedCachesSizeForTest() == 0u; }));

  // 8. Verify empty
  EXPECT_EQ(manager->GetSharedCachesByDbIdSizeForTest(), 0u);
  EXPECT_EQ(manager->GetSharedCachesByNikSizeForTest(), 0u);
}

TEST_F(SqlBackendImplSharedCacheTest,
       ProcessSharedCacheEligibleEntriesIncremental) {
  auto backend = CreateBackendAndInit();

  const net::SchemefulSite kSite(GURL("https://example.test"));
  const net::NetworkIsolationKey kNik(kSite, kSite);
  const std::string kKey1 = "0/0/https://example.test/1";
  const GURL kUrl1("https://example.test/1");
  const std::string kKey2 = "0/0/https://example.test/2";
  const GURL kUrl2("https://example.test/2");

  // 1. Create entry 1 and mark eligible
  CreateAndRegisterSharedCacheEntry(backend.get(), kKey1, "Data 1", kNik);
  backend->RunUntilAllTasksCompleteForTest();
  EXPECT_EQ(backend->GetSharedCacheEligibleEntriesCountForTest(), 1u);

  // 2. Register MockSharedCacheClientRemote
  auto client = std::make_unique<MockSharedCacheClientRemote>();
  auto* client_ptr = client.get();
  backend->RegisterSharedCacheClientRemote(kNik, std::move(client));

  // 3. Process entry 1
  base::RunLoop process_run_loop1;
  backend->ProcessSharedCacheEligibleEntriesForTest(
      base::ScopedClosureRunner(process_run_loop1.QuitClosure()),
      /*on_entry_copied_callback=*/base::NullCallback());

  client_ptr->WaitUntilInitialized();
  client_ptr->WaitUntilOnResourcesAdded(1);
  process_run_loop1.Run();

  EXPECT_EQ(client_ptr->on_resources_added_call_count(), 1u);
  EXPECT_THAT(client_ptr->new_hashes(),
              testing::ElementsAre(base::PersistentHash(kUrl1.spec())));

  // 4. Create entry 2 and mark eligible
  CreateAndRegisterSharedCacheEntry(backend.get(), kKey2, "Data 2", kNik);
  backend->RunUntilAllTasksCompleteForTest();
  EXPECT_EQ(backend->GetSharedCacheEligibleEntriesCountForTest(), 1u);

  // 5. Process entry 2
  base::RunLoop process_run_loop2;
  backend->ProcessSharedCacheEligibleEntriesForTest(
      base::ScopedClosureRunner(process_run_loop2.QuitClosure()),
      /*on_entry_copied_callback=*/base::NullCallback());

  client_ptr->WaitUntilOnResourcesAdded(2);
  process_run_loop2.Run();

  // Verify that OnResourcesAdded was called again with only the newly added
  // hash.
  EXPECT_EQ(client_ptr->on_resources_added_call_count(), 2u);
  EXPECT_THAT(client_ptr->new_hashes(),
              testing::ElementsAre(base::PersistentHash(kUrl2.spec())));

  auto* manager =
      backend->GetSqlStoreForTest()->shared_cache_manager_for_testing();
  EXPECT_EQ(manager->GetSharedCachesSizeForTest(), 1u);

  client_ptr->RunDisconnectHandler();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return manager->GetSharedCachesSizeForTest() == 0u; }));
}

TEST_F(SqlBackendImplSharedCacheTest,
       ProcessSharedCacheEligibleEntriesDuplicate) {
  auto backend = CreateBackendAndInit();

  const net::SchemefulSite kSite(GURL("https://example.test"));
  const net::NetworkIsolationKey kNik(kSite, kSite);
  const std::string kKey = "0/0/https://example.test/";
  const GURL kUrl("https://example.test/");
  const std::string kData = "Data";
  base::Time response_time = base::Time::Now();

  // 1. Create and copy the entry.
  CreateAndRegisterSharedCacheEntry(backend.get(), kKey, kData, kNik,
                                    response_time);
  backend->RunUntilAllTasksCompleteForTest();

  auto client = std::make_unique<MockSharedCacheClientRemote>();
  auto* client_ptr = client.get();
  backend->RegisterSharedCacheClientRemote(kNik, std::move(client));

  base::RunLoop process_run_loop1;
  backend->ProcessSharedCacheEligibleEntriesForTest(
      base::ScopedClosureRunner(process_run_loop1.QuitClosure()),
      /*on_entry_copied_callback=*/base::NullCallback());

  client_ptr->WaitUntilInitialized();
  client_ptr->WaitUntilOnResourcesAdded(1);
  process_run_loop1.Run();

  EXPECT_EQ(client_ptr->on_resources_added_call_count(), 1u);
  EXPECT_THAT(client_ptr->new_hashes(),
              testing::ElementsAre(base::PersistentHash(kUrl.spec())));

  // 2. Mark the same entry eligible again and process.
  net::HttpResponseInfo info;
  info.response_time = response_time;
  info.headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\0\0");
  backend->OnEntryEligibleForSharedCache(
      kKey, kUrl, std::make_unique<net::HttpResponseInfo>(info), kNik);

  base::RunLoop process_run_loop2;
  backend->ProcessSharedCacheEligibleEntriesForTest(
      base::ScopedClosureRunner(process_run_loop2.QuitClosure()),
      /*on_entry_copied_callback=*/base::NullCallback());
  process_run_loop2.Run();

  // OnResourcesAdded should NOT be called again because the hash is already
  // cached.
  EXPECT_EQ(client_ptr->on_resources_added_call_count(), 1u);

  client_ptr->RunDisconnectHandler();
  auto* manager =
      backend->GetSqlStoreForTest()->shared_cache_manager_for_testing();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return manager->GetSharedCachesSizeForTest() == 0u; }));
}

TEST_F(SqlBackendImplSharedCacheTest,
       ProcessSharedCacheEligibleEntriesMultipleClientsBroadcast) {
  auto backend = CreateBackendAndInit();

  const net::SchemefulSite kSite(GURL("https://example.test"));
  const net::NetworkIsolationKey kNik(kSite, kSite);
  const std::string kKey = "0/0/https://example.test/";
  const GURL kUrl("https://example.test/");
  const std::string kData = "Data";

  CreateAndRegisterSharedCacheEntry(backend.get(), kKey, kData, kNik);
  backend->RunUntilAllTasksCompleteForTest();

  // Register two clients for the same NIK.
  auto client1 = std::make_unique<MockSharedCacheClientRemote>();
  auto* client_ptr1 = client1.get();
  backend->RegisterSharedCacheClientRemote(kNik, std::move(client1));

  auto client2 = std::make_unique<MockSharedCacheClientRemote>();
  auto* client_ptr2 = client2.get();
  backend->RegisterSharedCacheClientRemote(kNik, std::move(client2));

  base::RunLoop process_run_loop;
  backend->ProcessSharedCacheEligibleEntriesForTest(
      base::ScopedClosureRunner(process_run_loop.QuitClosure()),
      /*on_entry_copied_callback=*/base::NullCallback());

  client_ptr1->WaitUntilInitialized();
  client_ptr1->WaitUntilOnResourcesAdded(1);
  client_ptr2->WaitUntilInitialized();
  client_ptr2->WaitUntilOnResourcesAdded(1);
  process_run_loop.Run();

  // Both clients should receive OnResourcesAdded with the new hash.
  EXPECT_EQ(client_ptr1->on_resources_added_call_count(), 1u);
  EXPECT_THAT(client_ptr1->new_hashes(),
              testing::ElementsAre(base::PersistentHash(kUrl.spec())));

  EXPECT_EQ(client_ptr2->on_resources_added_call_count(), 1u);
  EXPECT_THAT(client_ptr2->new_hashes(),
              testing::ElementsAre(base::PersistentHash(kUrl.spec())));

  client_ptr1->RunDisconnectHandler();
  client_ptr2->RunDisconnectHandler();

  auto* manager =
      backend->GetSqlStoreForTest()->shared_cache_manager_for_testing();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return manager->GetSharedCachesSizeForTest() == 0u; }));
}

TEST_F(SqlBackendImplSharedCacheTest,
       ProcessSharedCacheEligibleEntriesPartialClientDisconnect) {
  auto backend = CreateBackendAndInit();

  const net::SchemefulSite kSite(GURL("https://example.test"));
  const net::NetworkIsolationKey kNik(kSite, kSite);
  const std::string kKey1 = "0/0/https://example.test/1";
  const GURL kUrl1("https://example.test/1");
  const std::string kKey2 = "0/0/https://example.test/2";
  const GURL kUrl2("https://example.test/2");

  // Register two clients for the same NIK.
  bool client1_destroyed = false;
  auto client1 = std::make_unique<MockSharedCacheClientRemote>();
  client1->SetOnDestroyHandler(
      base::BindLambdaForTesting([&]() { client1_destroyed = true; }));
  auto* client_ptr1 = client1.get();
  backend->RegisterSharedCacheClientRemote(kNik, std::move(client1));

  auto client2 = std::make_unique<MockSharedCacheClientRemote>();
  auto* client_ptr2 = client2.get();
  backend->RegisterSharedCacheClientRemote(kNik, std::move(client2));

  // Copy entry 1.
  CreateAndRegisterSharedCacheEntry(backend.get(), kKey1, "Data 1", kNik);
  backend->RunUntilAllTasksCompleteForTest();

  base::RunLoop process_run_loop1;
  backend->ProcessSharedCacheEligibleEntriesForTest(
      base::ScopedClosureRunner(process_run_loop1.QuitClosure()),
      /*on_entry_copied_callback=*/base::NullCallback());

  client_ptr1->WaitUntilInitialized();
  client_ptr1->WaitUntilOnResourcesAdded(1);
  client_ptr2->WaitUntilInitialized();
  client_ptr2->WaitUntilOnResourcesAdded(1);
  process_run_loop1.Run();

  // Disconnect client 1.
  client_ptr1->RunDisconnectHandler();
  EXPECT_TRUE(client1_destroyed);

  // Copy entry 2.
  CreateAndRegisterSharedCacheEntry(backend.get(), kKey2, "Data 2", kNik);
  backend->RunUntilAllTasksCompleteForTest();

  base::RunLoop process_run_loop2;
  backend->ProcessSharedCacheEligibleEntriesForTest(
      base::ScopedClosureRunner(process_run_loop2.QuitClosure()),
      /*on_entry_copied_callback=*/base::NullCallback());

  client_ptr2->WaitUntilOnResourcesAdded(2);
  process_run_loop2.Run();

  // client 1 was destroyed upon disconnect, and client 2 receives call count 2.
  EXPECT_EQ(client_ptr2->on_resources_added_call_count(), 2u);
  EXPECT_THAT(client_ptr2->new_hashes(),
              testing::ElementsAre(base::PersistentHash(kUrl2.spec())));

  client_ptr2->RunDisconnectHandler();
  auto* manager =
      backend->GetSqlStoreForTest()->shared_cache_manager_for_testing();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return manager->GetSharedCachesSizeForTest() == 0u; }));
}

TEST_F(SqlBackendImplSharedCacheTest,
       RegisterSharedCacheClientRemoteTransientNik) {
  auto backend = CreateBackendAndInit();
  auto transient_nik = net::NetworkIsolationKey::CreateTransientForTesting();
  ASSERT_TRUE(transient_nik.IsTransient());

  bool client_destroyed = false;
  auto client = std::make_unique<MockSharedCacheClientRemote>();
  client->SetOnDestroyHandler(
      base::BindLambdaForTesting([&]() { client_destroyed = true; }));

  backend->RegisterSharedCacheClientRemote(transient_nik, std::move(client));

  // The client should be destroyed immediately because transient NIKs are
  // ignored.
  EXPECT_TRUE(client_destroyed);

  auto* manager =
      backend->GetSqlStoreForTest()->shared_cache_manager_for_testing();
  EXPECT_EQ(manager->GetSharedCachesSizeForTest(), 0u);
}

TEST_F(SqlBackendImplSharedCacheTest,
       ProcessSharedCacheEligibleEntriesMultipleNiks) {
  auto backend = CreateBackendAndInit();

  const net::SchemefulSite kSite1(GURL("https://site1.test"));
  const net::NetworkIsolationKey kNik1(kSite1, kSite1);
  const net::SchemefulSite kSite2(GURL("https://site2.test"));
  const net::NetworkIsolationKey kNik2(kSite2, kSite2);

  const std::string kKey1 = "0/0/https://example1.test";
  const std::string kKey2 = "0/0/https://example2.test";

  CreateAndRegisterSharedCacheEntry(backend.get(), kKey1, "Data 1", kNik1);
  CreateAndRegisterSharedCacheEntry(backend.get(), kKey2, "Data 2", kNik2);
  backend->RunUntilAllTasksCompleteForTest();

  EXPECT_EQ(backend->GetSharedCacheEligibleEntriesCountForTest(), 2u);

  // Register clients
  auto client1 = std::make_unique<MockSharedCacheClientRemote>();
  auto* client_ptr1 = client1.get();
  backend->RegisterSharedCacheClientRemote(kNik1, std::move(client1));

  auto client2 = std::make_unique<MockSharedCacheClientRemote>();
  auto* client_ptr2 = client2.get();
  backend->RegisterSharedCacheClientRemote(kNik2, std::move(client2));

  // Process
  base::RunLoop process_run_loop;
  backend->ProcessSharedCacheEligibleEntriesForTest(
      base::ScopedClosureRunner(process_run_loop.QuitClosure()),
      /*on_entry_copied_callback=*/base::NullCallback());

  client_ptr1->WaitUntilInitialized();
  client_ptr1->WaitUntilOnResourcesAdded();
  client_ptr2->WaitUntilInitialized();
  client_ptr2->WaitUntilOnResourcesAdded();

  process_run_loop.Run();

  VerifySharedCacheResourceIdExists(backend.get(), kKey1);
  VerifySharedCacheResourceIdExists(backend.get(), kKey2);

  EXPECT_TRUE(client_ptr1->initialize_called());
  EXPECT_TRUE(client_ptr2->initialize_called());

  auto* manager =
      backend->GetSqlStoreForTest()->shared_cache_manager_for_testing();
  EXPECT_EQ(manager->GetSharedCachesSizeForTest(), 2u);

  client_ptr1->RunDisconnectHandler();
  client_ptr2->RunDisconnectHandler();

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return manager->GetSharedCachesSizeForTest() == 0u; }));
}

TEST_F(SqlBackendImplSharedCacheTest,
       ProcessSharedCacheEligibleEntriesDbFailure) {
  auto backend = CreateBackendAndInit();
  auto* manager =
      backend->GetSqlStoreForTest()->shared_cache_manager_for_testing();
  manager->SetSimulateDbFailureForTesting(true);

  const net::SchemefulSite kSite(GURL("https://example.test"));
  const net::NetworkIsolationKey kNik(kSite, kSite);
  const std::string kKey = "0/0/https://example.test";

  CreateAndRegisterSharedCacheEntry(backend.get(), kKey, "Hello Shared Cache",
                                    kNik);
  EXPECT_EQ(backend->GetSharedCacheEligibleEntriesCountForTest(), 1u);

  base::RunLoop destroy_run_loop;
  auto client = std::make_unique<MockSharedCacheClientRemote>();
  client->SetOnDestroyHandler(destroy_run_loop.QuitClosure());
  backend->RegisterSharedCacheClientRemote(kNik, std::move(client));

  base::RunLoop process_run_loop;
  backend->ProcessSharedCacheEligibleEntriesForTest(
      base::ScopedClosureRunner(process_run_loop.QuitClosure()),
      base::NullCallback());
  process_run_loop.Run();

  // On DB failure, processing fails and the entries are dropped, leaving 0
  // pending eligible entries.
  EXPECT_EQ(backend->GetSharedCacheEligibleEntriesCountForTest(), 0u);

  // Due to DB failure, processing fails and cache drops, which triggers client
  // destruction.
  destroy_run_loop.Run();

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return manager->GetSharedCachesSizeForTest() == 0u; }));
}

TEST_F(SqlBackendImplSharedCacheTest,
       ProcessSharedCacheEligibleEntriesSkipsActiveEntry) {
  auto backend = CreateBackendAndInit();

  const net::SchemefulSite kSite1(GURL("https://site1.test"));
  const net::NetworkIsolationKey kNik1(kSite1, kSite1);
  const net::SchemefulSite kSite2(GURL("https://site2.test"));
  const net::NetworkIsolationKey kNik2(kSite2, kSite2);

  const std::string kKey1 = "0/0/https://example1.test";
  const std::string kKey2 = "0/0/https://example2.test";

  // Create entry1 and entry2 and keep BOTH open (active).
  auto* entry1 = CreateAndRegisterSharedCacheEntry(
      backend.get(), kKey1, "Data 1", kNik1, base::Time::Now(),
      /*close_entry=*/false);
  auto* entry2 = CreateAndRegisterSharedCacheEntry(
      backend.get(), kKey2, "Data 2", kNik2, base::Time::Now(),
      /*close_entry=*/false);
  backend->RunUntilAllTasksCompleteForTest();

  EXPECT_EQ(backend->GetSharedCacheEligibleEntriesCountForTest(), 2u);

  // 1. When ALL entries are active, HandleProcessSharedCacheEligibleEntries
  // finds entries_to_process is empty and returns early without processing.
  base::RunLoop process_run_loop_all_active;
  backend->ProcessSharedCacheEligibleEntriesForTest(
      base::ScopedClosureRunner(process_run_loop_all_active.QuitClosure()),
      /*on_entry_copied_callback=*/base::NullCallback());
  process_run_loop_all_active.Run();
  backend->RunUntilAllTasksCompleteForTest();

  // Both entries were active, so 0 entries were processed, leaving 2 eligible
  // entries.
  EXPECT_EQ(backend->GetSharedCacheEligibleEntriesCountForTest(), 2u);

  // 2. Close entry2, keep entry1 open.
  entry2->Close();
  backend->RunUntilAllTasksCompleteForTest();

  base::RunLoop process_run_loop;
  backend->ProcessSharedCacheEligibleEntriesForTest(
      base::ScopedClosureRunner(process_run_loop.QuitClosure()),
      /*on_entry_copied_callback=*/base::NullCallback());

  process_run_loop.Run();
  backend->RunUntilAllTasksCompleteForTest();

  // entry1 was skipped (remains active/eligible), entry2 was processed.
  EXPECT_EQ(backend->GetSharedCacheEligibleEntriesCountForTest(), 1u);
  VerifySharedCacheResourceIdExists(backend.get(), kKey2);

  // 3. Close entry1 and process again.
  entry1->Close();
  backend->RunUntilAllTasksCompleteForTest();

  base::RunLoop process_run_loop2;
  backend->ProcessSharedCacheEligibleEntriesForTest(
      base::ScopedClosureRunner(process_run_loop2.QuitClosure()),
      /*on_entry_copied_callback=*/base::NullCallback());

  process_run_loop2.Run();
  backend->RunUntilAllTasksCompleteForTest();

  // entry1 should now be processed.
  EXPECT_EQ(backend->GetSharedCacheEligibleEntriesCountForTest(), 0u);
  VerifySharedCacheResourceIdExists(backend.get(), kKey1);
}

TEST_F(SqlBackendImplSharedCacheTest,
       OnEntryEligibleForSharedCacheUpdateWithNewerResponseTime) {
  auto backend = CreateBackendAndInit();
  const std::string kKey = "0/0/https://example.test";
  const GURL kUrl("https://example.test");
  const net::SchemefulSite kSite(GURL("https://example.test"));
  const net::NetworkIsolationKey kNik(kSite, kSite);

  base::Time now = base::Time::Now();

  // Register an entry initially.
  {
    auto response_info = std::make_unique<net::HttpResponseInfo>();
    response_info->response_time = now;
    response_info->headers =
        base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\n\n");
    backend->OnEntryEligibleForSharedCache(kKey, kUrl, std::move(response_info),
                                           kNik);
  }
  EXPECT_EQ(backend->GetSharedCacheEligibleEntriesCountForTest(), 1u);
  {
    const auto& map = backend->GetSharedCacheEligibleEntriesForTest();
    auto it = map.find(CacheEntryKey(kKey));
    ASSERT_NE(it, map.end());
    EXPECT_EQ(it->second.response_info->response_time, now);
  }

  // Register the same key with an older response_time. It should be ignored.
  {
    auto response_info = std::make_unique<net::HttpResponseInfo>();
    response_info->response_time = now - base::Seconds(10);
    response_info->headers =
        base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\n\n");
    backend->OnEntryEligibleForSharedCache(kKey, kUrl, std::move(response_info),
                                           kNik);
  }
  EXPECT_EQ(backend->GetSharedCacheEligibleEntriesCountForTest(), 1u);
  {
    const auto& map = backend->GetSharedCacheEligibleEntriesForTest();
    auto it = map.find(CacheEntryKey(kKey));
    ASSERT_NE(it, map.end());
    EXPECT_EQ(it->second.response_info->response_time, now);
  }

  // Register the same key with a newer response_time. It should execute the if
  // branch and update the entry.
  base::Time newer = now + base::Seconds(10);
  {
    auto response_info = std::make_unique<net::HttpResponseInfo>();
    response_info->response_time = newer;
    response_info->headers =
        base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\n\n");
    backend->OnEntryEligibleForSharedCache(kKey, kUrl, std::move(response_info),
                                           kNik);
  }
  EXPECT_EQ(backend->GetSharedCacheEligibleEntriesCountForTest(), 1u);
  {
    const auto& map = backend->GetSharedCacheEligibleEntriesForTest();
    auto it = map.find(CacheEntryKey(kKey));
    ASSERT_NE(it, map.end());
    EXPECT_EQ(it->second.response_info->response_time, newer);
  }
}

TEST_F(SqlBackendImplSharedCacheTest,
       OnProcessSharedCacheEligibleEntriesCompleteReinsertsUnprocessedEntries) {
  auto backend = CreateBackendAndInit();

  const std::string kKey1 = "0/0/https://example1.test";
  const std::string kKey2 = "0/0/https://example2.test";
  const std::string kKey3 = "0/0/https://example3.test";

  const net::SchemefulSite kSite1(GURL("https://site1.test"));
  const net::NetworkIsolationKey kNik1(kSite1, kSite1);
  const net::SchemefulSite kSite2(GURL("https://site2.test"));
  const net::NetworkIsolationKey kNik2(kSite2, kSite2);

  // kKey1 and kKey2 share kNik1; kKey3 uses kNik2.
  CreateAndRegisterSharedCacheEntry(backend.get(), kKey1, "Data 1", kNik1);
  CreateAndRegisterSharedCacheEntry(backend.get(), kKey2, "Data 2", kNik1);
  CreateAndRegisterSharedCacheEntry(backend.get(), kKey3, "Data 3", kNik2);
  backend->RunUntilAllTasksCompleteForTest();

  EXPECT_EQ(backend->GetSharedCacheEligibleEntriesCountForTest(), 3u);

  std::unique_ptr<OperationHandle> in_flight_handle;

  base::RunLoop process_run_loop;
  // When kKey1 finishes copying, post a normal operation so that CopyEntries
  // aborts remaining entries in the same NIK group (kKey2) and subsequent NIK
  // groups (kKey3).
  backend->ProcessSharedCacheEligibleEntriesForTest(
      base::ScopedClosureRunner(process_run_loop.QuitClosure()),
      base::BindRepeating(
          [](SqlBackendImpl* backend,
             std::unique_ptr<OperationHandle>* handle_out,
             const CacheEntryKey& key) {
            if (!*handle_out) {
              backend->GetExclusiveOperationCoordinatorForTest()
                  ->PostOrRunNormalOperation(
                      CacheEntryKey("fake_key_for_abort"),
                      base::BindOnce(
                          [](std::unique_ptr<OperationHandle>* out,
                             std::unique_ptr<OperationHandle> handle) {
                            *out = std::move(handle);
                          },
                          handle_out),
                      /*low_priority=*/false);
            }
          },
          backend.get(), &in_flight_handle));

  process_run_loop.Run();

  // kKey1 was processed; kKey2 (same NIK) and kKey3 (different NIK) were
  // aborted and re-inserted.
  EXPECT_EQ(backend->GetSharedCacheEligibleEntriesCountForTest(), 2u);

  // Release normal operation so backend becomes idle again.
  in_flight_handle.reset();

  base::RunLoop process_run_loop2;
  backend->ProcessAllSharedCacheEligibleEntriesForTest(
      base::ScopedClosureRunner(process_run_loop2.QuitClosure()));
  process_run_loop2.Run();
  backend->RunUntilAllTasksCompleteForTest();

  // All entries should now be in shared cache.
  EXPECT_EQ(backend->GetSharedCacheEligibleEntriesCountForTest(), 0u);
  VerifySharedCacheResourceIdExists(backend.get(), kKey1);
  VerifySharedCacheResourceIdExists(backend.get(), kKey2);
  VerifySharedCacheResourceIdExists(backend.get(), kKey3);
}

TEST_F(SqlBackendImplSharedCacheTest,
       OnProcessSharedCacheEligibleEntriesCompleteConflictResponseTime) {
  auto backend = CreateBackendAndInit();

  const std::string kKey1 = "0/0/https://example1.test";
  const std::string kKey2 = "0/0/https://example2.test";
  const net::SchemefulSite kSite1(GURL("https://site1.test"));
  const net::NetworkIsolationKey kNik1(kSite1, kSite1);
  const net::SchemefulSite kSite2(GURL("https://site2.test"));
  const net::NetworkIsolationKey kNik2(kSite2, kSite2);

  base::Time now = base::Time::Now();
  base::Time t1 = now;

  CreateAndRegisterSharedCacheEntry(backend.get(), kKey1, "Data 1", kNik1, t1);
  CreateAndRegisterSharedCacheEntry(backend.get(), kKey2, "Data 2", kNik2, t1);
  backend->RunUntilAllTasksCompleteForTest();

  std::unique_ptr<OperationHandle> in_flight_handle;
  base::Time t0 = now - base::Seconds(10);

  base::RunLoop process_run_loop;
  backend->ProcessSharedCacheEligibleEntriesForTest(
      base::ScopedClosureRunner(process_run_loop.QuitClosure()),
      base::BindRepeating(
          [](SqlBackendImpl* backend,
             std::unique_ptr<OperationHandle>* handle_out,
             const std::string& key2, const GURL& url,
             const net::NetworkIsolationKey& nik, base::Time t0,
             const CacheEntryKey& key) {
            if (!*handle_out) {
              backend->GetExclusiveOperationCoordinatorForTest()
                  ->PostOrRunNormalOperation(
                      CacheEntryKey("fake_key_for_abort"),
                      base::BindOnce(
                          [](std::unique_ptr<OperationHandle>* out,
                             std::unique_ptr<OperationHandle> handle) {
                            *out = std::move(handle);
                          },
                          handle_out),
                      /*low_priority=*/false);
              auto info = std::make_unique<net::HttpResponseInfo>();
              info->response_time = t0;
              info->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
                  "HTTP/1.1 200 OK\n\n");
              backend->OnEntryEligibleForSharedCache(key2, url, std::move(info),
                                                     nik);
            }
          },
          backend.get(), &in_flight_handle, kKey2,
          GURL(net::HttpCache::GetResourceURLFromHttpCacheKey(kKey2)), kNik2,
          t0));

  process_run_loop.Run();
  in_flight_handle.reset();
  backend->RunUntilAllTasksCompleteForTest();

  // Unprocessed entry t1 for kKey2 is newer than t0, so t1 overwrites t0.
  const auto& map1 = backend->GetSharedCacheEligibleEntriesForTest();
  auto it1 = map1.find(CacheEntryKey(kKey2));
  ASSERT_TRUE(it1 != map1.end());
  EXPECT_EQ(it1->second.response_info->response_time, t1);

  // 2. Test t2 > t1 case for kKey2.
  base::Time t2 = now + base::Seconds(10);

  // Register a new entry kKey3 instead of re-registering kKey1, because
  // kKey1 was already copied to the shared cache and will be skipped.
  const std::string kKey3 = "0/0/https://example3.test";
  CreateAndRegisterSharedCacheEntry(backend.get(), kKey3, "Data 3", kNik1, t1);

  base::RunLoop process_run_loop2;
  backend->ProcessSharedCacheEligibleEntriesForTest(
      base::ScopedClosureRunner(process_run_loop2.QuitClosure()),
      base::BindRepeating(
          [](SqlBackendImpl* backend,
             std::unique_ptr<OperationHandle>* handle_out,
             const std::string& key2, const GURL& url,
             const net::NetworkIsolationKey& nik, base::Time t2,
             const CacheEntryKey& key) {
            if (!*handle_out) {
              backend->GetExclusiveOperationCoordinatorForTest()
                  ->PostOrRunNormalOperation(
                      CacheEntryKey("fake_key_for_abort"),
                      base::BindOnce(
                          [](std::unique_ptr<OperationHandle>* out,
                             std::unique_ptr<OperationHandle> handle) {
                            *out = std::move(handle);
                          },
                          handle_out),
                      /*low_priority=*/false);
              auto info = std::make_unique<net::HttpResponseInfo>();
              info->response_time = t2;
              info->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
                  "HTTP/1.1 200 OK\n\n");
              backend->OnEntryEligibleForSharedCache(key2, url, std::move(info),
                                                     nik);
            }
          },
          backend.get(), &in_flight_handle, kKey2,
          GURL(net::HttpCache::GetResourceURLFromHttpCacheKey(kKey2)), kNik2,
          t2));

  process_run_loop2.Run();
  in_flight_handle.reset();
  backend->RunUntilAllTasksCompleteForTest();

  const auto& map2 = backend->GetSharedCacheEligibleEntriesForTest();
  auto it2 = map2.find(CacheEntryKey(kKey2));
  ASSERT_TRUE(it2 != map2.end());
  EXPECT_EQ(it2->second.response_info->response_time, t2);
}

TEST_F(SqlBackendImplSharedCacheTest,
       ProcessAllSharedCacheEligibleEntriesForTest) {
  auto backend = CreateBackendAndInit();

  const std::string kKey1 = "0/0/https://example1.test";
  const std::string kKey2 = "0/0/https://example2.test";
  const net::SchemefulSite kSite1(GURL("https://site1.test"));
  const net::NetworkIsolationKey kNik1(kSite1, kSite1);
  const net::SchemefulSite kSite2(GURL("https://site2.test"));
  const net::NetworkIsolationKey kNik2(kSite2, kSite2);

  CreateAndRegisterSharedCacheEntry(backend.get(), kKey1, "Data 1", kNik1);
  CreateAndRegisterSharedCacheEntry(backend.get(), kKey2, "Data 2", kNik2);
  backend->RunUntilAllTasksCompleteForTest();

  auto verify_count_task = base::BindOnce(
      [](SqlBackendImpl* backend, std::unique_ptr<OperationHandle> handle) {
        // While `handle` is held, pass 1 has finished and re-inserted the
        // aborted entry (kKey2) into the queue.
        EXPECT_EQ(backend->GetSharedCacheEligibleEntriesCountForTest(), 1u);
      },
      backend.get());

  auto on_normal_op = base::BindOnce(
      [](base::OnceCallback<void(std::unique_ptr<OperationHandle>)> task,
         std::unique_ptr<OperationHandle> handle) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(task), std::move(handle)));
      },
      std::move(verify_count_task));

  using NormalOpCallback =
      base::OnceCallback<void(std::unique_ptr<OperationHandle>)>;
  auto on_normal_op_holder =
      std::make_unique<NormalOpCallback>(std::move(on_normal_op));

  auto on_entry_copied = base::BindRepeating(
      [](SqlBackendImpl* backend, NormalOpCallback* on_normal_op_holder,
         const CacheEntryKey& key) {
        if (*on_normal_op_holder) {
          backend->GetExclusiveOperationCoordinatorForTest()
              ->PostOrRunNormalOperation(CacheEntryKey("fake_key_for_abort"),
                                         std::move(*on_normal_op_holder),
                                         /*low_priority=*/false);
        }
      },
      backend.get(), base::Owned(on_normal_op_holder.release()));

  base::RunLoop process_run_loop;
  backend->ProcessAllSharedCacheEligibleEntriesWithCallbackForTest(
      base::ScopedClosureRunner(process_run_loop.QuitClosure()),
      std::move(on_entry_copied));

  process_run_loop.Run();

  EXPECT_EQ(backend->GetSharedCacheEligibleEntriesCountForTest(), 0u);
  VerifySharedCacheResourceIdExists(backend.get(), kKey1);
  VerifySharedCacheResourceIdExists(backend.get(), kKey2);
}

TEST_F(SqlBackendImplSharedCacheTest, BackendInterfaceSupportsSharedCache) {
  auto backend = CreateBackendAndInit();
  disk_cache::Backend* base_backend = backend.get();
  EXPECT_TRUE(base_backend->SupportsSharedCache());
}

TEST_F(SqlBackendImplSharedCacheTest,
       BackendInterfaceProcessAllSharedCacheEligibleEntries) {
  auto backend = CreateBackendAndInit();
  disk_cache::Backend* base_backend = backend.get();

  const std::string kKey = "0/0/https://example.com/test";
  const net::SchemefulSite kSite(GURL("https://example.com"));
  const net::NetworkIsolationKey kNik(kSite, kSite);

  CreateAndRegisterSharedCacheEntry(backend.get(), kKey, "test data", kNik);

  base::RunLoop run_loop;
  base_backend->ProcessAllSharedCacheEligibleEntriesForTest(
      base::ScopedClosureRunner(run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_EQ(backend->GetSharedCacheEligibleEntriesCountForTest(), 0u);
  VerifySharedCacheResourceIdExists(backend.get(), kKey);
}

TEST_F(SqlBackendImplSharedCacheTest,
       ProcessAllSharedCacheEligibleEntriesWithActiveEntry) {
  auto backend = CreateBackendAndInit();
  const std::string kKey = "0/0/https://example.com/active";
  const net::SchemefulSite kSite(GURL("https://example.com"));
  const net::NetworkIsolationKey kNik(kSite, kSite);

  // 1. Create and populate an entry, but do not close it so it remains active.
  disk_cache::Entry* entry = CreateAndRegisterSharedCacheEntry(
      backend.get(), kKey, "test data", kNik, base::Time::Now(),
      /*close_entry=*/false);

  // 2. Post a task to close the entry shortly after.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&disk_cache::Entry::Close, base::Unretained(entry)));

  // 3. Trigger ProcessAllSharedCacheEligibleEntriesForTest. Since the entry is
  // active initially, it is skipped. The posted asynchronous retry allows the
  // pending Close task to execute first, so the entry is processed on the next
  // attempt.
  base::RunLoop process_run_loop;
  backend->ProcessAllSharedCacheEligibleEntriesForTest(
      base::ScopedClosureRunner(process_run_loop.QuitClosure()));
  process_run_loop.Run();
  backend->RunUntilAllTasksCompleteForTest();

  EXPECT_EQ(backend->GetSharedCacheEligibleEntriesCountForTest(), 0u);
  VerifySharedCacheResourceIdExists(backend.get(), kKey);
}

class SqlBackendImplSharedCacheWriteTest
    : public SqlBackendImplSharedCacheTest,
      public testing::WithParamInterface<bool> {
 public:
  SqlBackendImplSharedCacheWriteTest()
      : kSite(GURL("https://example.com")), kNik(kSite, kSite) {}

  static std::string DescribeParams(const testing::TestParamInfo<bool>& info) {
    return info.param ? "WithOtherEntry" : "SingleEntry";
  }

 protected:
  static constexpr char kTargetKey[] = "0/0/https://example.com/";
  static constexpr char kOtherKey[] = "0/0/https://example.com/2";

  const net::SchemefulSite kSite;
  const net::NetworkIsolationKey kNik;

  bool WithOtherEntry() const { return GetParam(); }

  void SetupSharedCacheEntries(
      std::unique_ptr<SqlBackendImpl>& backend,
      const std::string& data,
      std::optional<SqlSharedCacheDbId>& out_db_id,
      std::optional<SqlSharedCacheRowId>& out_row_id1,
      std::optional<SqlSharedCacheRowId>& out_row_id2) {
    // 1. Create first entry
    auto* entry = CreateEntryAndWriteData(backend.get(), kTargetKey, data);

    base::Time response_time = base::Time::Now();

    // Write headers for the first entry
    net::HttpResponseInfo response_info_for_pickle;
    response_info_for_pickle.response_time = response_time;
    response_info_for_pickle.headers =
        base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\0\0");
    std::unique_ptr<base::Pickle> pickle =
        response_info_for_pickle.MakePickle(false, false);
    std::string pickle_data(reinterpret_cast<const char*>(pickle->data()),
                            pickle->size());
    auto pickle_buffer = base::MakeRefCounted<net::StringIOBuffer>(pickle_data);
    net::TestCompletionCallback cb_write_pickle;
    EXPECT_EQ(cb_write_pickle.GetResult(
                  entry->WriteData(0, 0, pickle_buffer.get(), pickle->size(),
                                   cb_write_pickle.callback(), false)),
              static_cast<int>(pickle->size()));
    entry->Close();

    // 2. Make it eligible
    auto response_info = std::make_unique<net::HttpResponseInfo>();
    response_info->response_time = response_time;
    response_info->headers =
        base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\0\0");
    backend->OnEntryEligibleForSharedCache(kTargetKey,
                                           GURL("https://example.com/"),
                                           std::move(response_info), kNik);

    // If we want another entry in the same shared cache (same NIK)
    if (WithOtherEntry()) {
      FastForwardBy(base::Seconds(1));
      base::Time response_time2 = base::Time::Now();

      auto* entry2 = CreateEntryAndWriteData(backend.get(), kOtherKey, data);

      net::HttpResponseInfo response_info_for_pickle2;
      response_info_for_pickle2.response_time = response_time2;
      response_info_for_pickle2.headers =
          base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\0\0");
      std::unique_ptr<base::Pickle> pickle2 =
          response_info_for_pickle2.MakePickle(false, false);
      std::string pickle_data2(reinterpret_cast<const char*>(pickle2->data()),
                               pickle2->size());
      auto pickle_buffer2 =
          base::MakeRefCounted<net::StringIOBuffer>(pickle_data2);

      net::TestCompletionCallback cb_write_pickle2;
      EXPECT_EQ(cb_write_pickle2.GetResult(entry2->WriteData(
                    0, 0, pickle_buffer2.get(), pickle2->size(),
                    cb_write_pickle2.callback(), false)),
                static_cast<int>(pickle2->size()));
      entry2->Close();

      auto response_info2 = std::make_unique<net::HttpResponseInfo>();
      response_info2->response_time = response_time2;
      response_info2->headers =
          base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\0\0");
      backend->OnEntryEligibleForSharedCache(kOtherKey,
                                             GURL("https://example.com/2"),
                                             std::move(response_info2), kNik);
    }

    base::RunLoop process_run_loop;
    backend->ProcessAllSharedCacheEligibleEntriesForTest(
        base::ScopedClosureRunner(process_run_loop.QuitClosure()));
    process_run_loop.Run();
    backend->RunUntilAllTasksCompleteForTest();

    // After processing, re-open the entries to capture the row IDs.
    TestEntryResultCompletionCallback cb_open;
    disk_cache::EntryResult open_result = cb_open.GetResult(
        backend->OpenEntry(kTargetKey, net::HIGHEST, cb_open.callback()));
    ASSERT_EQ(open_result.net_error(), net::OK);
    auto* opened_entry = open_result.ReleaseEntry();

    auto* sql_entry = static_cast<SqlEntryImpl*>(opened_entry);
    auto shared_resource = sql_entry->db_handle()->shared_cache_resource_id();
    ASSERT_TRUE(shared_resource);
    out_db_id = shared_resource->db_id;
    out_row_id1 = shared_resource->row_id;
    opened_entry->Close();

    if (WithOtherEntry()) {
      TestEntryResultCompletionCallback cb_open2;
      disk_cache::EntryResult open_result2 = cb_open2.GetResult(
          backend->OpenEntry(kOtherKey, net::HIGHEST, cb_open2.callback()));
      ASSERT_EQ(open_result2.net_error(), net::OK);
      auto* opened_entry2 = open_result2.ReleaseEntry();

      auto* sql_entry2 = static_cast<SqlEntryImpl*>(opened_entry2);
      auto shared_resource2 =
          sql_entry2->db_handle()->shared_cache_resource_id();
      ASSERT_TRUE(shared_resource2);
      out_row_id2 = shared_resource2->row_id;
      opened_entry2->Close();
    }
    backend->RunUntilAllTasksCompleteForTest();
  }

  void VerifySharedCacheDeleted(SqlBackendImpl* backend,
                                std::optional<SqlSharedCacheDbId> db_id,
                                std::optional<SqlSharedCacheRowId> row_id) {
    backend->RunUntilAllTasksCompleteForTest();

    base::FilePath file_path =
        temp_dir_.GetPath()
            .AppendASCII(
                base::StrCat({kSqlBackendSharedCacheIsolatedFileNamePrefix,
                              base::NumberToString(db_id->value())}))
            .AddExtension(FILE_PATH_LITERAL(".db"));

    if (WithOtherEntry()) {
      // DB should still exist
      EXPECT_TRUE(base::PathExists(file_path));

      // Verify that the target entry (key) does not exist.
      {
        base::test::TestFuture<SqlPersistentStore::EntryInfoOrError> future;
        backend->GetSqlStoreForTest()->OpenEntry(CacheEntryKey(kTargetKey),
                                                 future.GetCallback());
        auto result = future.Take();
        EXPECT_FALSE(result.has_value());
      }

      // The other remaining entry (key + "2") should still be in shared cache.
      {
        base::test::TestFuture<SqlPersistentStore::EntryInfoOrError> future;
        backend->GetSqlStoreForTest()->OpenEntry(CacheEntryKey(kOtherKey),
                                                 future.GetCallback());
        auto result = future.Take();
        ASSERT_TRUE(result.has_value());
        EXPECT_TRUE(result->shared_cache_resource_id.has_value());
      }
    } else {
      // Whole DB should be gone
      EXPECT_TRUE(
          base::test::RunUntil([&]() { return !base::PathExists(file_path); }));
    }
    backend->RunUntilAllTasksCompleteForTest();
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         SqlBackendImplSharedCacheWriteTest,
                         testing::Bool(),
                         SqlBackendImplSharedCacheWriteTest::DescribeParams);

TEST_P(SqlBackendImplSharedCacheWriteTest,
       WriteDataInternalOffsetZeroTruncate) {
  auto backend = CreateBackendAndInit();

  std::optional<SqlSharedCacheDbId> db_id;
  std::optional<SqlSharedCacheRowId> row_id1;
  std::optional<SqlSharedCacheRowId> row_id2;

  SetupSharedCacheEntries(backend, "Data", db_id, row_id1, row_id2);

  // Open the entry again
  TestEntryResultCompletionCallback cb_open;
  disk_cache::EntryResult open_result = cb_open.GetResult(
      backend->OpenEntry(kTargetKey, net::HIGHEST, cb_open.callback()));
  ASSERT_EQ(open_result.net_error(), net::OK);
  auto* entry = open_result.ReleaseEntry();

  // WriteData at offset 0 and truncate
  auto buffer = base::MakeRefCounted<net::StringIOBuffer>("New Data");
  net::TestCompletionCallback cb_write;
  EXPECT_EQ(cb_write.GetResult(entry->WriteData(
                1, 0, buffer.get(), buffer->size(), cb_write.callback(), true)),
            static_cast<int>(buffer->size()));

  entry->Close();

  VerifySharedCacheDeleted(backend.get(), db_id, row_id1);
}

TEST_P(SqlBackendImplSharedCacheWriteTest, WriteDataInternalCopySmall) {
  auto backend = CreateBackendAndInit();

  std::optional<SqlSharedCacheDbId> db_id;
  std::optional<SqlSharedCacheRowId> row_id1;
  std::optional<SqlSharedCacheRowId> row_id2;

  // Data < 16KB
  SetupSharedCacheEntries(backend, "Small Data", db_id, row_id1, row_id2);

  TestEntryResultCompletionCallback cb_open;
  disk_cache::EntryResult open_result = cb_open.GetResult(
      backend->OpenEntry(kTargetKey, net::HIGHEST, cb_open.callback()));
  ASSERT_EQ(open_result.net_error(), net::OK);
  auto* entry = open_result.ReleaseEntry();

  // WriteData NOT offset 0 && truncate
  auto buffer = base::MakeRefCounted<net::StringIOBuffer>("Append");
  net::TestCompletionCallback cb_write;
  EXPECT_EQ(
      cb_write.GetResult(entry->WriteData(1, 5, buffer.get(), buffer->size(),
                                          cb_write.callback(), false)),
      static_cast<int>(buffer->size()));

  const std::string expected_data = "SmallAppend";
  EXPECT_EQ(entry->GetDataSize(1), static_cast<int>(expected_data.size()));
  auto read_buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(expected_data.size());
  net::TestCompletionCallback cb_read;
  EXPECT_EQ(
      cb_read.GetResult(entry->ReadData(
          1, 0, read_buffer.get(), read_buffer->size(), cb_read.callback())),
      static_cast<int>(expected_data.size()));
  EXPECT_EQ(std::string_view(read_buffer->data(), read_buffer->size()),
            expected_data);

  entry->Close();

  VerifySharedCacheDeleted(backend.get(), db_id, row_id1);
}

TEST_P(SqlBackendImplSharedCacheWriteTest, WriteDataInternalCopyLarge) {
  auto backend = CreateBackendAndInit();

  std::optional<SqlSharedCacheDbId> db_id;
  std::optional<SqlSharedCacheRowId> row_id1;
  std::optional<SqlSharedCacheRowId> row_id2;

  // Data > 16KB to trigger multiple copy iterations
  std::string large_data(20 * 1024, '\0');
  for (size_t i = 0; i < large_data.size(); ++i) {
    large_data[i] = static_cast<char>('a' + (i % 26));
  }
  SetupSharedCacheEntries(backend, large_data, db_id, row_id1, row_id2);

  TestEntryResultCompletionCallback cb_open;
  disk_cache::EntryResult open_result = cb_open.GetResult(
      backend->OpenEntry(kTargetKey, net::HIGHEST, cb_open.callback()));
  ASSERT_EQ(open_result.net_error(), net::OK);
  auto* entry = open_result.ReleaseEntry();

  // WriteData NOT offset 0 && truncate
  auto buffer = base::MakeRefCounted<net::StringIOBuffer>("Append");
  net::TestCompletionCallback cb_write;
  EXPECT_EQ(cb_write.GetResult(entry->WriteData(1, large_data.size(),
                                                buffer.get(), buffer->size(),
                                                cb_write.callback(), false)),
            static_cast<int>(buffer->size()));

  const std::string expected_large_data = large_data + "Append";
  EXPECT_EQ(entry->GetDataSize(1),
            static_cast<int>(expected_large_data.size()));
  auto read_buffer_large =
      base::MakeRefCounted<net::IOBufferWithSize>(expected_large_data.size());
  net::TestCompletionCallback cb_read_large;
  EXPECT_EQ(cb_read_large.GetResult(entry->ReadData(
                1, 0, read_buffer_large.get(), read_buffer_large->size(),
                cb_read_large.callback())),
            static_cast<int>(expected_large_data.size()));
  EXPECT_EQ(
      std::string_view(read_buffer_large->data(), read_buffer_large->size()),
      expected_large_data);

  entry->Close();

  VerifySharedCacheDeleted(backend.get(), db_id, row_id1);
}

TEST_P(SqlBackendImplSharedCacheWriteTest,
       CopySharedCacheToBlobTableInFlightModification) {
  auto backend = CreateBackendAndInit();

  std::optional<SqlSharedCacheDbId> db_id;
  std::optional<SqlSharedCacheRowId> row_id1;
  std::optional<SqlSharedCacheRowId> row_id2;

  SetupSharedCacheEntries(backend, "Small Data", db_id, row_id1, row_id2);

  TestEntryResultCompletionCallback cb_open;
  disk_cache::EntryResult open_result = cb_open.GetResult(
      backend->OpenEntry(kTargetKey, net::HIGHEST, cb_open.callback()));
  ASSERT_EQ(open_result.net_error(), net::OK);
  auto* entry = open_result.ReleaseEntry();

  // Post `OpenNextEntry` on the iterator beforehand.
  // Then calling `WriteData` creates a scenario where the iterator's DB record
  // lookup and entry instantiation happen asynchronously while the background
  // copy operation (`CopySharedCacheToBlobTableAndWrite`) is in progress.
  auto iter = backend->CreateIterator();
  TestEntryResultCompletionCallback cb_next1;
  EntryResult result_iter = iter->OpenNextEntry(cb_next1.callback());

  auto buffer = base::MakeRefCounted<net::StringIOBuffer>("Append");
  base::test::TestFuture<int> write_future;
  std::ignore = entry->WriteData(1, 10, buffer.get(), buffer->size(),
                                 write_future.GetCallback(), false);
  entry->Close();

  // Retrieve the result of the iterator's open operation.
  result_iter = cb_next1.GetResult(std::move(result_iter));
  ASSERT_THAT(result_iter.net_error(), IsOk());
  entry = result_iter.ReleaseEntry();
  // When WithOtherEntry() is true, another entry in the shared cache might be
  // returned first, so advance the iterator until we reach `kTargetKey`.
  if (entry->GetKey() != kTargetKey) {
    entry->Close();
    TestEntryResultCompletionCallback cb_next2;
    result_iter = iter->OpenNextEntry(cb_next2.callback());
    result_iter = cb_next2.GetResult(std::move(result_iter));
    ASSERT_THAT(result_iter.net_error(), IsOk());
    entry = result_iter.ReleaseEntry();
  }
  EXPECT_EQ(entry->GetKey(), std::string(kTargetKey));

  // Verify that `InFlightEntryModification` is preserved and applied during
  // `CopySharedCacheToBlobTableAndWrite`, so the `body_end` of the entry opened
  // by the iterator reflects the written data size (10 + 6 = 16).
  EXPECT_EQ(entry->GetDataSize(1), 16);

  entry->Close();

  EXPECT_EQ(write_future.Get(), static_cast<int>(buffer->size()));

  VerifySharedCacheDeleted(backend.get(), db_id, row_id1);
}

TEST_P(SqlBackendImplSharedCacheWriteTest,
       CopySharedCacheToBlobTableAbortOnBackendDestruction) {
  auto backend = CreateBackendAndInit();

  std::optional<SqlSharedCacheDbId> db_id;
  std::optional<SqlSharedCacheRowId> row_id1;
  std::optional<SqlSharedCacheRowId> row_id2;

  SetupSharedCacheEntries(backend, "Small Data", db_id, row_id1, row_id2);

  TestEntryResultCompletionCallback cb_open;
  disk_cache::EntryResult open_result = cb_open.GetResult(
      backend->OpenEntry(kTargetKey, net::HIGHEST, cb_open.callback()));
  ASSERT_EQ(open_result.net_error(), net::OK);
  auto* entry = open_result.ReleaseEntry();

  auto buffer = base::MakeRefCounted<net::StringIOBuffer>("Append");
  base::test::TestFuture<int> write_future;
  std::ignore = entry->WriteData(1, 5, buffer.get(), buffer->size(),
                                 write_future.GetCallback(), false);

  entry->Close();
  backend.reset();

  EXPECT_EQ(write_future.Get(), net::ERR_ABORTED);
}

TEST_P(SqlBackendImplSharedCacheWriteTest,
       CopySharedCacheToBlobTableDbHandleHasError) {
  auto backend = CreateBackendAndInit();

  std::optional<SqlSharedCacheDbId> db_id;
  std::optional<SqlSharedCacheRowId> row_id1;
  std::optional<SqlSharedCacheRowId> row_id2;

  SetupSharedCacheEntries(backend, "Small Data", db_id, row_id1, row_id2);

  TestEntryResultCompletionCallback cb_open;
  disk_cache::EntryResult open_result = cb_open.GetResult(
      backend->OpenEntry(kTargetKey, net::HIGHEST, cb_open.callback()));
  ASSERT_EQ(open_result.net_error(), net::OK);
  auto* entry = open_result.ReleaseEntry();

  auto* sql_entry = static_cast<SqlEntryImpl*>(entry);
  sql_entry->db_handle()->MarkAsErrorOccurred(
      SqlPersistentStore::Error::kFailedForTesting);

  auto buffer = base::MakeRefCounted<net::StringIOBuffer>("Append");
  net::TestCompletionCallback cb_write;
  EXPECT_EQ(
      cb_write.GetResult(entry->WriteData(1, 5, buffer.get(), buffer->size(),
                                          cb_write.callback(), false)),
      net::ERR_FAILED);

  entry->Close();
  backend->RunUntilAllTasksCompleteForTest();
}

TEST_P(SqlBackendImplSharedCacheWriteTest,
       CopySharedCacheToBlobTableReadFromSharedCacheFailure) {
  auto backend = CreateBackendAndInit();

  std::optional<SqlSharedCacheDbId> db_id;
  std::optional<SqlSharedCacheRowId> row_id1;
  std::optional<SqlSharedCacheRowId> row_id2;

  SetupSharedCacheEntries(backend, "Small Data", db_id, row_id1, row_id2);

  TestEntryResultCompletionCallback cb_open;
  disk_cache::EntryResult open_result = cb_open.GetResult(
      backend->OpenEntry(kTargetKey, net::HIGHEST, cb_open.callback()));
  ASSERT_EQ(open_result.net_error(), net::OK);
  auto* entry = open_result.ReleaseEntry();

  // Set invalid shared_cache_resource_id so ReadFromSharedCache fails with
  // kNotFound.
  auto* sql_entry = static_cast<SqlEntryImpl*>(entry);
  sql_entry->db_handle()->set_shared_cache_resource_id(SqlSharedCacheResourceId(
      SqlSharedCacheDbId(99999), SqlSharedCacheRowId(99999)));

  auto buffer = base::MakeRefCounted<net::StringIOBuffer>("Append");
  net::TestCompletionCallback cb_write;
  EXPECT_EQ(
      cb_write.GetResult(entry->WriteData(1, 5, buffer.get(), buffer->size(),
                                          cb_write.callback(), false)),
      net::ERR_FAILED);
  EXPECT_TRUE(sql_entry->db_handle()->GetError().has_value());

  entry->Close();
  backend->RunUntilAllTasksCompleteForTest();
}

TEST_P(SqlBackendImplSharedCacheWriteTest,
       CopySharedCacheToBlobTableWriteToBlobTableFailure) {
  auto backend = CreateBackendAndInit();

  std::optional<SqlSharedCacheDbId> db_id;
  std::optional<SqlSharedCacheRowId> row_id1;
  std::optional<SqlSharedCacheRowId> row_id2;

  SetupSharedCacheEntries(backend, "Small Data", db_id, row_id1, row_id2);

  TestEntryResultCompletionCallback cb_open;
  disk_cache::EntryResult open_result = cb_open.GetResult(
      backend->OpenEntry(kTargetKey, net::HIGHEST, cb_open.callback()));
  ASSERT_EQ(open_result.net_error(), net::OK);
  auto* entry = open_result.ReleaseEntry();

  // Force store to simulate DB failure so WriteEntryData fails.
  backend->GetSqlStoreForTest()->SetSimulateDbFailureForTesting(true);

  auto buffer = base::MakeRefCounted<net::StringIOBuffer>("Append");
  net::TestCompletionCallback cb_write;
  EXPECT_EQ(
      cb_write.GetResult(entry->WriteData(1, 5, buffer.get(), buffer->size(),
                                          cb_write.callback(), false)),
      net::ERR_FAILED);
  auto* sql_entry = static_cast<SqlEntryImpl*>(entry);
  EXPECT_TRUE(sql_entry->db_handle()->GetError().has_value());

  entry->Close();
  backend->RunUntilAllTasksCompleteForTest();
}

}  // namespace
}  // namespace disk_cache
