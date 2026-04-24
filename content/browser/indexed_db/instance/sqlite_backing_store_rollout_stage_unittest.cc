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
#include "content/browser/indexed_db/indexed_db_reporting.h"
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
  using enum DatabaseConnectionOpenResult;

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

  struct Expectation {
    DatabaseConnectionOpenResult result;
    bool is_sqlite;
    std::string_view histogram_suffix = ".OnDisk";
  };

  static constexpr const bool kIsSqlite = true;
  static constexpr const bool kIsLevelDb = false;
  static constexpr const std::string_view kExperimentalSuffix = ".Experimental";

  // Some common expectations.
  const Expectation kCreatedWithSqlite{kSuccessUpgradeNeeded, kIsSqlite};
  const Expectation kCreatedWithLevelDb{kSuccessUpgradeNeeded, kIsLevelDb};
  const Expectation kOpenedLevelDb{kSuccessDirectOpen, kIsLevelDb};

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
        case kSuccessDirectOpen:
          OpenDatabase(factory_remote, kDatabaseName, /*transaction_id=*/1);
          EXPECT_EQ(is_sqlite, bucket_context->IsUsingSqlite());
          break;
        case kSuccessUpgradeNeeded:
          CreateDatabase(factory_remote, kDatabaseName,
                         /*transaction_id=*/1, blink::mojom::IDBDataLoss::None);
          EXPECT_EQ(is_sqlite, bucket_context->IsUsingSqlite());
          break;
        case kSuccessUpgradeNeededWithDataLoss:
          CreateDatabase(factory_remote, kDatabaseName,
                         /*transaction_id=*/1,
                         blink::mojom::IDBDataLoss::Total);
          EXPECT_EQ(is_sqlite, bucket_context->IsUsingSqlite());
          break;
        case kErrorBackingStoreInitFailed: {
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
          break;
        }
        default:
          NOTREACHED();
      }
      std::string histogram_name = base::StrCat(
          {"IndexedDB.DatabaseConnectionOpenResult", histogram_suffix});
      histograms.ExpectTotalCount(histogram_name, 2);
      histograms.ExpectBucketCount(histogram_name, kReceivedRequest, 1);
      histograms.ExpectBucketCount(histogram_name, result, 1);
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
        {kSuccessUpgradeNeededWithDataLoss, kIsLevelDb}},
       {StoreType::kLevelDbCurrentMissing, kOpenedLevelDb},
       {StoreType::kLevelDbFilesMissing,
        {kErrorBackingStoreInitFailed, kIsLevelDb}},
       {StoreType::kLevelDbInternalCorruption,
        {kSuccessUpgradeNeededWithDataLoss, kIsLevelDb}},
       {StoreType::kLevelDbBackingStoreCorruption,
        {kSuccessUpgradeNeededWithDataLoss, kIsLevelDb}}});
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
        {kSuccessUpgradeNeeded, kIsLevelDb, kExperimentalSuffix}},
       {StoreType::kLevelDb, kOpenedLevelDb},
       {StoreType::kSqlite,
        {kSuccessUpgradeNeeded, kIsLevelDb, kExperimentalSuffix}},
       {StoreType::kEmptyLevelDbDirectory,
        {kSuccessUpgradeNeeded, kIsLevelDb, kExperimentalSuffix}},
       {StoreType::kLevelDbWithCorruptionInfo,
        {kSuccessUpgradeNeededWithDataLoss, kIsLevelDb, kExperimentalSuffix}},
       {StoreType::kLevelDbCurrentMissing, kOpenedLevelDb},
       {StoreType::kLevelDbFilesMissing,
        {kErrorBackingStoreInitFailed, kIsLevelDb}},
       {StoreType::kLevelDbInternalCorruption,
        {kSuccessUpgradeNeededWithDataLoss, kIsLevelDb, kExperimentalSuffix}},
       {StoreType::kLevelDbBackingStoreCorruption,
        {kSuccessUpgradeNeededWithDataLoss, kIsLevelDb, kExperimentalSuffix}}});
  // The experimental suffix should persist across opens.
  CloseAllBackingStores();
  ValidateExpectationsForStage(
      SqliteRolloutStage::kUseLevelDbAsControl,
      {{StoreType::kNone,
        {kSuccessDirectOpen, kIsLevelDb, kExperimentalSuffix}},
       {StoreType::kLevelDb, kOpenedLevelDb},
       {StoreType::kSqlite,
        {kSuccessDirectOpen, kIsLevelDb, kExperimentalSuffix}},
       {StoreType::kEmptyLevelDbDirectory,
        {kSuccessDirectOpen, kIsLevelDb, kExperimentalSuffix}},
       {StoreType::kLevelDbWithCorruptionInfo,
        {kSuccessDirectOpen, kIsLevelDb, kExperimentalSuffix}},
       {StoreType::kLevelDbCurrentMissing, kOpenedLevelDb},
       {StoreType::kLevelDbFilesMissing,
        {kErrorBackingStoreInitFailed, kIsLevelDb}},
       {StoreType::kLevelDbInternalCorruption,
        {kSuccessDirectOpen, kIsLevelDb, kExperimentalSuffix}},
       {StoreType::kLevelDbBackingStoreCorruption,
        {kSuccessDirectOpen, kIsLevelDb, kExperimentalSuffix}}});
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
        {kErrorBackingStoreInitFailed, kIsLevelDb}},
       {StoreType::kLevelDbInternalCorruption, kOpenedLevelDb},
       {StoreType::kLevelDbBackingStoreCorruption, kOpenedLevelDb}});
}

TEST_P(SqliteBackingStoreRolloutStageTest, UseSqliteForNewStores) {
  ValidateExpectationsForStage(
      SqliteRolloutStage::kUseSqliteForNewStores,
      {{StoreType::kNone,
        {kSuccessUpgradeNeeded, kIsSqlite, kExperimentalSuffix}},
       {StoreType::kLevelDb, kOpenedLevelDb},
       {StoreType::kSqlite,
        {kSuccessDirectOpen, kIsSqlite, kExperimentalSuffix}},
       {StoreType::kEmptyLevelDbDirectory,
        {kSuccessUpgradeNeeded, kIsSqlite, kExperimentalSuffix}},
       {StoreType::kLevelDbWithCorruptionInfo,
        {kSuccessUpgradeNeededWithDataLoss, kIsSqlite, kExperimentalSuffix}},
       {StoreType::kLevelDbCurrentMissing, kOpenedLevelDb},
       {StoreType::kLevelDbFilesMissing,
        {kErrorBackingStoreInitFailed, kIsLevelDb}},
       {StoreType::kLevelDbInternalCorruption,
        {kSuccessUpgradeNeededWithDataLoss, kIsSqlite, kExperimentalSuffix}},
       {StoreType::kLevelDbBackingStoreCorruption,
        {kSuccessUpgradeNeededWithDataLoss, kIsSqlite, kExperimentalSuffix}}});
  // SQLite should persist across opens.
  CloseAllBackingStores();
  ValidateExpectationsForStage(
      SqliteRolloutStage::kUseSqliteForNewStores,
      {{StoreType::kNone, {kSuccessDirectOpen, kIsSqlite, kExperimentalSuffix}},
       {StoreType::kLevelDb, kOpenedLevelDb},
       {StoreType::kSqlite,
        {kSuccessDirectOpen, kIsSqlite, kExperimentalSuffix}},
       {StoreType::kEmptyLevelDbDirectory,
        {kSuccessDirectOpen, kIsSqlite, kExperimentalSuffix}},
       {StoreType::kLevelDbWithCorruptionInfo,
        {kSuccessDirectOpen, kIsSqlite, kExperimentalSuffix}},
       {StoreType::kLevelDbCurrentMissing, kOpenedLevelDb},
       {StoreType::kLevelDbFilesMissing,
        {kErrorBackingStoreInitFailed, kIsLevelDb}},
       {StoreType::kLevelDbInternalCorruption,
        {kSuccessDirectOpen, kIsSqlite, kExperimentalSuffix}},
       {StoreType::kLevelDbBackingStoreCorruption,
        {kSuccessDirectOpen, kIsSqlite, kExperimentalSuffix}}});
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
        {kErrorBackingStoreInitFailed, kIsLevelDb}},
       {StoreType::kLevelDbInternalCorruption, kCreatedWithLevelDb},
       {StoreType::kLevelDbBackingStoreCorruption, kCreatedWithLevelDb}});
}

TEST_P(SqliteBackingStoreRolloutStageTest, UseSqliteOnly) {
  ValidateExpectationsForStage(
      SqliteRolloutStage::kUseSqliteOnly,
      {{StoreType::kNone, kCreatedWithSqlite},
       {StoreType::kLevelDb, kCreatedWithSqlite},
       {StoreType::kSqlite, {kSuccessDirectOpen, kIsSqlite}},
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
