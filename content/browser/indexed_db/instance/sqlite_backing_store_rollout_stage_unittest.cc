// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string_view>
#include <tuple>

#include "base/auto_reset.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "content/browser/indexed_db/indexed_db_data_format_version.h"
#include "content/browser/indexed_db/indexed_db_test_base.h"
#include "content/browser/indexed_db/instance/bucket_context.h"
#include "content/browser/indexed_db/instance/sqlite/database_connection.h"
#include "content/browser/indexed_db/mock_mojo_indexed_db_database_callbacks.h"
#include "content/browser/indexed_db/mock_mojo_indexed_db_factory_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content::indexed_db {

namespace {

constexpr char16_t kDatabaseName[] = u"db";

}  // namespace

class SqliteBackingStoreRolloutStageTest
    : public IndexedDBTestBase,
      public testing::WithParamInterface<bool> {
 public:
  SqliteBackingStoreRolloutStageTest()
      : IndexedDBTestBase(/*use_default_buckets=*/GetParam(),
                          /*use_sqlite=*/false) {}
  ~SqliteBackingStoreRolloutStageTest() override = default;

  void SetUp() override {
    buckets_[StoreType::kNone] = GetOrCreateBucket(
        blink::StorageKey::CreateFromStringForTesting("http://none.com"));

    {
      storage::BucketInfo bucket_info = GetOrCreateBucket(
          blink::StorageKey::CreateFromStringForTesting("http://leveldb.com"));
      auto [factory_remote, bucket_context] = BindFactoryAndOverrideStage(
          bucket_info, SqliteRolloutStage::kUseLevelDbOnly);
      CreateDatabase(factory_remote, kDatabaseName, /*transaction_id=*/1);
      buckets_[StoreType::kLevelDb] = bucket_info;
    }

    {
      storage::BucketInfo bucket_info = GetOrCreateBucket(
          blink::StorageKey::CreateFromStringForTesting("http://sqlite.com"));
      auto [factory_remote, bucket_context] = BindFactoryAndOverrideStage(
          bucket_info, SqliteRolloutStage::kUseSqliteOnly);
      GetBucketContext(bucket_info.id)
          ->SetSqliteRolloutStageForTesting(SqliteRolloutStage::kUseSqliteOnly);
      CreateDatabase(factory_remote, kDatabaseName, /*transaction_id=*/1);
      buckets_[StoreType::kSqlite] = bucket_info;
    }

    {
      storage::BucketInfo bucket_info =
          GetOrCreateBucket(blink::StorageKey::CreateFromStringForTesting(
              "http://empty_leveldb_directory.com"));
      base::FilePath leveldb_dir_path =
          GetFilePathForTesting(bucket_info.ToBucketLocator());
      ASSERT_TRUE(base::CreateDirectory(leveldb_dir_path));
      buckets_[StoreType::kEmptyLevelDbDirectory] = bucket_info;
    }

    {
      storage::BucketInfo bucket_info =
          GetOrCreateBucket(blink::StorageKey::CreateFromStringForTesting(
              "http://leveldb_with_corruption_info.com"));
      auto [factory_remote, bucket_context] = BindFactoryAndOverrideStage(
          bucket_info, SqliteRolloutStage::kUseLevelDbOnly);
      CreateDatabase(factory_remote, kDatabaseName, /*transaction_id=*/1);
      bucket_context->HandleBackingStoreCorruption("testing");
      buckets_[StoreType::kLevelDbWithCorruptionInfo] = bucket_info;
    }

    {
      storage::BucketInfo bucket_info =
          GetOrCreateBucket(blink::StorageKey::CreateFromStringForTesting(
              "http://leveldb_current_missing.com"));
      auto [factory_remote, bucket_context] = BindFactoryAndOverrideStage(
          bucket_info, SqliteRolloutStage::kUseLevelDbOnly);
      CreateDatabase(factory_remote, kDatabaseName, /*transaction_id=*/1);
      bucket_context->ForceClose(/*doom=*/false);
      base::FilePath current_file_path = GetLevelDbCurrentFilePath(bucket_info);
      ASSERT_TRUE(base::PathExists(current_file_path));
      ASSERT_TRUE(base::DeleteFile(current_file_path));
      buckets_[StoreType::kLevelDbCurrentMissing] = bucket_info;
    }

    {
      storage::BucketInfo bucket_info =
          GetOrCreateBucket(blink::StorageKey::CreateFromStringForTesting(
              "http://leveldb_files_missing.com"));
      auto [factory_remote, bucket_context] = BindFactoryAndOverrideStage(
          bucket_info, SqliteRolloutStage::kUseLevelDbOnly);
      CreateDatabase(factory_remote, kDatabaseName, /*transaction_id=*/1);
      bucket_context->ForceClose(/*doom=*/false);
      base::FilePath current_file_path = GetLevelDbCurrentFilePath(bucket_info);
      ASSERT_TRUE(base::PathExists(current_file_path));
      // Delete everything except the CURRENT file.
      base::FileEnumerator enumerator(current_file_path.DirName(),
                                      /*recursive=*/false,
                                      base::FileEnumerator::FILES);
      enumerator.ForEach([&](const base::FilePath& path) {
        if (path == current_file_path) {
          return;
        }
        ASSERT_TRUE(base::DeleteFile(path));
      });
      buckets_[StoreType::kLevelDbFilesMissing] = bucket_info;
    }

    {
      storage::BucketInfo bucket_info =
          GetOrCreateBucket(blink::StorageKey::CreateFromStringForTesting(
              "http://leveldb_internal_corruption.com"));
      auto [factory_remote, bucket_context] = BindFactoryAndOverrideStage(
          bucket_info, SqliteRolloutStage::kUseLevelDbOnly);
      CreateDatabase(factory_remote, kDatabaseName, /*transaction_id=*/1);
      bucket_context->ForceClose(/*doom=*/false);
      // Corrupt the MANIFEST-* files.
      base::FileEnumerator enumerator(
          GetFilePathForTesting(bucket_info.ToBucketLocator()),
          /*recursive=*/false, base::FileEnumerator::FILES,
          FILE_PATH_LITERAL("MANIFEST-*"));
      enumerator.ForEach([](const base::FilePath& path) {
        base::WriteFile(path, "corrupted");
      });
      buckets_[StoreType::kLevelDbInternalCorruption] = bucket_info;
    }

    {
      storage::BucketInfo bucket_info =
          GetOrCreateBucket(blink::StorageKey::CreateFromStringForTesting(
              "http://leveldb_backing_store_corruption.com"));
      auto [factory_remote, bucket_context] = BindFactoryAndOverrideStage(
          bucket_info, SqliteRolloutStage::kUseLevelDbOnly);
      // Create the backing store with a future data format version. When opened
      // with the real (current) version, the backing store will treat the
      // stored version as unknown and report corruption.
      base::AutoReset<IndexedDBDataFormatVersion> override_version(
          &IndexedDBDataFormatVersion::GetMutableCurrentForTesting(),
          IndexedDBDataFormatVersion(99, 99));
      CreateDatabase(factory_remote, kDatabaseName, /*transaction_id=*/1);
      buckets_[StoreType::kLevelDbBackingStoreCorruption] = bucket_info;
    }

    CloseAllBackingStores();
  }

 protected:
  enum class StoreType {
    kNone,
    kLevelDb,
    kSqlite,
    kEmptyLevelDbDirectory,
    // The LevelDB dir contains corruption info recorded by a prior instance.
    kLevelDbWithCorruptionInfo,
    // The LevelDB dir is missing the CURRENT file, which triggers internal
    // recovery but only when create_if_missing is true.
    kLevelDbCurrentMissing,
    // The LevelDB dir is missing files other than CURRENT, which due to
    // crbug.com/760362 surfaces as a persistent IO error and not corruption.
    kLevelDbFilesMissing,
    // Some LevelDB files are corrupted such that LevelDB code itself detects
    // and reports corruption.
    kLevelDbInternalCorruption,
    // The LevelDB backing store has unexpected content which is treated as
    // corruption at the backing store level in the first open attempt.
    kLevelDbBackingStoreCorruption,
  };

  enum class StoreInitResult { kOpened, kCreated, kDataLoss, kFailed };

  struct Expectation {
    StoreInitResult result;
    bool is_sqlite;
    std::string_view histogram_suffix = ".OnDisk";
  };

  static constexpr const bool kIsSqlite = true;
  static constexpr const bool kIsLevelDb = false;
  static constexpr const std::string_view kExperimentalSuffix = ".Experimental";

  // Some common expectations.
  const Expectation kCreatedWithSqlite{StoreInitResult::kCreated, kIsSqlite};
  const Expectation kCreatedWithLevelDb{StoreInitResult::kCreated, kIsLevelDb};
  const Expectation kOpenedLevelDb{StoreInitResult::kOpened, kIsLevelDb};

  void ValidateExpectationsForStage(
      SqliteRolloutStage stage,
      std::map<StoreType, Expectation> expectations) {
    ASSERT_EQ(expectations.size(), buckets_.size());
    for (const auto& [bucket_type, bucket_info] : buckets_) {
      LOG(INFO) << "Validating bucket " << bucket_info.storage_key.origin();
      base::HistogramTester histograms;
      auto [factory_remote, bucket_context] =
          BindFactoryAndOverrideStage(bucket_info, stage);
      auto [result, is_sqlite, histogram_suffix] = expectations[bucket_type];
      switch (result) {
        case StoreInitResult::kOpened:
          OpenDatabase(factory_remote, kDatabaseName, /*transaction_id=*/1);
          EXPECT_EQ(is_sqlite, bucket_context->IsUsingSqlite());
          histograms.ExpectUniqueSample(
              base::StrCat(
                  {"IndexedDB.BackingStore.CreateIfMissing", histogram_suffix}),
              0 /*Status::Type::kOk*/, 1);
          break;
        case StoreInitResult::kCreated:
          CreateDatabase(factory_remote, kDatabaseName,
                         /*transaction_id=*/1);
          EXPECT_EQ(is_sqlite, bucket_context->IsUsingSqlite());
          histograms.ExpectUniqueSample(
              base::StrCat(
                  {"IndexedDB.BackingStore.CreateIfMissing", histogram_suffix}),
              0 /*Status::Type::kOk*/, 1);
          break;
        case StoreInitResult::kDataLoss:
          CreateDatabase(factory_remote, kDatabaseName,
                         /*transaction_id=*/1,
                         blink::mojom::IDBDataLoss::Total);
          EXPECT_EQ(is_sqlite, bucket_context->IsUsingSqlite());
          histograms.ExpectUniqueSample(
              base::StrCat(
                  {"IndexedDB.BackingStore.CreateIfMissing", histogram_suffix}),
              0 /*Status::Type::kOk*/, 1);
          break;
        case StoreInitResult::kFailed: {
          base::RunLoop run_loop;
          MockMojoFactoryClient client;
          MockMojoDatabaseCallbacks database_callbacks;
          EXPECT_CALL(client, Error)
              .WillOnce(::base::test::RunClosure(run_loop.QuitClosure()));
          mojo::AssociatedRemote<blink::mojom::IDBTransaction>
              transaction_remote;
          factory_remote->Open(
              client.CreateInterfacePtrAndBind(),
              database_callbacks.CreateInterfacePtrAndBind(), kDatabaseName,
              /*version=*/1,
              transaction_remote.BindNewEndpointAndPassReceiver(),
              /*transaction_id=*/1, /*priority=*/0);
          run_loop.Run();
          histograms.ExpectTotalCount(
              base::StrCat(
                  {"IndexedDB.BackingStore.CreateIfMissing", histogram_suffix}),
              1);
          histograms.ExpectBucketCount(
              base::StrCat(
                  {"IndexedDB.BackingStore.CreateIfMissing", histogram_suffix}),
              0 /*Status::Type::kOk*/, 0);
          break;
        }
      }
    }
  }

  void CloseAllBackingStores() {
    // The SQLite store has an added delay between when an individual database
    // connection is dropped and when store shutdown is initiated.
    task_environment_.FastForwardBy(
        sqlite::DatabaseConnection::GetDestructionGracePeriodForTesting());
    task_environment_.FastForwardBy(
        BucketContext::GetBackingStoreGracePeriodForTesting());
  }

 private:
  std::tuple<mojo::Remote<blink::mojom::IDBFactory>, BucketContext*>
  BindFactoryAndOverrideStage(const storage::BucketInfo& bucket_info,
                              SqliteRolloutStage stage) {
    mojo::Remote<blink::mojom::IDBFactory> factory_remote;
    mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
        checker_remote;
    BindFactory(std::move(checker_remote),
                factory_remote.BindNewPipeAndPassReceiver(), bucket_info);
    BucketContext* bucket_context = GetBucketContext(bucket_info.id);
    bucket_context->SetSqliteRolloutStageForTesting(stage);
    return {std::move(factory_remote), bucket_context};
  }

  base::FilePath GetLevelDbCurrentFilePath(
      const storage::BucketInfo& bucket_info) {
    return GetFilePathForTesting(bucket_info.ToBucketLocator())
        .Append(FILE_PATH_LITERAL("CURRENT"));
  }

  std::map<StoreType, storage::BucketInfo> buckets_;
};

INSTANTIATE_TEST_SUITE_P(
    IndexedDB,
    SqliteBackingStoreRolloutStageTest,
    /*use default buckets*/ testing::Bool(),
    [](const testing::TestParamInfo<
        SqliteBackingStoreRolloutStageTest::ParamType>& info) {
      return info.param ? "Default" : "NonDefault";
    });

TEST_P(SqliteBackingStoreRolloutStageTest, UseLevelDbOnly) {
  ValidateExpectationsForStage(
      SqliteRolloutStage::kUseLevelDbOnly,
      {{StoreType::kNone, kCreatedWithLevelDb},
       {StoreType::kLevelDb, kOpenedLevelDb},
       {StoreType::kSqlite, kCreatedWithLevelDb},
       {StoreType::kEmptyLevelDbDirectory, kCreatedWithLevelDb},
       {StoreType::kLevelDbWithCorruptionInfo,
        {StoreInitResult::kDataLoss, kIsLevelDb}},
       {StoreType::kLevelDbCurrentMissing, kOpenedLevelDb},
       {StoreType::kLevelDbFilesMissing,
        {StoreInitResult::kFailed, kIsLevelDb}},
       {StoreType::kLevelDbInternalCorruption,
        {StoreInitResult::kDataLoss, kIsLevelDb}},
       {StoreType::kLevelDbBackingStoreCorruption,
        {StoreInitResult::kDataLoss, kIsLevelDb}}});
  // SQLite stores should be deleted by the above stage.
  CloseAllBackingStores();
  ValidateExpectationsForStage(
      SqliteRolloutStage::kUseSqliteOnly,
      {{StoreType::kNone, kCreatedWithSqlite},
       {StoreType::kLevelDb, kCreatedWithSqlite},
       {StoreType::kSqlite, kCreatedWithSqlite},
       {StoreType::kEmptyLevelDbDirectory, kCreatedWithSqlite},
       {StoreType::kLevelDbWithCorruptionInfo, kCreatedWithSqlite},
       {StoreType::kLevelDbCurrentMissing, kCreatedWithSqlite},
       {StoreType::kLevelDbFilesMissing, kCreatedWithSqlite},
       {StoreType::kLevelDbInternalCorruption, kCreatedWithSqlite},
       {StoreType::kLevelDbBackingStoreCorruption, kCreatedWithSqlite}});
}

TEST_P(SqliteBackingStoreRolloutStageTest, UseLevelDbAsControl) {
  ValidateExpectationsForStage(
      SqliteRolloutStage::kUseLevelDbAsControl,
      {{StoreType::kNone,
        {StoreInitResult::kCreated, kIsLevelDb, kExperimentalSuffix}},
       {StoreType::kLevelDb, kOpenedLevelDb},
       {StoreType::kSqlite,
        {StoreInitResult::kCreated, kIsLevelDb, kExperimentalSuffix}},
       {StoreType::kEmptyLevelDbDirectory,
        {StoreInitResult::kCreated, kIsLevelDb, kExperimentalSuffix}},
       {StoreType::kLevelDbWithCorruptionInfo,
        {StoreInitResult::kDataLoss, kIsLevelDb, kExperimentalSuffix}},
       {StoreType::kLevelDbCurrentMissing, kOpenedLevelDb},
       {StoreType::kLevelDbFilesMissing,
        {StoreInitResult::kFailed, kIsLevelDb}},
       {StoreType::kLevelDbInternalCorruption,
        {StoreInitResult::kDataLoss, kIsLevelDb, kExperimentalSuffix}},
       {StoreType::kLevelDbBackingStoreCorruption,
        {StoreInitResult::kDataLoss, kIsLevelDb, kExperimentalSuffix}}});
  // The experimental suffix should persist across opens.
  CloseAllBackingStores();
  ValidateExpectationsForStage(
      SqliteRolloutStage::kUseLevelDbAsControl,
      {{StoreType::kNone,
        {StoreInitResult::kOpened, kIsLevelDb, kExperimentalSuffix}},
       {StoreType::kLevelDb, kOpenedLevelDb},
       {StoreType::kSqlite,
        {StoreInitResult::kOpened, kIsLevelDb, kExperimentalSuffix}},
       {StoreType::kEmptyLevelDbDirectory,
        {StoreInitResult::kOpened, kIsLevelDb, kExperimentalSuffix}},
       {StoreType::kLevelDbWithCorruptionInfo,
        {StoreInitResult::kOpened, kIsLevelDb, kExperimentalSuffix}},
       {StoreType::kLevelDbCurrentMissing, kOpenedLevelDb},
       {StoreType::kLevelDbFilesMissing,
        {StoreInitResult::kFailed, kIsLevelDb}},
       {StoreType::kLevelDbInternalCorruption,
        {StoreInitResult::kOpened, kIsLevelDb, kExperimentalSuffix}},
       {StoreType::kLevelDbBackingStoreCorruption,
        {StoreInitResult::kOpened, kIsLevelDb, kExperimentalSuffix}}});
  // Rolling back to `kUseLevelDbOnly` should open all LevelDB stores normally,
  // with histograms emitted to the default suffix.
  CloseAllBackingStores();
  ValidateExpectationsForStage(
      SqliteRolloutStage::kUseLevelDbOnly,
      {{StoreType::kNone, kOpenedLevelDb},
       {StoreType::kLevelDb, kOpenedLevelDb},
       {StoreType::kSqlite, kOpenedLevelDb},
       {StoreType::kEmptyLevelDbDirectory, kOpenedLevelDb},
       {StoreType::kLevelDbWithCorruptionInfo, kOpenedLevelDb},
       {StoreType::kLevelDbCurrentMissing, kOpenedLevelDb},
       {StoreType::kLevelDbFilesMissing,
        {StoreInitResult::kFailed, kIsLevelDb}},
       {StoreType::kLevelDbInternalCorruption, kOpenedLevelDb},
       {StoreType::kLevelDbBackingStoreCorruption, kOpenedLevelDb}});
}

TEST_P(SqliteBackingStoreRolloutStageTest, UseSqliteForNewStores) {
  ValidateExpectationsForStage(
      SqliteRolloutStage::kUseSqliteForNewStores,
      {{StoreType::kNone,
        {StoreInitResult::kCreated, kIsSqlite, kExperimentalSuffix}},
       {StoreType::kLevelDb, kOpenedLevelDb},
       {StoreType::kSqlite,
        {StoreInitResult::kOpened, kIsSqlite, kExperimentalSuffix}},
       {StoreType::kEmptyLevelDbDirectory,
        {StoreInitResult::kCreated, kIsSqlite, kExperimentalSuffix}},
       {StoreType::kLevelDbWithCorruptionInfo,
        {StoreInitResult::kDataLoss, kIsSqlite, kExperimentalSuffix}},
       {StoreType::kLevelDbCurrentMissing, kOpenedLevelDb},
       {StoreType::kLevelDbFilesMissing,
        {StoreInitResult::kFailed, kIsLevelDb}},
       {StoreType::kLevelDbInternalCorruption,
        {StoreInitResult::kDataLoss, kIsSqlite, kExperimentalSuffix}},
       {StoreType::kLevelDbBackingStoreCorruption,
        {StoreInitResult::kDataLoss, kIsSqlite, kExperimentalSuffix}}});
  // SQLite should persist across opens.
  CloseAllBackingStores();
  ValidateExpectationsForStage(
      SqliteRolloutStage::kUseSqliteForNewStores,
      {{StoreType::kNone,
        {StoreInitResult::kOpened, kIsSqlite, kExperimentalSuffix}},
       {StoreType::kLevelDb, kOpenedLevelDb},
       {StoreType::kSqlite,
        {StoreInitResult::kOpened, kIsSqlite, kExperimentalSuffix}},
       {StoreType::kEmptyLevelDbDirectory,
        {StoreInitResult::kOpened, kIsSqlite, kExperimentalSuffix}},
       {StoreType::kLevelDbWithCorruptionInfo,
        {StoreInitResult::kOpened, kIsSqlite, kExperimentalSuffix}},
       {StoreType::kLevelDbCurrentMissing, kOpenedLevelDb},
       {StoreType::kLevelDbFilesMissing,
        {StoreInitResult::kFailed, kIsLevelDb}},
       {StoreType::kLevelDbInternalCorruption,
        {StoreInitResult::kOpened, kIsSqlite, kExperimentalSuffix}},
       {StoreType::kLevelDbBackingStoreCorruption,
        {StoreInitResult::kOpened, kIsSqlite, kExperimentalSuffix}}});
  // Rolling back to `kUseLevelDbOnly` should delete SQLite stores and create
  // fresh LevelDB stores. Stores that stayed on LevelDB should open normally.
  CloseAllBackingStores();
  ValidateExpectationsForStage(
      SqliteRolloutStage::kUseLevelDbOnly,
      {{StoreType::kNone, kCreatedWithLevelDb},
       {StoreType::kLevelDb, kOpenedLevelDb},
       {StoreType::kSqlite, kCreatedWithLevelDb},
       {StoreType::kEmptyLevelDbDirectory, kCreatedWithLevelDb},
       {StoreType::kLevelDbWithCorruptionInfo, kCreatedWithLevelDb},
       {StoreType::kLevelDbCurrentMissing, kOpenedLevelDb},
       {StoreType::kLevelDbFilesMissing,
        {StoreInitResult::kFailed, kIsLevelDb}},
       {StoreType::kLevelDbInternalCorruption, kCreatedWithLevelDb},
       {StoreType::kLevelDbBackingStoreCorruption, kCreatedWithLevelDb}});
}

TEST_P(SqliteBackingStoreRolloutStageTest, UseSqliteOnly) {
  ValidateExpectationsForStage(
      SqliteRolloutStage::kUseSqliteOnly,
      {{StoreType::kNone, kCreatedWithSqlite},
       {StoreType::kLevelDb, kCreatedWithSqlite},
       {StoreType::kSqlite, {StoreInitResult::kOpened, kIsSqlite}},
       {StoreType::kEmptyLevelDbDirectory, kCreatedWithSqlite},
       {StoreType::kLevelDbWithCorruptionInfo, kCreatedWithSqlite},
       {StoreType::kLevelDbCurrentMissing, kCreatedWithSqlite},
       {StoreType::kLevelDbFilesMissing, kCreatedWithSqlite},
       {StoreType::kLevelDbInternalCorruption, kCreatedWithSqlite},
       {StoreType::kLevelDbBackingStoreCorruption, kCreatedWithSqlite}});
  // LevelDB stores should be deleted by the above stage.
  CloseAllBackingStores();
  ValidateExpectationsForStage(
      SqliteRolloutStage::kUseLevelDbOnly,
      {{StoreType::kNone, kCreatedWithLevelDb},
       {StoreType::kLevelDb, kCreatedWithLevelDb},
       {StoreType::kSqlite, kCreatedWithLevelDb},
       {StoreType::kEmptyLevelDbDirectory, kCreatedWithLevelDb},
       {StoreType::kLevelDbWithCorruptionInfo, kCreatedWithLevelDb},
       {StoreType::kLevelDbCurrentMissing, kCreatedWithLevelDb},
       {StoreType::kLevelDbFilesMissing, kCreatedWithLevelDb},
       {StoreType::kLevelDbInternalCorruption, kCreatedWithLevelDb},
       {StoreType::kLevelDbBackingStoreCorruption, kCreatedWithLevelDb}});
}

}  // namespace content::indexed_db
