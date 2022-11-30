// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <ctime>
#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/barrier_closure.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/default_clock.h"
#include "components/services/storage/filesystem_proxy_factory.h"
#include "components/services/storage/indexed_db/leveldb/fake_leveldb_factory.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_database.h"
#include "components/services/storage/privileged/mojom/indexed_db_control.mojom-test-utils.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/mojom/storage_usage_info.mojom.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_bucket_state.h"
#include "content/browser/indexed_db/indexed_db_class_factory.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_data_format_version.h"
#include "content/browser/indexed_db/indexed_db_factory_impl.h"
#include "content/browser/indexed_db/indexed_db_leveldb_env.h"
#include "content/browser/indexed_db/indexed_db_pre_close_task_queue.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "content/browser/indexed_db/mock_indexed_db_callbacks.h"
#include "content/browser/indexed_db/mock_indexed_db_database_callbacks.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "net/base/features.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/indexeddb/web_idb_types.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

using blink::IndexedDBDatabaseMetadata;
using url::Origin;

namespace content {

namespace {

void CreateAndBindTransactionPlaceholder(
    base::WeakPtr<IndexedDBTransaction> transaction) {}

}  // namespace

class IndexedDBFactoryTest : public testing::Test {
 public:
  IndexedDBFactoryTest()
      : task_environment_(std::make_unique<base::test::TaskEnvironment>()) {}

  explicit IndexedDBFactoryTest(
      std::unique_ptr<base::test::TaskEnvironment> task_environment)
      : task_environment_(std::move(task_environment)) {}

  IndexedDBFactoryTest(const IndexedDBFactoryTest&) = delete;
  IndexedDBFactoryTest& operator=(const IndexedDBFactoryTest&) = delete;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    quota_policy_ = base::MakeRefCounted<storage::MockSpecialStoragePolicy>();
    quota_manager_ = base::MakeRefCounted<storage::MockQuotaManager>(
        /*is_incognito=*/false, temp_dir_.GetPath(),
        base::SingleThreadTaskRunner::GetCurrentDefault().get(),
        quota_policy_.get());

    quota_manager_proxy_ = base::MakeRefCounted<storage::MockQuotaManagerProxy>(
        quota_manager_.get(),
        base::SingleThreadTaskRunner::GetCurrentDefault().get());
  }

  void TearDown() override {
    if (context_ && !context_->IsInMemoryContext()) {
      IndexedDBFactoryImpl* factory = context_->GetIDBFactory();

      // Loop through all open storage keys, and force close them, and request
      // the deletion of the leveldb state. Once the states are no longer
      // around, delete all of the databases on disk.
      for (const auto& bucket_id : factory->GetOpenBuckets()) {
        context_->ForceClose(
            bucket_id,
            storage::mojom::ForceCloseReason::FORCE_CLOSE_DELETE_ORIGIN,
            base::DoNothing());
      }
      // All leveldb databases are closed, and they can be deleted.
      for (auto bucket_locator : context_->GetAllBuckets()) {
        bool success = false;
        storage::mojom::IndexedDBControlAsyncWaiter waiter(context_.get());
        waiter.DeleteForStorageKey(bucket_locator.storage_key, &success);
        EXPECT_TRUE(success);
      }
    }
    IndexedDBClassFactory::Get()->SetLevelDBFactoryForTesting(nullptr);
    quota_manager_.reset();
  }

  void SetupContext() {
    context_ = base::MakeRefCounted<IndexedDBContextImpl>(
        temp_dir_.GetPath(), quota_manager_proxy_.get(),
        base::DefaultClock::GetInstance(),
        /*blob_storage_context=*/mojo::NullRemote(),
        /*file_system_access_context=*/mojo::NullRemote(),
        base::SequencedTaskRunner::GetCurrentDefault(),
        base::SequencedTaskRunner::GetCurrentDefault());
  }

  void SetupInMemoryContext() {
    context_ = base::MakeRefCounted<IndexedDBContextImpl>(
        base::FilePath(), quota_manager_proxy_.get(),
        base::DefaultClock::GetInstance(),
        /*blob_storage_context=*/mojo::NullRemote(),
        /*file_system_access_context=*/mojo::NullRemote(),
        base::SequencedTaskRunner::GetCurrentDefault(),
        base::SequencedTaskRunner::GetCurrentDefault());
  }

  void SetupContextWithFactories(LevelDBFactory* factory, base::Clock* clock) {
    context_ = base::MakeRefCounted<IndexedDBContextImpl>(
        temp_dir_.GetPath(), quota_manager_proxy_.get(), clock,
        /*blob_storage_context=*/mojo::NullRemote(),
        /*file_system_access_context=*/mojo::NullRemote(),
        base::SequencedTaskRunner::GetCurrentDefault(),
        base::SequencedTaskRunner::GetCurrentDefault());
    if (factory)
      IndexedDBClassFactory::Get()->SetLevelDBFactoryForTesting(factory);
  }

  // Runs through the upgrade flow to create a basic database connection. There
  // is no actual data in the database.
  std::tuple<std::unique_ptr<IndexedDBConnection>,
             scoped_refptr<MockIndexedDBDatabaseCallbacks>>
  CreateConnectionForDatatabase(const storage::BucketLocator& bucket_locator,
                                const std::u16string& name) {
    auto callbacks = base::MakeRefCounted<MockIndexedDBCallbacks>();
    auto db_callbacks = base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();
    const int64_t transaction_id = 1;
    auto create_transaction_callback =
        base::BindOnce(&CreateAndBindTransactionPlaceholder);
    auto connection = std::make_unique<IndexedDBPendingConnection>(
        callbacks, db_callbacks,
        transaction_id, IndexedDBDatabaseMetadata::NO_VERSION,
        std::move(create_transaction_callback));

    // Do the first half of the upgrade, and request the upgrade from renderer.
    {
      base::RunLoop loop;
      callbacks->CallOnUpgradeNeeded(
          base::BindLambdaForTesting([&]() { loop.Quit(); }));
      factory()->Open(name, std::move(connection), bucket_locator,
                      context()->GetDataPath(bucket_locator));
      loop.Run();
    }

    EXPECT_TRUE(callbacks->upgrade_called());
    EXPECT_TRUE(callbacks->connection());
    EXPECT_TRUE(callbacks->connection()->database());
    if (!callbacks->connection())
      return {nullptr, nullptr};

    // Finish the upgrade by committing the transaction.
    {
      base::RunLoop loop;
      callbacks->CallOnDBSuccess(
          base::BindLambdaForTesting([&]() { loop.Quit(); }));
      callbacks->connection()
          ->transactions()
          .find(transaction_id)
          ->second->SetCommitFlag();
      loop.Run();
    }
    return {callbacks->TakeConnection(), db_callbacks};
  }

  void RunPostedTasks() {
    base::RunLoop loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, loop.QuitClosure());
    loop.Run();
  }

 protected:
  IndexedDBContextImpl* context() const { return context_.get(); }

  IndexedDBFactoryImpl* factory() const { return context_->GetIDBFactory(); }

  base::test::TaskEnvironment* task_environment() const {
    return task_environment_.get();
  }

  IndexedDBBucketState* StorageBucketFromHandle(
      IndexedDBBucketStateHandle& handle) {
    return handle.bucket_state();
  }

  storage::MockQuotaManager* quota_manager() { return quota_manager_.get(); }

 protected:
  std::unique_ptr<base::test::TaskEnvironment> task_environment_;

  base::ScopedTempDir temp_dir_;
  scoped_refptr<storage::MockSpecialStoragePolicy> quota_policy_;
  scoped_refptr<storage::MockQuotaManager> quota_manager_;
  scoped_refptr<storage::MockQuotaManagerProxy> quota_manager_proxy_;
  scoped_refptr<IndexedDBContextImpl> context_;
};

class IndexedDBFactoryTestWithMockTime : public IndexedDBFactoryTest {
 public:
  IndexedDBFactoryTestWithMockTime()
      : IndexedDBFactoryTest(std::make_unique<base::test::TaskEnvironment>(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME)) {}

  IndexedDBFactoryTestWithMockTime(const IndexedDBFactoryTestWithMockTime&) =
      delete;
  IndexedDBFactoryTestWithMockTime& operator=(
      const IndexedDBFactoryTestWithMockTime&) = delete;
};

class IndexedDBFactoryTestWithStoragePartitioning
    : public IndexedDBFactoryTest,
      public testing::WithParamInterface<bool> {
 public:
  IndexedDBFactoryTestWithStoragePartitioning() {
    feature_list_.InitWithFeatureState(
        net::features::kThirdPartyStoragePartitioning,
        IsThirdPartyStoragePartitioningEnabled());
  }

  bool IsThirdPartyStoragePartitioningEnabled() { return GetParam(); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    IndexedDBFactoryTestWithStoragePartitioning,
    testing::Bool());

TEST_P(IndexedDBFactoryTestWithStoragePartitioning,
       BasicFactoryCreationAndTearDown) {
  auto filesystem_proxy = storage::CreateFilesystemProxy();
  SetupContext();

  const blink::StorageKey storage_key_1 =
      blink::StorageKey::CreateFromStringForTesting("http://localhost:81");
  storage::BucketLocator bucket_locator_1 =
      quota_manager()
          ->GetOrCreateBucketSync(
              storage::BucketInitParams::ForDefaultBucket(storage_key_1))
          ->ToBucketLocator();
  auto file_1 = context_->GetLevelDBPathForTesting(bucket_locator_1)
                    .AppendASCII("1.json");
  ASSERT_TRUE(CreateDirectory(file_1.DirName()));
  ASSERT_TRUE(base::WriteFile(file_1, std::string(10, 'a')));

  const blink::StorageKey storage_key_2 =
      blink::StorageKey::CreateFromStringForTesting("http://localhost:82");
  storage::BucketLocator bucket_locator_2 =
      quota_manager()
          ->GetOrCreateBucketSync(
              storage::BucketInitParams::ForDefaultBucket(storage_key_2))
          ->ToBucketLocator();
  auto file_2 = context_->GetLevelDBPathForTesting(bucket_locator_2)
                    .AppendASCII("2.json");
  ASSERT_TRUE(CreateDirectory(file_2.DirName()));
  ASSERT_TRUE(base::WriteFile(file_2, std::string(100, 'a')));

  const blink::StorageKey storage_key_3 =
      blink::StorageKey::CreateFromStringForTesting("http://localhost2:82");
  storage::BucketLocator bucket_locator_3 =
      quota_manager()
          ->GetOrCreateBucketSync(
              storage::BucketInitParams::ForDefaultBucket(storage_key_3))
          ->ToBucketLocator();
  auto file_3 = context_->GetLevelDBPathForTesting(bucket_locator_3)
                    .AppendASCII("3.json");
  ASSERT_TRUE(CreateDirectory(file_3.DirName()));
  ASSERT_TRUE(base::WriteFile(file_3, std::string(1000, 'a')));

  const blink::StorageKey storage_key_4 =
      blink::StorageKey::CreateWithOptionalNonce(
          storage_key_1.origin(), net::SchemefulSite(storage_key_3.origin()),
          nullptr, blink::mojom::AncestorChainBit::kCrossSite);
  storage::BucketLocator bucket_locator_4 =
      quota_manager()
          ->GetOrCreateBucketSync(
              storage::BucketInitParams::ForDefaultBucket(storage_key_4))
          ->ToBucketLocator();
  auto file_4 = context_->GetLevelDBPathForTesting(bucket_locator_4)
                    .AppendASCII("4.json");
  ASSERT_TRUE(CreateDirectory(file_4.DirName()));
  ASSERT_TRUE(base::WriteFile(file_4, std::string(10000, 'a')));

  const blink::StorageKey storage_key_5 = storage_key_1;
  storage::BucketInitParams params(storage_key_5, "inbox");
  storage::BucketLocator bucket_locator_5 =
      quota_manager()->GetOrCreateBucketSync(params)->ToBucketLocator();
  auto file_5 = context_->GetLevelDBPathForTesting(bucket_locator_5)
                    .AppendASCII("5.json");
  ASSERT_TRUE(CreateDirectory(file_5.DirName()));
  ASSERT_TRUE(base::WriteFile(file_5, std::string(20000, 'a')));
  EXPECT_NE(file_5.DirName(), file_1.DirName());

  IndexedDBBucketStateHandle bucket_state1_handle;
  IndexedDBBucketStateHandle bucket_state2_handle;
  IndexedDBBucketStateHandle bucket_state3_handle;
  IndexedDBBucketStateHandle bucket_state4_handle;
  IndexedDBBucketStateHandle bucket_state5_handle;
  leveldb::Status s;

  std::tie(bucket_state1_handle, s, std::ignore, std::ignore, std::ignore) =
      factory()->GetOrOpenBucketFactory(
          bucket_locator_1, context()->GetDataPath(bucket_locator_1),
          /*create_if_missing=*/true);
  EXPECT_TRUE(bucket_state1_handle.IsHeld()) << s.ToString();
  EXPECT_TRUE(s.ok()) << s.ToString();

  std::tie(bucket_state2_handle, s, std::ignore, std::ignore, std::ignore) =
      factory()->GetOrOpenBucketFactory(
          bucket_locator_2, context()->GetDataPath(bucket_locator_2),
          /*create_if_missing=*/true);
  EXPECT_TRUE(bucket_state2_handle.IsHeld()) << s.ToString();
  EXPECT_TRUE(s.ok()) << s.ToString();

  std::tie(bucket_state3_handle, s, std::ignore, std::ignore, std::ignore) =
      factory()->GetOrOpenBucketFactory(
          bucket_locator_3, context()->GetDataPath(bucket_locator_3),
          /*create_if_missing=*/true);
  EXPECT_TRUE(bucket_state3_handle.IsHeld()) << s.ToString();
  EXPECT_TRUE(s.ok()) << s.ToString();

  std::tie(bucket_state4_handle, s, std::ignore, std::ignore, std::ignore) =
      factory()->GetOrOpenBucketFactory(
          bucket_locator_4, context()->GetDataPath(bucket_locator_4),
          /*create_if_missing=*/true);
  EXPECT_TRUE(bucket_state4_handle.IsHeld()) << s.ToString();
  EXPECT_TRUE(s.ok()) << s.ToString();

  std::tie(bucket_state5_handle, s, std::ignore, std::ignore, std::ignore) =
      factory()->GetOrOpenBucketFactory(
          bucket_locator_5, context()->GetDataPath(bucket_locator_5),
          /*create_if_missing=*/true);
  EXPECT_TRUE(bucket_state5_handle.IsHeld()) << s.ToString();
  EXPECT_TRUE(s.ok()) << s.ToString();

  std::vector<storage::mojom::StorageUsageInfoPtr> infos;
  storage::mojom::IndexedDBControlAsyncWaiter sync_control(context());
  sync_control.GetUsage(&infos);

  int64_t bucket_size_1 =
      filesystem_proxy->ComputeDirectorySize(file_1.DirName());
  int64_t bucket_size_2 =
      filesystem_proxy->ComputeDirectorySize(file_2.DirName());
  int64_t bucket_size_3 =
      filesystem_proxy->ComputeDirectorySize(file_3.DirName());
  int64_t bucket_size_4 =
      filesystem_proxy->ComputeDirectorySize(file_4.DirName());
  int64_t bucket_size_5 =
      filesystem_proxy->ComputeDirectorySize(file_5.DirName());

  // Buckets 1, 4, and 5 merge if partitioning is on. If partitioning is off
  // buckets 1 and 5 merge.
  EXPECT_EQ(IsThirdPartyStoragePartitioningEnabled() ? 4ul : 3ul, infos.size());
  for (const auto& info : infos) {
    if (info->storage_key == bucket_locator_1.storage_key) {
      // This is the size of the 10 and 10000 character files (buckets 1 and 4).
      if (IsThirdPartyStoragePartitioningEnabled()) {
        // If third party storage partitioning is on, additional space is taken
        // by supporting files for the independent buckets.
        EXPECT_NE(bucket_size_1, bucket_size_4);
      } else {
        EXPECT_NE(bucket_size_1, bucket_size_5);
      }
      EXPECT_NE(bucket_size_1, bucket_size_5);
      EXPECT_EQ(info->total_size_bytes, bucket_size_1 + bucket_size_5);
    } else if (info->storage_key == bucket_locator_2.storage_key) {
      // This is the size of the 100 character file (bucket 2).
      EXPECT_EQ(info->total_size_bytes, bucket_size_2);
    } else if (info->storage_key == bucket_locator_3.storage_key) {
      // This is the size of the 1000 character file (bucket 3).
      EXPECT_EQ(info->total_size_bytes, bucket_size_3);
    } else if (info->storage_key == bucket_locator_4.storage_key) {
      // This is the size of the 1000 character file (bucket 4).
      EXPECT_EQ(info->total_size_bytes, bucket_size_4);
    } else {
      NOTREACHED();
    }
  }
  if (IsThirdPartyStoragePartitioningEnabled()) {
    EXPECT_EQ(5ul, factory()->GetOpenBuckets().size());
  } else {
    EXPECT_EQ(4ul, factory()->GetOpenBuckets().size());
  }
}

TEST_F(IndexedDBFactoryTest, CloseSequenceStarts) {
  SetupContext();

  const blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://localhost:81");
  auto bucket_locator = storage::BucketLocator();
  bucket_locator.storage_key = storage_key;
  IndexedDBBucketStateHandle bucket_state_handle;
  leveldb::Status s;

  std::tie(bucket_state_handle, s, std::ignore, std::ignore, std::ignore) =
      factory()->GetOrOpenBucketFactory(bucket_locator,
                                        context()->GetDataPath(bucket_locator),
                                        /*create_if_missing=*/true);
  EXPECT_TRUE(bucket_state_handle.IsHeld()) << s.ToString();
  bucket_state_handle.Release();

  EXPECT_TRUE(factory()->GetBucketFactory(bucket_locator.id));
  EXPECT_TRUE(factory()->GetBucketFactory(bucket_locator.id)->IsClosing());

  factory()->ForceClose(bucket_locator.id, false);
  RunPostedTasks();
  EXPECT_FALSE(factory()->GetBucketFactory(bucket_locator.id));
}

TEST_F(IndexedDBFactoryTest, ImmediateClose) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      kIDBCloseImmediatelySwitch);
  SetupContext();

  const blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://localhost:81");
  auto bucket_locator = storage::BucketLocator();
  bucket_locator.storage_key = storage_key;
  IndexedDBBucketStateHandle bucket_state_handle;
  leveldb::Status s;

  std::tie(bucket_state_handle, s, std::ignore, std::ignore, std::ignore) =
      factory()->GetOrOpenBucketFactory(bucket_locator,
                                        context()->GetDataPath(bucket_locator),
                                        /*create_if_missing=*/true);
  EXPECT_TRUE(bucket_state_handle.IsHeld()) << s.ToString();
  bucket_state_handle.Release();

  EXPECT_TRUE(factory()->GetBucketFactory(bucket_locator.id));
  RunPostedTasks();
  EXPECT_FALSE(factory()->GetBucketFactory(bucket_locator.id));
  EXPECT_EQ(0ul, factory()->GetOpenBuckets().size());
}

TEST_F(IndexedDBFactoryTestWithMockTime, PreCloseTasksStart) {
  base::SimpleTestClock clock;
  clock.SetNow(base::Time::Now());
  SetupContextWithFactories(nullptr, &clock);

  const blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://localhost:81");
  auto bucket_locator = storage::BucketLocator();
  bucket_locator.storage_key = storage_key;
  IndexedDBBucketStateHandle bucket_state_handle;
  leveldb::Status s;

  // Open a connection & immediately release it to cause the closing sequence to
  // start.
  std::tie(bucket_state_handle, s, std::ignore, std::ignore, std::ignore) =
      factory()->GetOrOpenBucketFactory(bucket_locator,
                                        context()->GetDataPath(bucket_locator),
                                        /*create_if_missing=*/true);
  EXPECT_TRUE(bucket_state_handle.IsHeld()) << s.ToString();
  bucket_state_handle.Release();

  EXPECT_TRUE(factory()->GetBucketFactory(bucket_locator.id));
  EXPECT_TRUE(factory()->GetBucketFactory(bucket_locator.id)->IsClosing());

  EXPECT_EQ(IndexedDBBucketState::ClosingState::kPreCloseGracePeriod,
            factory()->GetBucketFactory(bucket_locator.id)->closing_stage());

  task_environment()->FastForwardBy(base::Seconds(2));

  // The factory should be closed, as the pre close tasks are delayed.
  EXPECT_FALSE(factory()->GetBucketFactory(bucket_locator.id));

  // Move the clock to run the tasks in the next close sequence.
  // NOTE: The constants rate-limiting sweeps and compaction are currently the
  // same. This test may need to be restructured if these values diverge.
  clock.Advance(IndexedDBBucketState::kMaxEarliestGlobalSweepFromNow);

  // Open a connection & immediately release it to cause the closing sequence to
  // start again.
  std::tie(bucket_state_handle, s, std::ignore, std::ignore, std::ignore) =
      factory()->GetOrOpenBucketFactory(bucket_locator,
                                        context()->GetDataPath(bucket_locator),
                                        /*create_if_missing=*/true);
  EXPECT_TRUE(bucket_state_handle.IsHeld()) << s.ToString();
  bucket_state_handle.Release();

  // Manually execute the timer so that the PreCloseTaskList task doesn't also
  // run.
  factory()->GetBucketFactory(bucket_locator.id)->close_timer()->FireNow();

  // The pre-close tasks should be running now.
  ASSERT_TRUE(factory()->GetBucketFactory(bucket_locator.id));
  EXPECT_EQ(IndexedDBBucketState::ClosingState::kRunningPreCloseTasks,
            factory()->GetBucketFactory(bucket_locator.id)->closing_stage());
  ASSERT_TRUE(
      factory()->GetBucketFactory(bucket_locator.id)->pre_close_task_queue());
  EXPECT_TRUE(factory()
                  ->GetBucketFactory(bucket_locator.id)
                  ->pre_close_task_queue()
                  ->started());

  // Stop sweep by opening a connection.
  std::tie(bucket_state_handle, s, std::ignore, std::ignore, std::ignore) =
      factory()->GetOrOpenBucketFactory(bucket_locator,
                                        context()->GetDataPath(bucket_locator),
                                        /*create_if_missing=*/true);
  EXPECT_TRUE(bucket_state_handle.IsHeld()) << s.ToString();
  EXPECT_FALSE(
      StorageBucketFromHandle(bucket_state_handle)->pre_close_task_queue());
  bucket_state_handle.Release();

  // Move clock forward to trigger next sweep, but storage key has longer
  // sweep minimum, so no tasks should execute.
  clock.Advance(IndexedDBBucketState::kMaxEarliestGlobalSweepFromNow);

  bucket_state_handle.Release();
  EXPECT_TRUE(factory()->GetBucketFactory(bucket_locator.id));
  EXPECT_EQ(IndexedDBBucketState::ClosingState::kPreCloseGracePeriod,
            factory()->GetBucketFactory(bucket_locator.id)->closing_stage());

  // Manually execute the timer so that the PreCloseTaskList task doesn't also
  // run.
  factory()->GetBucketFactory(bucket_locator.id)->close_timer()->FireNow();
  EXPECT_TRUE(factory()->GetBucketFactory(bucket_locator.id));
  RunPostedTasks();
  EXPECT_FALSE(factory()->GetBucketFactory(bucket_locator.id));

  //  Finally, move the clock forward so the storage key should allow a sweep.
  clock.Advance(IndexedDBBucketState::kMaxEarliestBucketSweepFromNow);
  std::tie(bucket_state_handle, s, std::ignore, std::ignore, std::ignore) =
      factory()->GetOrOpenBucketFactory(bucket_locator,
                                        context()->GetDataPath(bucket_locator),
                                        /*create_if_missing=*/true);
  bucket_state_handle.Release();
  factory()->GetBucketFactory(bucket_locator.id)->close_timer()->FireNow();

  ASSERT_TRUE(factory()->GetBucketFactory(bucket_locator.id));
  EXPECT_EQ(IndexedDBBucketState::ClosingState::kRunningPreCloseTasks,
            factory()->GetBucketFactory(bucket_locator.id)->closing_stage());
  ASSERT_TRUE(
      factory()->GetBucketFactory(bucket_locator.id)->pre_close_task_queue());
  EXPECT_TRUE(factory()
                  ->GetBucketFactory(bucket_locator.id)
                  ->pre_close_task_queue()
                  ->started());
}

TEST_F(IndexedDBFactoryTestWithMockTime, TombstoneSweeperTiming) {
  base::SimpleTestClock clock;
  clock.SetNow(base::Time::Now());
  SetupContextWithFactories(nullptr, &clock);

  const blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://localhost:81");
  auto bucket_locator = storage::BucketLocator();
  bucket_locator.storage_key = storage_key;
  IndexedDBBucketStateHandle bucket_state_handle;
  leveldb::Status s;

  // Open a connection & immediately release it to cause the closing sequence to
  // start.
  std::tie(bucket_state_handle, s, std::ignore, std::ignore, std::ignore) =
      factory()->GetOrOpenBucketFactory(bucket_locator,
                                        context()->GetDataPath(bucket_locator),
                                        /*create_if_missing=*/true);
  EXPECT_TRUE(bucket_state_handle.IsHeld()) << s.ToString();

  // The factory should be closed, as the pre close tasks are delayed.
  EXPECT_FALSE(bucket_state_handle.bucket_state()->ShouldRunTombstoneSweeper());

  // Move the clock to run the tasks in the next close sequence.
  clock.Advance(IndexedDBBucketState::kMaxEarliestGlobalSweepFromNow);

  EXPECT_TRUE(bucket_state_handle.bucket_state()->ShouldRunTombstoneSweeper());

  // Move clock forward to trigger next sweep, but storage key has longer
  // sweep minimum, so no tasks should execute.
  clock.Advance(IndexedDBBucketState::kMaxEarliestGlobalSweepFromNow);

  EXPECT_FALSE(bucket_state_handle.bucket_state()->ShouldRunTombstoneSweeper());

  //  Finally, move the clock forward so the storage key should allow a sweep.
  clock.Advance(IndexedDBBucketState::kMaxEarliestBucketSweepFromNow);

  EXPECT_TRUE(bucket_state_handle.bucket_state()->ShouldRunTombstoneSweeper());
}

TEST_F(IndexedDBFactoryTestWithMockTime, CompactionTaskTiming) {
  base::SimpleTestClock clock;
  clock.SetNow(base::Time::Now());
  SetupContextWithFactories(nullptr, &clock);

  const blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://localhost:81");
  auto bucket_locator = storage::BucketLocator();
  bucket_locator.storage_key = storage_key;
  IndexedDBBucketStateHandle bucket_state_handle;
  leveldb::Status s;

  // Open a connection & immediately release it to cause the closing sequence to
  // start.
  std::tie(bucket_state_handle, s, std::ignore, std::ignore, std::ignore) =
      factory()->GetOrOpenBucketFactory(bucket_locator,
                                        context()->GetDataPath(bucket_locator),
                                        /*create_if_missing=*/true);
  EXPECT_TRUE(bucket_state_handle.IsHeld()) << s.ToString();

  // The factory should be closed, as the pre close tasks are delayed.
  EXPECT_FALSE(bucket_state_handle.bucket_state()->ShouldRunCompaction());

  // Move the clock to run the tasks in the next close sequence.
  clock.Advance(IndexedDBBucketState::kMaxEarliestGlobalCompactionFromNow);

  EXPECT_TRUE(bucket_state_handle.bucket_state()->ShouldRunCompaction());

  // Move clock forward to trigger next compaction, but storage key has longer
  // compaction minimum, so no tasks should execute.
  clock.Advance(IndexedDBBucketState::kMaxEarliestGlobalCompactionFromNow);

  EXPECT_FALSE(bucket_state_handle.bucket_state()->ShouldRunCompaction());

  //  Finally, move the clock forward so the storage key should allow a
  //  compaction.
  clock.Advance(IndexedDBBucketState::kMaxEarliestBucketCompactionFromNow);

  EXPECT_TRUE(bucket_state_handle.bucket_state()->ShouldRunCompaction());
}

// Remove this test when the kill switch is removed.
TEST_F(IndexedDBFactoryTest, CompactionKillSwitchWorks) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({}, {kCompactIDBOnClose});

  SetupContext();

  const blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://localhost:81");
  auto bucket_locator = storage::BucketLocator();
  bucket_locator.storage_key = storage_key;
  IndexedDBBucketStateHandle bucket_state_handle;
  leveldb::Status s;

  // Open a connection & immediately release it to cause the closing sequence to
  // start.
  std::tie(bucket_state_handle, s, std::ignore, std::ignore, std::ignore) =
      factory()->GetOrOpenBucketFactory(bucket_locator,
                                        context()->GetDataPath(bucket_locator),
                                        /*create_if_missing=*/true);
  EXPECT_TRUE(bucket_state_handle.IsHeld()) << s.ToString();

  EXPECT_FALSE(bucket_state_handle.bucket_state()->ShouldRunCompaction());
}

TEST_F(IndexedDBFactoryTest, InMemoryFactoriesStay) {
  SetupInMemoryContext();
  ASSERT_TRUE(context()->IsInMemoryContext());

  const blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://localhost:81");
  storage::BucketLocator bucket_locator =
      quota_manager()
          ->GetOrCreateBucketSync(
              storage::BucketInitParams::ForDefaultBucket(storage_key))
          ->ToBucketLocator();
  IndexedDBBucketStateHandle bucket_state_handle;
  leveldb::Status s;

  std::tie(bucket_state_handle, s, std::ignore, std::ignore, std::ignore) =
      factory()->GetOrOpenBucketFactory(bucket_locator,
                                        context()->GetDataPath(bucket_locator),
                                        /*create_if_missing=*/true);
  EXPECT_TRUE(bucket_state_handle.IsHeld()) << s.ToString();
  EXPECT_TRUE(StorageBucketFromHandle(bucket_state_handle)
                  ->backing_store()
                  ->is_incognito());
  bucket_state_handle.Release();

  EXPECT_TRUE(factory()->GetBucketFactory(bucket_locator.id));
  EXPECT_FALSE(factory()->GetBucketFactory(bucket_locator.id)->IsClosing());

  factory()->ForceClose(bucket_locator.id, false);
  EXPECT_TRUE(factory()->GetBucketFactory(bucket_locator.id));

  factory()->ForceClose(bucket_locator.id, true);
  EXPECT_FALSE(factory()->GetBucketFactory(bucket_locator.id));
}

TEST_F(IndexedDBFactoryTest, TooLongOrigin) {
  SetupContext();

  base::FilePath temp_dir =
      context()->GetFirstPartyDataPathForTesting().DirName();
  int limit = base::GetMaximumPathComponentLength(temp_dir);
  EXPECT_GT(limit, 0);

  std::string origin(limit + 1, 'x');
  const blink::StorageKey too_long_storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://" + origin +
                                                    ":81/");
  storage::BucketLocator bucket_locator =
      quota_manager()
          ->GetOrCreateBucketSync(
              storage::BucketInitParams::ForDefaultBucket(too_long_storage_key))
          ->ToBucketLocator();
  IndexedDBBucketStateHandle bucket_state_handle;
  leveldb::Status s;

  std::tie(bucket_state_handle, s, std::ignore, std::ignore, std::ignore) =
      factory()->GetOrOpenBucketFactory(bucket_locator,
                                        context()->GetDataPath(bucket_locator),
                                        /*create_if_missing=*/true);
  EXPECT_FALSE(bucket_state_handle.IsHeld());
  EXPECT_TRUE(s.IsIOError());
}

TEST_F(IndexedDBFactoryTest, ContextDestructionClosesConnections) {
  SetupContext();
  const blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://localhost:81");
  auto bucket_locator = storage::BucketLocator();
  bucket_locator.storage_key = storage_key;

  auto callbacks = base::MakeRefCounted<MockIndexedDBCallbacks>();
  auto db_callbacks = base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();

  const int64_t transaction_id = 1;
  auto create_transaction_callback =
      base::BindOnce(&CreateAndBindTransactionPlaceholder);
  auto connection = std::make_unique<IndexedDBPendingConnection>(
      callbacks, db_callbacks,
      transaction_id, IndexedDBDatabaseMetadata::DEFAULT_VERSION,
      std::move(create_transaction_callback));
  factory()->Open(u"db", std::move(connection), bucket_locator,
                  context()->GetDataPath(bucket_locator));
  RunPostedTasks();

  // Now simulate shutdown, which should clear all factories.
  factory()->ContextDestroyed();
  EXPECT_TRUE(db_callbacks->forced_close_called());
}

TEST_F(IndexedDBFactoryTest, ContextDestructionClosesHandles) {
  SetupContext();
  const blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://localhost:81");
  auto bucket_locator = storage::BucketLocator();
  bucket_locator.storage_key = storage_key;
  IndexedDBBucketStateHandle bucket_state_handle;
  leveldb::Status s;

  std::tie(bucket_state_handle, s, std::ignore, std::ignore, std::ignore) =
      factory()->GetOrOpenBucketFactory(bucket_locator,
                                        context()->GetDataPath(bucket_locator),
                                        /*create_if_missing=*/true);
  EXPECT_TRUE(bucket_state_handle.IsHeld()) << s.ToString();

  // Now simulate shutdown, which should clear all factories.
  factory()->ContextDestroyed();
  EXPECT_FALSE(StorageBucketFromHandle(bucket_state_handle));
  EXPECT_FALSE(factory()->GetBucketFactory(bucket_locator.id));
}

TEST_F(IndexedDBFactoryTest, FactoryForceClose) {
  SetupContext();
  const blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://localhost:81");
  auto bucket_locator = storage::BucketLocator();
  bucket_locator.storage_key = storage_key;
  IndexedDBBucketStateHandle bucket_state_handle;
  leveldb::Status s;

  std::tie(bucket_state_handle, s, std::ignore, std::ignore, std::ignore) =
      factory()->GetOrOpenBucketFactory(bucket_locator,
                                        context()->GetDataPath(bucket_locator),
                                        /*create_if_missing=*/true);
  EXPECT_TRUE(bucket_state_handle.IsHeld()) << s.ToString();

  StorageBucketFromHandle(bucket_state_handle)->ForceClose();
  bucket_state_handle.Release();

  EXPECT_TRUE(factory()->GetBucketFactory(bucket_locator.id));
  RunPostedTasks();
  EXPECT_FALSE(factory()->GetBucketFactory(bucket_locator.id));
}

TEST_F(IndexedDBFactoryTest, ConnectionForceClose) {
  SetupContext();
  const blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://localhost:81");
  auto bucket_locator = storage::BucketLocator();
  bucket_locator.storage_key = storage_key;

  auto callbacks = base::MakeRefCounted<MockIndexedDBCallbacks>();
  auto db_callbacks = base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();

  const int64_t transaction_id = 1;
  auto create_transaction_callback =
      base::BindOnce(&CreateAndBindTransactionPlaceholder);
  auto connection = std::make_unique<IndexedDBPendingConnection>(
      callbacks, db_callbacks,
      transaction_id, IndexedDBDatabaseMetadata::DEFAULT_VERSION,
      std::move(create_transaction_callback));
  factory()->Open(u"db", std::move(connection), bucket_locator,
                  context()->GetDataPath(bucket_locator));
  EXPECT_FALSE(callbacks->connection());
  RunPostedTasks();
  EXPECT_TRUE(callbacks->connection());

  EXPECT_TRUE(factory()->GetBucketFactory(bucket_locator.id));
  EXPECT_FALSE(factory()->GetBucketFactory(bucket_locator.id)->IsClosing());

  callbacks->connection()->CloseAndReportForceClose();

  EXPECT_TRUE(factory()->GetBucketFactory(bucket_locator.id));
  EXPECT_TRUE(factory()->GetBucketFactory(bucket_locator.id)->IsClosing());

  EXPECT_TRUE(db_callbacks->forced_close_called());
}

TEST_F(IndexedDBFactoryTest, DatabaseForceCloseDuringUpgrade) {
  SetupContext();
  const blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://localhost:81");
  auto bucket_locator = storage::BucketLocator();
  bucket_locator.storage_key = storage_key;

  auto callbacks = base::MakeRefCounted<MockIndexedDBCallbacks>();
  auto db_callbacks = base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();

  const int64_t transaction_id = 1;
  auto create_transaction_callback =
      base::BindOnce(&CreateAndBindTransactionPlaceholder);
  auto connection = std::make_unique<IndexedDBPendingConnection>(
      callbacks, db_callbacks,
      transaction_id, IndexedDBDatabaseMetadata::NO_VERSION,
      std::move(create_transaction_callback));

  // Do the first half of the upgrade, and request the upgrade from renderer.
  {
    base::RunLoop loop;
    callbacks->CallOnUpgradeNeeded(
        base::BindLambdaForTesting([&]() { loop.Quit(); }));
    factory()->Open(u"db", std::move(connection), bucket_locator,
                    context()->GetDataPath(bucket_locator));
    loop.Run();
  }

  EXPECT_TRUE(callbacks->upgrade_called());
  ASSERT_TRUE(callbacks->connection());
  ASSERT_TRUE(callbacks->connection()->database());

  callbacks->connection()->database()->ForceCloseAndRunTasks();

  EXPECT_TRUE(db_callbacks->forced_close_called());
  // Since there are no more references the factory should be closing.
  EXPECT_TRUE(factory()->GetBucketFactory(bucket_locator.id));
  EXPECT_TRUE(factory()->GetBucketFactory(bucket_locator.id)->IsClosing());
}

TEST_F(IndexedDBFactoryTest, ConnectionCloseDuringUpgrade) {
  SetupContext();
  const blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://localhost:81");
  auto bucket_locator = storage::BucketLocator();
  bucket_locator.storage_key = storage_key;

  auto callbacks = base::MakeRefCounted<MockIndexedDBCallbacks>();
  auto db_callbacks = base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();

  const int64_t transaction_id = 1;
  auto create_transaction_callback =
      base::BindOnce(&CreateAndBindTransactionPlaceholder);
  auto connection = std::make_unique<IndexedDBPendingConnection>(
      callbacks, db_callbacks,
      transaction_id, IndexedDBDatabaseMetadata::NO_VERSION,
      std::move(create_transaction_callback));

  // Do the first half of the upgrade, and request the upgrade from renderer.
  {
    base::RunLoop loop;
    callbacks->CallOnUpgradeNeeded(
        base::BindLambdaForTesting([&]() { loop.Quit(); }));
    factory()->Open(u"db", std::move(connection), bucket_locator,
                    context()->GetDataPath(bucket_locator));
    loop.Run();
  }

  EXPECT_TRUE(callbacks->upgrade_called());
  ASSERT_TRUE(callbacks->connection());

  // Close the connection.
  callbacks->connection()->AbortTransactionsAndClose(
      IndexedDBConnection::CloseErrorHandling::kAbortAllReturnLastError);

  // Since there are no more references the factory should be closing.
  EXPECT_TRUE(factory()->GetBucketFactory(bucket_locator.id));
  EXPECT_TRUE(factory()->GetBucketFactory(bucket_locator.id)->IsClosing());
}

TEST_F(IndexedDBFactoryTest, DatabaseForceCloseWithFullConnection) {
  SetupContext();
  const blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://localhost:81");
  auto bucket_locator = storage::BucketLocator();
  bucket_locator.storage_key = storage_key;

  std::unique_ptr<IndexedDBConnection> connection;
  scoped_refptr<MockIndexedDBDatabaseCallbacks> db_callbacks;
  std::tie(connection, db_callbacks) =
      CreateConnectionForDatatabase(bucket_locator, u"db");

  // Force close the database.
  connection->database()->ForceCloseAndRunTasks();

  EXPECT_TRUE(db_callbacks->forced_close_called());
  // Since there are no more references the factory should be closing.
  EXPECT_TRUE(factory()->GetBucketFactory(bucket_locator.id));
  EXPECT_TRUE(factory()->GetBucketFactory(bucket_locator.id)->IsClosing());
}

TEST_F(IndexedDBFactoryTest, DeleteDatabase) {
  SetupContext();

  auto callbacks = base::MakeRefCounted<MockIndexedDBCallbacks>(
      /*expect_connection=*/false);

  const blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://localhost:81");
  auto bucket_locator = storage::BucketLocator();
  bucket_locator.storage_key = storage_key;

  factory()->DeleteDatabase(u"db", callbacks, bucket_locator,
                            context()->GetDataPath(bucket_locator),
                            /*force_close=*/false);

  // Since there are no more references the factory should be closing.
  EXPECT_TRUE(factory()->GetBucketFactory(bucket_locator.id));
  EXPECT_TRUE(factory()->GetBucketFactory(bucket_locator.id)->IsClosing());
}

TEST_F(IndexedDBFactoryTest, DeleteDatabaseWithForceClose) {
  SetupContext();

  const blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://localhost:81");
  auto bucket_locator = storage::BucketLocator();
  bucket_locator.storage_key = storage_key;
  const std::u16string name = u"db";

  std::unique_ptr<IndexedDBConnection> connection;
  scoped_refptr<MockIndexedDBDatabaseCallbacks> db_callbacks;
  std::tie(connection, db_callbacks) =
      CreateConnectionForDatatabase(bucket_locator, name);

  base::RunLoop run_loop;
  factory()->CallOnDatabaseDeletedForTesting(base::BindLambdaForTesting(
      [&bucket_locator,
       &run_loop](const storage::BucketLocator& deleted_bucket_locator) {
        if (deleted_bucket_locator == bucket_locator)
          run_loop.Quit();
      }));

  auto callbacks = base::MakeRefCounted<MockIndexedDBCallbacks>(
      /*expect_connection=*/false);

  factory()->DeleteDatabase(name, callbacks, bucket_locator,
                            context()->GetDataPath(bucket_locator),
                            /*force_close=*/true);

  // Force close means the connection has been force closed, but the factory
  // isn't force closed, and instead is going through it's shutdown sequence.
  EXPECT_FALSE(connection->IsConnected());
  EXPECT_TRUE(db_callbacks->forced_close_called());
  EXPECT_TRUE(factory()->GetBucketFactory(bucket_locator.id));
  EXPECT_TRUE(factory()->GetBucketFactory(bucket_locator.id)->IsClosing());

  // Wait until the DB is deleted before tearing down since these concurrent
  // operations may conflict.
  run_loop.Run();
}

TEST_F(IndexedDBFactoryTest, GetDatabaseNames_NoFactory) {
  SetupContext();

  auto callbacks = base::MakeRefCounted<MockIndexedDBCallbacks>(
      /*expect_connection=*/false);

  const blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://localhost:81");
  auto bucket_locator = storage::BucketLocator();
  bucket_locator.storage_key = storage_key;

  factory()->GetDatabaseInfo(callbacks, bucket_locator,
                             context()->GetDataPath(bucket_locator));

  EXPECT_TRUE(callbacks->info_called());
  // Don't create a factory if one doesn't exist.
  EXPECT_FALSE(factory()->GetBucketFactory(bucket_locator.id));
}

TEST_F(IndexedDBFactoryTest, GetDatabaseNames_ExistingFactory) {
  SetupContext();

  auto callbacks = base::MakeRefCounted<MockIndexedDBCallbacks>(
      /*expect_connection=*/false);

  const blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://localhost:81");
  auto bucket_locator = storage::BucketLocator();
  bucket_locator.storage_key = storage_key;
  IndexedDBBucketStateHandle bucket_state_handle;
  leveldb::Status s;

  std::tie(bucket_state_handle, s, std::ignore, std::ignore, std::ignore) =
      factory()->GetOrOpenBucketFactory(bucket_locator,
                                        context()->GetDataPath(bucket_locator),
                                        /*create_if_missing=*/true);
  EXPECT_TRUE(bucket_state_handle.IsHeld()) << s.ToString();

  factory()->GetDatabaseInfo(callbacks, bucket_locator,
                             context()->GetDataPath(bucket_locator));

  EXPECT_TRUE(callbacks->info_called());
  EXPECT_TRUE(factory()->GetBucketFactory(bucket_locator.id));
  // GetDatabaseInfo didn't create the factory, so it shouldn't close it.
  EXPECT_FALSE(factory()->GetBucketFactory(bucket_locator.id)->IsClosing());
}

class LookingForQuotaErrorMockCallbacks : public IndexedDBCallbacks {
 public:
  LookingForQuotaErrorMockCallbacks()
      : IndexedDBCallbacks(nullptr,
                           absl::nullopt,
                           mojo::NullAssociatedRemote(),
                           base::SequencedTaskRunner::GetCurrentDefault()) {}

  LookingForQuotaErrorMockCallbacks(const LookingForQuotaErrorMockCallbacks&) =
      delete;
  LookingForQuotaErrorMockCallbacks& operator=(
      const LookingForQuotaErrorMockCallbacks&) = delete;

  void OnError(const IndexedDBDatabaseError& error) override {
    error_called_ = true;
    EXPECT_EQ(blink::mojom::IDBException::kQuotaError, error.code());
  }
  bool error_called() const { return error_called_; }

 private:
  ~LookingForQuotaErrorMockCallbacks() override = default;
  bool error_called_ = false;
};

TEST_F(IndexedDBFactoryTest, QuotaErrorOnDiskFull) {
  FakeLevelDBFactory fake_ldb_factory(
      IndexedDBClassFactory::GetLevelDBOptions(), "indexed-db");
  fake_ldb_factory.EnqueueNextOpenLevelDBStateResult(
      nullptr, leveldb::Status::IOError("Disk is full."), true);
  SetupContextWithFactories(&fake_ldb_factory,
                            base::DefaultClock::GetInstance());

  auto callbacks = base::MakeRefCounted<LookingForQuotaErrorMockCallbacks>();
  auto dummy_database_callbacks =
      base::MakeRefCounted<IndexedDBDatabaseCallbacks>(
          nullptr, mojo::NullAssociatedRemote(), context()->IDBTaskRunner());
  const blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://localhost:81");
  auto bucket_locator = storage::BucketLocator();
  bucket_locator.storage_key = storage_key;
  const std::u16string name(u"name");
  auto create_transaction_callback =
      base::BindOnce(&CreateAndBindTransactionPlaceholder);
  auto connection = std::make_unique<IndexedDBPendingConnection>(
      callbacks, dummy_database_callbacks,
      /*transaction_id=*/1, /*version=*/1,
      std::move(create_transaction_callback));
  factory()->Open(name, std::move(connection), bucket_locator,
                  context()->GetDataPath(bucket_locator));
  EXPECT_TRUE(callbacks->error_called());
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1U, quota_manager()->write_error_tracker().size());
  EXPECT_EQ(storage_key, quota_manager()->write_error_tracker().begin()->first);
  EXPECT_EQ(1, quota_manager()->write_error_tracker().begin()->second);
}

TEST_F(IndexedDBFactoryTest, NotifyQuotaOnDatabaseError) {
  SetupContext();
  const blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting("www.example.com");
  auto bucket_locator = storage::BucketLocator();
  bucket_locator.storage_key = storage_key;
  factory()->OnDatabaseError(bucket_locator,
                             leveldb::Status::Corruption("Corrupted stuff."),
                             "Corrupted stuff.");
  base::RunLoop().RunUntilIdle();
  // Quota should not be notified unless the status is IOError.
  ASSERT_EQ(0U, quota_manager()->write_error_tracker().size());

  factory()->OnDatabaseError(bucket_locator,
                             leveldb::Status::IOError("Disk is full."),
                             "Disk is full.");
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1U, quota_manager()->write_error_tracker().size());
  EXPECT_EQ(storage_key, quota_manager()->write_error_tracker().begin()->first);
  EXPECT_EQ(1, quota_manager()->write_error_tracker().begin()->second);
}

class ErrorCallbacks : public MockIndexedDBCallbacks {
 public:
  ErrorCallbacks() : MockIndexedDBCallbacks(false) {}

  ErrorCallbacks(const ErrorCallbacks&) = delete;
  ErrorCallbacks& operator=(const ErrorCallbacks&) = delete;

  void OnError(const IndexedDBDatabaseError& error) override {
    saw_error_ = true;
  }
  bool saw_error() const { return saw_error_; }

 private:
  ~ErrorCallbacks() override = default;
  bool saw_error_ = false;
};

TEST_F(IndexedDBFactoryTest, DatabaseFailedOpen) {
  SetupContext();
  const blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://localhost:81");
  auto bucket_locator = storage::BucketLocator();
  bucket_locator.storage_key = storage_key;
  const std::u16string db_name(u"db");
  const int64_t transaction_id = 1;

  auto callbacks = base::MakeRefCounted<MockIndexedDBCallbacks>();
  auto db_callbacks = base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();
  auto failed_open_callbacks = base::MakeRefCounted<ErrorCallbacks>();
  auto db_callbacks2 = base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();

  // Open at version 2.
  {
    const int64_t db_version = 2;
    auto create_transaction_callback =
        base::BindOnce(&CreateAndBindTransactionPlaceholder);

    auto connection = std::make_unique<IndexedDBPendingConnection>(
        callbacks, db_callbacks, transaction_id, db_version,
        std::move(create_transaction_callback));
    {
      base::RunLoop loop;
      callbacks->CallOnUpgradeNeeded(
          base::BindLambdaForTesting([&]() { loop.Quit(); }));
      factory()->Open(db_name, std::move(connection), bucket_locator,
                      context()->GetDataPath(bucket_locator));
      loop.Run();
    }
    EXPECT_TRUE(callbacks->upgrade_called());
    EXPECT_TRUE(factory()->IsDatabaseOpen(bucket_locator, db_name));
  }

  // Finish connecting, then close the connection.
  {
    base::RunLoop loop;
    callbacks->CallOnDBSuccess(
        base::BindLambdaForTesting([&]() { loop.Quit(); }));
    EXPECT_TRUE(callbacks->connection());
    callbacks->connection()->database()->Commit(
        callbacks->connection()->GetTransaction(transaction_id));
    loop.Run();
    callbacks->connection()->AbortTransactionsAndClose(
        IndexedDBConnection::CloseErrorHandling::kAbortAllReturnLastError);
    RunPostedTasks();
    EXPECT_FALSE(factory()->IsDatabaseOpen(bucket_locator, db_name));
  }

  // Open at version < 2, which will fail.
  {
    const int64_t db_version = 1;
    auto create_transaction_callback =
        base::BindOnce(&CreateAndBindTransactionPlaceholder);
    auto connection = std::make_unique<IndexedDBPendingConnection>(
        failed_open_callbacks, db_callbacks2,
        transaction_id, db_version, std::move(create_transaction_callback));
    factory()->Open(db_name, std::move(connection), bucket_locator,
                    context()->GetDataPath(bucket_locator));
    EXPECT_TRUE(factory()->IsDatabaseOpen(bucket_locator, db_name));
    RunPostedTasks();
    EXPECT_TRUE(failed_open_callbacks->saw_error());
    EXPECT_FALSE(factory()->IsDatabaseOpen(bucket_locator, db_name));
  }
}

namespace {

class DataLossCallbacks final : public MockIndexedDBCallbacks {
 public:
  blink::mojom::IDBDataLoss data_loss() const { return data_loss_; }

  void OnError(const IndexedDBDatabaseError& error) final {
    ADD_FAILURE() << "Unexpected IDB error: " << error.message();
  }
  void OnUpgradeNeeded(int64_t old_version,
                       std::unique_ptr<IndexedDBConnection> connection,
                       const IndexedDBDatabaseMetadata& metadata,
                       const IndexedDBDataLossInfo& data_loss) final {
    data_loss_ = data_loss.status;
    MockIndexedDBCallbacks::OnUpgradeNeeded(old_version, std::move(connection),
                                            metadata, data_loss);
  }

 private:
  ~DataLossCallbacks() final = default;
  blink::mojom::IDBDataLoss data_loss_ = blink::mojom::IDBDataLoss::None;
};

TEST_F(IndexedDBFactoryTest, DataFormatVersion) {
  SetupContext();
  auto try_open = [this](const storage::BucketLocator& bucket_locator,
                         const IndexedDBDataFormatVersion& version) {
    base::AutoReset<IndexedDBDataFormatVersion> override_version(
        &IndexedDBDataFormatVersion::GetMutableCurrentForTesting(), version);

    const int64_t transaction_id = 1;
    auto callbacks = base::MakeRefCounted<DataLossCallbacks>();
    auto db_callbacks = base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();
    auto create_transaction_callback =
        base::BindOnce(&CreateAndBindTransactionPlaceholder);
    auto pending_connection = std::make_unique<IndexedDBPendingConnection>(
        callbacks, db_callbacks,
        transaction_id,
        /*version=*/1, std::move(create_transaction_callback));

    {
      base::RunLoop loop;
      bool upgraded = false;
      // The database might already exist. Wait until either a success or an
      // ugprade request.
      callbacks->CallOnUpgradeNeeded(base::BindLambdaForTesting([&]() {
        upgraded = true;
        loop.Quit();
      }));
      callbacks->CallOnDBSuccess(
          base::BindLambdaForTesting([&]() { loop.Quit(); }));

      this->factory()->Open(u"test_db", std::move(pending_connection),
                            bucket_locator,
                            context()->GetDataPath(bucket_locator));
      loop.Run();

      // If an upgrade was requested, then commit the upgrade transaction.
      if (upgraded) {
        EXPECT_TRUE(callbacks->upgrade_called());
        EXPECT_TRUE(callbacks->connection());
        EXPECT_TRUE(callbacks->connection()->database());
        // Finish the upgrade by committing the transaction.
        auto* connection = callbacks->connection();
        {
          base::RunLoop inner_loop;
          callbacks->CallOnDBSuccess(
              base::BindLambdaForTesting([&]() { inner_loop.Quit(); }));
          connection->database()->Commit(
              connection->GetTransaction(transaction_id));
          inner_loop.Run();
        }
      }
    }
    RunPostedTasks();
    factory()->ForceClose(bucket_locator.id, false);
    RunPostedTasks();
    return callbacks->data_loss();
  };

  static const struct {
    const char* origin;
    IndexedDBDataFormatVersion open_version_1;
    IndexedDBDataFormatVersion open_version_2;
    blink::mojom::IDBDataLoss expected_data_loss;
  } kTestCases[] = {{"http://blink-downgrade.com/",
                     {3, 4},
                     {3, 3},
                     blink::mojom::IDBDataLoss::Total}};
  for (const auto& test : kTestCases) {
    SCOPED_TRACE(test.origin);
    const blink::StorageKey storage_key =
        blink::StorageKey::CreateFromStringForTesting(test.origin);
    auto bucket_locator = storage::BucketLocator();
    bucket_locator.storage_key = storage_key;
    ASSERT_EQ(blink::mojom::IDBDataLoss::None,
              try_open(bucket_locator, test.open_version_1));
    EXPECT_EQ(test.expected_data_loss,
              try_open(bucket_locator, test.open_version_2));
  }
}

}  // namespace
}  // namespace content
