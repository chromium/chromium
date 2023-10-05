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
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/default_clock.h"
#include "components/services/storage/filesystem_proxy_factory.h"
#include "components/services/storage/indexed_db/leveldb/fake_leveldb_factory.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_database.h"
#include "components/services/storage/privileged/mojom/indexed_db_control.mojom-test-utils.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/mojom/storage_usage_info.mojom.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_bucket_context.h"
#include "content/browser/indexed_db/indexed_db_class_factory.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_data_format_version.h"
#include "content/browser/indexed_db/indexed_db_factory.h"
#include "content/browser/indexed_db/indexed_db_pre_close_task_queue.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "content/browser/indexed_db/mock_indexed_db_database_callbacks.h"
#include "content/browser/indexed_db/mock_indexed_db_factory_client.h"
#include "content/browser/indexed_db/mock_mojo_indexed_db_database_callbacks.h"
#include "content/browser/indexed_db/mock_mojo_indexed_db_factory_client.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "net/base/features.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

using blink::IndexedDBDatabaseMetadata;
using testing::_;
using url::Origin;

namespace content {

namespace {

ACTION_TEMPLATE(MoveArgPointee,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(out)) {
  *out = std::move(*::testing::get<k>(args));
}

storage::BucketInfo ToBucketInfo(const storage::BucketLocator& bucket_locator) {
  storage::BucketInfo bucket_info;
  bucket_info.id = bucket_locator.id;
  bucket_info.storage_key = bucket_locator.storage_key;
  bucket_info.name = storage::kDefaultBucketName;
  return bucket_info;
}

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
      IndexedDBFactory* factory = context_->GetIDBFactory();

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
        base::test::TestFuture<bool> success;
        context_->DeleteForStorageKey(bucket_locator.storage_key,
                                      success.GetCallback());
        EXPECT_TRUE(success.Get());
      }
    }
    IndexedDBClassFactory::Get()->SetLevelDBFactoryForTesting(nullptr);
    quota_manager_.reset();
    // Wait for mojo pipes to flush or there may be leaks.
    task_environment_->RunUntilIdle();
  }

  void SetUpContext() {
    context_ = base::MakeRefCounted<IndexedDBContextImpl>(
        temp_dir_.GetPath(), quota_manager_proxy_.get(),
        base::DefaultClock::GetInstance(),
        /*blob_storage_context=*/mojo::NullRemote(),
        /*file_system_access_context=*/mojo::NullRemote(),
        base::SequencedTaskRunner::GetCurrentDefault(),
        base::SequencedTaskRunner::GetCurrentDefault());
  }

  void SetUpInMemoryContext() {
    context_ = base::MakeRefCounted<IndexedDBContextImpl>(
        base::FilePath(), quota_manager_proxy_.get(),
        base::DefaultClock::GetInstance(),
        /*blob_storage_context=*/mojo::NullRemote(),
        /*file_system_access_context=*/mojo::NullRemote(),
        base::SequencedTaskRunner::GetCurrentDefault(),
        base::SequencedTaskRunner::GetCurrentDefault());
  }

  void SetUpContextWithFactories(LevelDBFactory* factory, base::Clock* clock) {
    context_ = base::MakeRefCounted<IndexedDBContextImpl>(
        temp_dir_.GetPath(), quota_manager_proxy_.get(), clock,
        /*blob_storage_context=*/mojo::NullRemote(),
        /*file_system_access_context=*/mojo::NullRemote(),
        base::SequencedTaskRunner::GetCurrentDefault(),
        base::SequencedTaskRunner::GetCurrentDefault());
    if (factory)
      IndexedDBClassFactory::Get()->SetLevelDBFactoryForTesting(factory);
  }

  void RunPostedTasks() {
    base::RunLoop loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, loop.QuitClosure());
    loop.Run();
  }

  storage::BucketInfo GetOrCreateBucket(
      const storage::BucketInitParams& params) {
    base::test::TestFuture<storage::QuotaErrorOr<storage::BucketInfo>> future;
    quota_manager_proxy_->UpdateOrCreateBucket(
        params, base::SingleThreadTaskRunner::GetCurrentDefault(),
        future.GetCallback());
    return future.Take().value();
  }

  void BindIndexedDBFactory(
      mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
          checker_remote,
      mojo::PendingReceiver<blink::mojom::IDBFactory> receiver,
      storage::QuotaErrorOr<storage::BucketInfo> bucket_info) {
    context()->BindIndexedDBImpl(std::move(checker_remote), std::move(receiver),
                                 bucket_info);
  }

 protected:
  IndexedDBContextImpl* context() const { return context_.get(); }

  IndexedDBFactory* factory() const { return context_->GetIDBFactory(); }

  base::test::TaskEnvironment* task_environment() const {
    return task_environment_.get();
  }

  IndexedDBBucketContext* StorageBucketFromHandle(
      IndexedDBBucketContextHandle& handle) {
    return handle.bucket_context();
  }

  storage::MockQuotaManager* quota_manager() { return quota_manager_.get(); }

  std::unique_ptr<base::test::TaskEnvironment> task_environment_;

  base::ScopedTempDir temp_dir_;
  scoped_refptr<storage::MockSpecialStoragePolicy> quota_policy_;
  scoped_refptr<storage::MockQuotaManager> quota_manager_;
  scoped_refptr<storage::MockQuotaManagerProxy> quota_manager_proxy_;
  scoped_refptr<IndexedDBContextImpl> context_;
  std::unique_ptr<MockIndexedDBFactoryClient> mock_factory_client_;
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
  SetUpContext();

  const blink::StorageKey storage_key_1 =
      blink::StorageKey::CreateFromStringForTesting("http://localhost:81");
  storage::BucketInfo bucket_1 = GetOrCreateBucket(
      storage::BucketInitParams::ForDefaultBucket(storage_key_1));
  storage::BucketLocator bucket_locator_1 = bucket_1.ToBucketLocator();
  auto file_1 = context_->GetLevelDBPathForTesting(bucket_locator_1)
                    .AppendASCII("1.json");
  ASSERT_TRUE(CreateDirectory(file_1.DirName()));
  ASSERT_TRUE(base::WriteFile(file_1, std::string(10, 'a')));

  const blink::StorageKey storage_key_2 =
      blink::StorageKey::CreateFromStringForTesting("http://localhost:82");
  storage::BucketInfo bucket_2 = GetOrCreateBucket(
      storage::BucketInitParams::ForDefaultBucket(storage_key_2));
  storage::BucketLocator bucket_locator_2 = bucket_2.ToBucketLocator();
  auto file_2 = context_->GetLevelDBPathForTesting(bucket_locator_2)
                    .AppendASCII("2.json");
  ASSERT_TRUE(CreateDirectory(file_2.DirName()));
  ASSERT_TRUE(base::WriteFile(file_2, std::string(100, 'a')));

  const blink::StorageKey storage_key_3 =
      blink::StorageKey::CreateFromStringForTesting("http://localhost2:82");
  storage::BucketInfo bucket_3 = GetOrCreateBucket(
      storage::BucketInitParams::ForDefaultBucket(storage_key_3));
  storage::BucketLocator bucket_locator_3 = bucket_3.ToBucketLocator();
  auto file_3 = context_->GetLevelDBPathForTesting(bucket_locator_3)
                    .AppendASCII("3.json");
  ASSERT_TRUE(CreateDirectory(file_3.DirName()));
  ASSERT_TRUE(base::WriteFile(file_3, std::string(1000, 'a')));

  const blink::StorageKey storage_key_4 = blink::StorageKey::Create(
      storage_key_1.origin(), net::SchemefulSite(storage_key_3.origin()),
      blink::mojom::AncestorChainBit::kCrossSite);
  storage::BucketInfo bucket_4 = GetOrCreateBucket(
      storage::BucketInitParams::ForDefaultBucket(storage_key_4));
  storage::BucketLocator bucket_locator_4 = bucket_4.ToBucketLocator();
  auto file_4 = context_->GetLevelDBPathForTesting(bucket_locator_4)
                    .AppendASCII("4.json");
  ASSERT_TRUE(CreateDirectory(file_4.DirName()));
  ASSERT_TRUE(base::WriteFile(file_4, std::string(10000, 'a')));

  const blink::StorageKey storage_key_5 = storage_key_1;
  storage::BucketInitParams params(storage_key_5, "inbox");
  storage::BucketInfo bucket_5 = GetOrCreateBucket(params);
  storage::BucketLocator bucket_locator_5 = bucket_5.ToBucketLocator();
  auto file_5 = context_->GetLevelDBPathForTesting(bucket_locator_5)
                    .AppendASCII("5.json");
  ASSERT_TRUE(CreateDirectory(file_5.DirName()));
  ASSERT_TRUE(base::WriteFile(file_5, std::string(20000, 'a')));
  EXPECT_NE(file_5.DirName(), file_1.DirName());

  IndexedDBBucketContextHandle bucket_context1_handle;
  IndexedDBBucketContextHandle bucket_context2_handle;
  IndexedDBBucketContextHandle bucket_context3_handle;
  IndexedDBBucketContextHandle bucket_context4_handle;
  IndexedDBBucketContextHandle bucket_context5_handle;
  leveldb::Status s;

  std::tie(bucket_context1_handle, s, std::ignore, std::ignore, std::ignore) =
      factory()->GetOrCreateBucketContext(
          bucket_1, context()->GetDataPath(bucket_locator_1),
          /*create_if_missing=*/true);
  EXPECT_TRUE(bucket_context1_handle.IsHeld()) << s.ToString();
  EXPECT_TRUE(s.ok()) << s.ToString();

  std::tie(bucket_context2_handle, s, std::ignore, std::ignore, std::ignore) =
      factory()->GetOrCreateBucketContext(
          bucket_2, context()->GetDataPath(bucket_locator_2),
          /*create_if_missing=*/true);
  EXPECT_TRUE(bucket_context2_handle.IsHeld()) << s.ToString();
  EXPECT_TRUE(s.ok()) << s.ToString();

  std::tie(bucket_context3_handle, s, std::ignore, std::ignore, std::ignore) =
      factory()->GetOrCreateBucketContext(
          bucket_3, context()->GetDataPath(bucket_locator_3),
          /*create_if_missing=*/true);
  EXPECT_TRUE(bucket_context3_handle.IsHeld()) << s.ToString();
  EXPECT_TRUE(s.ok()) << s.ToString();

  std::tie(bucket_context4_handle, s, std::ignore, std::ignore, std::ignore) =
      factory()->GetOrCreateBucketContext(
          bucket_4, context()->GetDataPath(bucket_locator_4),
          /*create_if_missing=*/true);
  EXPECT_TRUE(bucket_context4_handle.IsHeld()) << s.ToString();
  EXPECT_TRUE(s.ok()) << s.ToString();

  std::tie(bucket_context5_handle, s, std::ignore, std::ignore, std::ignore) =
      factory()->GetOrCreateBucketContext(
          bucket_5, context()->GetDataPath(bucket_locator_5),
          /*create_if_missing=*/true);
  EXPECT_TRUE(bucket_context5_handle.IsHeld()) << s.ToString();
  EXPECT_TRUE(s.ok()) << s.ToString();

  base::test::TestFuture<std::vector<storage::mojom::StorageUsageInfoPtr>>
      infos_future;
  context()->GetUsage(infos_future.GetCallback());
  auto infos = infos_future.Take();

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
  SetUpContext();

  const blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://localhost:81");
  auto bucket_locator = storage::BucketLocator();
  bucket_locator.storage_key = storage_key;
  IndexedDBBucketContextHandle bucket_context_handle;
  leveldb::Status s;

  std::tie(bucket_context_handle, s, std::ignore, std::ignore, std::ignore) =
      factory()->GetOrCreateBucketContext(
          ToBucketInfo(bucket_locator), context()->GetDataPath(bucket_locator),
          /*create_if_missing=*/true);
  EXPECT_TRUE(bucket_context_handle.IsHeld()) << s.ToString();
  bucket_context_handle.Release();

  EXPECT_TRUE(factory()->GetBucketContext(bucket_locator.id));
  EXPECT_TRUE(factory()->GetBucketContext(bucket_locator.id)->IsClosing());

  factory()->ForceClose(bucket_locator.id, false);
  RunPostedTasks();
  EXPECT_FALSE(factory()->GetBucketContext(bucket_locator.id));
}

TEST_F(IndexedDBFactoryTest, ImmediateClose) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      kIDBCloseImmediatelySwitch);
  SetUpContext();

  const blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://localhost:81");
  auto bucket_locator = storage::BucketLocator();
  bucket_locator.storage_key = storage_key;
  IndexedDBBucketContextHandle bucket_context_handle;
  leveldb::Status s;

  std::tie(bucket_context_handle, s, std::ignore, std::ignore, std::ignore) =
      factory()->GetOrCreateBucketContext(
          ToBucketInfo(bucket_locator), context()->GetDataPath(bucket_locator),
          /*create_if_missing=*/true);
  EXPECT_TRUE(bucket_context_handle.IsHeld()) << s.ToString();
  bucket_context_handle.Release();

  EXPECT_TRUE(factory()->GetBucketContext(bucket_locator.id));
  RunPostedTasks();
  EXPECT_FALSE(factory()->GetBucketContext(bucket_locator.id));
  EXPECT_EQ(0ul, factory()->GetOpenBuckets().size());
}

TEST_F(IndexedDBFactoryTestWithMockTime, PreCloseTasksStart) {
  base::SimpleTestClock clock;
  clock.SetNow(base::Time::Now());
  SetUpContextWithFactories(nullptr, &clock);

  const blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://localhost:81");
  auto bucket_locator = storage::BucketLocator();
  bucket_locator.storage_key = storage_key;
  IndexedDBBucketContextHandle bucket_context_handle;
  leveldb::Status s;

  // Open a connection & immediately release it to cause the closing sequence to
  // start.
  std::tie(bucket_context_handle, s, std::ignore, std::ignore, std::ignore) =
      factory()->GetOrCreateBucketContext(
          ToBucketInfo(bucket_locator), context()->GetDataPath(bucket_locator),
          /*create_if_missing=*/true);
  EXPECT_TRUE(bucket_context_handle.IsHeld()) << s.ToString();
  bucket_context_handle.Release();

  EXPECT_TRUE(factory()->GetBucketContext(bucket_locator.id));
  EXPECT_TRUE(factory()->GetBucketContext(bucket_locator.id)->IsClosing());

  EXPECT_EQ(IndexedDBBucketContext::ClosingState::kPreCloseGracePeriod,
            factory()->GetBucketContext(bucket_locator.id)->closing_stage());

  task_environment()->FastForwardBy(base::Seconds(2));

  // The factory should be closed, as the pre close tasks are delayed.
  EXPECT_FALSE(factory()->GetBucketContext(bucket_locator.id));

  // Move the clock to run the tasks in the next close sequence.
  // NOTE: The constants rate-limiting sweeps and compaction are currently the
  // same. This test may need to be restructured if these values diverge.
  clock.Advance(IndexedDBBucketContext::kMaxEarliestGlobalSweepFromNow);

  // Open a connection & immediately release it to cause the closing sequence to
  // start again.
  std::tie(bucket_context_handle, s, std::ignore, std::ignore, std::ignore) =
      factory()->GetOrCreateBucketContext(
          ToBucketInfo(bucket_locator), context()->GetDataPath(bucket_locator),
          /*create_if_missing=*/true);
  EXPECT_TRUE(bucket_context_handle.IsHeld()) << s.ToString();
  bucket_context_handle.Release();

  // Manually execute the timer so that the PreCloseTaskList task doesn't also
  // run.
  factory()->GetBucketContext(bucket_locator.id)->close_timer()->FireNow();

  // The pre-close tasks should be running now.
  ASSERT_TRUE(factory()->GetBucketContext(bucket_locator.id));
  EXPECT_EQ(IndexedDBBucketContext::ClosingState::kRunningPreCloseTasks,
            factory()->GetBucketContext(bucket_locator.id)->closing_stage());
  ASSERT_TRUE(
      factory()->GetBucketContext(bucket_locator.id)->pre_close_task_queue());
  EXPECT_TRUE(factory()
                  ->GetBucketContext(bucket_locator.id)
                  ->pre_close_task_queue()
                  ->started());

  // Stop sweep by opening a connection.
  std::tie(bucket_context_handle, s, std::ignore, std::ignore, std::ignore) =
      factory()->GetOrCreateBucketContext(
          ToBucketInfo(bucket_locator), context()->GetDataPath(bucket_locator),
          /*create_if_missing=*/true);
  EXPECT_TRUE(bucket_context_handle.IsHeld()) << s.ToString();
  EXPECT_FALSE(
      StorageBucketFromHandle(bucket_context_handle)->pre_close_task_queue());
  bucket_context_handle.Release();

  // Move clock forward to trigger next sweep, but storage key has longer
  // sweep minimum, so no tasks should execute.
  clock.Advance(IndexedDBBucketContext::kMaxEarliestGlobalSweepFromNow);

  bucket_context_handle.Release();
  EXPECT_TRUE(factory()->GetBucketContext(bucket_locator.id));
  EXPECT_EQ(IndexedDBBucketContext::ClosingState::kPreCloseGracePeriod,
            factory()->GetBucketContext(bucket_locator.id)->closing_stage());

  // Manually execute the timer so that the PreCloseTaskList task doesn't also
  // run.
  factory()->GetBucketContext(bucket_locator.id)->close_timer()->FireNow();
  EXPECT_TRUE(factory()->GetBucketContext(bucket_locator.id));
  RunPostedTasks();
  EXPECT_FALSE(factory()->GetBucketContext(bucket_locator.id));

  //  Finally, move the clock forward so the storage key should allow a sweep.
  clock.Advance(IndexedDBBucketContext::kMaxEarliestBucketSweepFromNow);
  std::tie(bucket_context_handle, s, std::ignore, std::ignore, std::ignore) =
      factory()->GetOrCreateBucketContext(
          ToBucketInfo(bucket_locator), context()->GetDataPath(bucket_locator),
          /*create_if_missing=*/true);
  bucket_context_handle.Release();
  factory()->GetBucketContext(bucket_locator.id)->close_timer()->FireNow();

  ASSERT_TRUE(factory()->GetBucketContext(bucket_locator.id));
  EXPECT_EQ(IndexedDBBucketContext::ClosingState::kRunningPreCloseTasks,
            factory()->GetBucketContext(bucket_locator.id)->closing_stage());
  ASSERT_TRUE(
      factory()->GetBucketContext(bucket_locator.id)->pre_close_task_queue());
  EXPECT_TRUE(factory()
                  ->GetBucketContext(bucket_locator.id)
                  ->pre_close_task_queue()
                  ->started());
}

TEST_F(IndexedDBFactoryTestWithMockTime, TombstoneSweeperTiming) {
  base::SimpleTestClock clock;
  clock.SetNow(base::Time::Now());
  SetUpContextWithFactories(nullptr, &clock);

  const blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://localhost:81");
  auto bucket_locator = storage::BucketLocator();
  bucket_locator.storage_key = storage_key;
  IndexedDBBucketContextHandle bucket_context_handle;
  leveldb::Status s;

  // Open a connection & immediately release it to cause the closing sequence to
  // start.
  std::tie(bucket_context_handle, s, std::ignore, std::ignore, std::ignore) =
      factory()->GetOrCreateBucketContext(
          ToBucketInfo(bucket_locator), context()->GetDataPath(bucket_locator),
          /*create_if_missing=*/true);
  EXPECT_TRUE(bucket_context_handle.IsHeld()) << s.ToString();

  // The factory should be closed, as the pre close tasks are delayed.
  EXPECT_FALSE(bucket_context_handle->ShouldRunTombstoneSweeper());

  // Move the clock to run the tasks in the next close sequence.
  clock.Advance(IndexedDBBucketContext::kMaxEarliestGlobalSweepFromNow);

  EXPECT_TRUE(bucket_context_handle->ShouldRunTombstoneSweeper());

  // Move clock forward to trigger next sweep, but storage key has longer
  // sweep minimum, so no tasks should execute.
  clock.Advance(IndexedDBBucketContext::kMaxEarliestGlobalSweepFromNow);

  EXPECT_FALSE(bucket_context_handle->ShouldRunTombstoneSweeper());

  //  Finally, move the clock forward so the storage key should allow a sweep.
  clock.Advance(IndexedDBBucketContext::kMaxEarliestBucketSweepFromNow);

  EXPECT_TRUE(bucket_context_handle->ShouldRunTombstoneSweeper());
}

TEST_F(IndexedDBFactoryTestWithMockTime, CompactionTaskTiming) {
  base::SimpleTestClock clock;
  clock.SetNow(base::Time::Now());
  SetUpContextWithFactories(nullptr, &clock);

  const blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://localhost:81");
  auto bucket_locator = storage::BucketLocator();
  bucket_locator.storage_key = storage_key;
  IndexedDBBucketContextHandle bucket_context_handle;
  leveldb::Status s;

  // Open a connection & immediately release it to cause the closing sequence to
  // start.
  std::tie(bucket_context_handle, s, std::ignore, std::ignore, std::ignore) =
      factory()->GetOrCreateBucketContext(
          ToBucketInfo(bucket_locator), context()->GetDataPath(bucket_locator),
          /*create_if_missing=*/true);
  EXPECT_TRUE(bucket_context_handle.IsHeld()) << s.ToString();

  // The factory should be closed, as the pre close tasks are delayed.
  EXPECT_FALSE(bucket_context_handle->ShouldRunCompaction());

  // Move the clock to run the tasks in the next close sequence.
  clock.Advance(IndexedDBBucketContext::kMaxEarliestGlobalCompactionFromNow);

  EXPECT_TRUE(bucket_context_handle->ShouldRunCompaction());

  // Move clock forward to trigger next compaction, but storage key has longer
  // compaction minimum, so no tasks should execute.
  clock.Advance(IndexedDBBucketContext::kMaxEarliestGlobalCompactionFromNow);

  EXPECT_FALSE(bucket_context_handle->ShouldRunCompaction());

  //  Finally, move the clock forward so the storage key should allow a
  //  compaction.
  clock.Advance(IndexedDBBucketContext::kMaxEarliestBucketCompactionFromNow);

  EXPECT_TRUE(bucket_context_handle->ShouldRunCompaction());
}

TEST_F(IndexedDBFactoryTest, InMemoryFactoriesStay) {
  SetUpInMemoryContext();
  ASSERT_TRUE(context()->IsInMemoryContext());

  const blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://localhost:81");
  storage::BucketInfo bucket_info = GetOrCreateBucket(
      storage::BucketInitParams::ForDefaultBucket(storage_key));
  storage::BucketLocator bucket_locator = bucket_info.ToBucketLocator();
  IndexedDBBucketContextHandle bucket_context_handle;
  leveldb::Status s;

  std::tie(bucket_context_handle, s, std::ignore, std::ignore, std::ignore) =
      factory()->GetOrCreateBucketContext(
          bucket_info, context()->GetDataPath(bucket_locator),
          /*create_if_missing=*/true);
  EXPECT_TRUE(bucket_context_handle.IsHeld()) << s.ToString();
  EXPECT_TRUE(StorageBucketFromHandle(bucket_context_handle)
                  ->backing_store()
                  ->is_incognito());
  bucket_context_handle.Release();

  EXPECT_TRUE(factory()->GetBucketContext(bucket_locator.id));
  EXPECT_FALSE(factory()->GetBucketContext(bucket_locator.id)->IsClosing());

  factory()->ForceClose(bucket_locator.id, false);
  EXPECT_TRUE(factory()->GetBucketContext(bucket_locator.id));

  factory()->ForceClose(bucket_locator.id, true);
  EXPECT_FALSE(factory()->GetBucketContext(bucket_locator.id));
}

TEST_F(IndexedDBFactoryTest, TooLongOrigin) {
  SetUpContext();

  base::FilePath temp_dir =
      context()->GetFirstPartyDataPathForTesting().DirName();
  int limit = base::GetMaximumPathComponentLength(temp_dir);
  EXPECT_GT(limit, 0);

  std::string origin(limit + 1, 'x');
  const blink::StorageKey too_long_storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://" + origin +
                                                    ":81/");
  storage::BucketInfo bucket_info = GetOrCreateBucket(
      storage::BucketInitParams::ForDefaultBucket(too_long_storage_key));
  storage::BucketLocator bucket_locator = bucket_info.ToBucketLocator();
  IndexedDBBucketContextHandle bucket_context_handle;
  leveldb::Status s;

  std::tie(bucket_context_handle, s, std::ignore, std::ignore, std::ignore) =
      factory()->GetOrCreateBucketContext(
          bucket_info, context()->GetDataPath(bucket_locator),
          /*create_if_missing=*/true);
  EXPECT_FALSE(bucket_context_handle.IsHeld());
  EXPECT_TRUE(s.IsIOError());
}

TEST_F(IndexedDBFactoryTest, FactoryForceClose) {
  SetUpContext();
  const blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://localhost:81");
  auto bucket_locator = storage::BucketLocator();
  bucket_locator.storage_key = storage_key;
  IndexedDBBucketContextHandle bucket_context_handle;
  leveldb::Status s;

  std::tie(bucket_context_handle, s, std::ignore, std::ignore, std::ignore) =
      factory()->GetOrCreateBucketContext(
          ToBucketInfo(bucket_locator), context()->GetDataPath(bucket_locator),
          /*create_if_missing=*/true);
  EXPECT_TRUE(bucket_context_handle.IsHeld()) << s.ToString();

  StorageBucketFromHandle(bucket_context_handle)->ForceClose();
  bucket_context_handle.Release();

  EXPECT_TRUE(factory()->GetBucketContext(bucket_locator.id));
  RunPostedTasks();
  EXPECT_FALSE(factory()->GetBucketContext(bucket_locator.id));
}

// Tests that the backing store is closed when the connection is closed during
// upgrade.
TEST_F(IndexedDBFactoryTest, ConnectionCloseDuringUpgrade) {
  SetUpContext();

  const blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://localhost:81");
  auto bucket_locator = storage::BucketLocator();
  bucket_locator.storage_key = storage_key;

  // Bind the IDBFactory.
  mojo::Remote<blink::mojom::IDBFactory> factory_remote;
  mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
      checker_remote;
  BindIndexedDBFactory(std::move(checker_remote),
                       factory_remote.BindNewPipeAndPassReceiver(),
                       ToBucketInfo(bucket_locator));

  // Now create a database and thus the backing store.
  MockMojoIndexedDBFactoryClient client;
  MockMojoIndexedDBDatabaseCallbacks database_callbacks;
  base::RunLoop run_loop;
  mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_database;
  EXPECT_CALL(client, MockedUpgradeNeeded)
      .WillOnce(
          testing::DoAll(MoveArgPointee<0>(&pending_database),
                         ::base::test::RunClosure(run_loop.QuitClosure())));
  mojo::AssociatedRemote<blink::mojom::IDBTransaction> transaction_remote;
  factory_remote->Open(client.CreateInterfacePtrAndBind(),
                       database_callbacks.CreateInterfacePtrAndBind(), u"db",
                       /*version=*/1,
                       transaction_remote.BindNewEndpointAndPassReceiver(),
                       /*transaction_id=*/1);
  run_loop.Run();

  EXPECT_TRUE(factory()->GetBucketContext(bucket_locator.id));
  EXPECT_FALSE(factory()->GetBucketContext(bucket_locator.id)->IsClosing());

  // Drop the connection.
  pending_database.reset();
  factory_remote.FlushForTesting();
  EXPECT_TRUE(factory()->GetBucketContext(bucket_locator.id)->IsClosing());
}

TEST_F(IndexedDBFactoryTest, DeleteDatabase) {
  SetUpContext();
  const blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://localhost:81");
  auto bucket_locator = storage::BucketLocator();
  bucket_locator.storage_key = storage_key;

  // Bind the IDBFactory.
  mojo::Remote<blink::mojom::IDBFactory> factory_remote;
  mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
      checker_remote;
  BindIndexedDBFactory(std::move(checker_remote),
                       factory_remote.BindNewPipeAndPassReceiver(),
                       ToBucketInfo(bucket_locator));

  // Delete db.
  MockMojoIndexedDBFactoryClient client;
  MockMojoIndexedDBDatabaseCallbacks database_callbacks;
  base::RunLoop run_loop;
  EXPECT_CALL(client, DeleteSuccess)
      .WillOnce(
          testing::DoAll(::base::test::RunClosure(run_loop.QuitClosure())));
  mojo::AssociatedRemote<blink::mojom::IDBTransaction> transaction_remote;
  factory_remote->DeleteDatabase(client.CreateInterfacePtrAndBind(), u"db",
                                 /*force_close=*/false);
  run_loop.Run();

  // Since there are no more references the factory should be closing.
  EXPECT_TRUE(factory()->GetBucketContext(bucket_locator.id));
  EXPECT_TRUE(factory()->GetBucketContext(bucket_locator.id)->IsClosing());
}

TEST_F(IndexedDBFactoryTest, GetDatabaseNames_NoFactory) {
  SetUpContext();

  const blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://localhost:81");
  auto bucket_locator = storage::BucketLocator();
  bucket_locator.storage_key = storage_key;

  // Bind the IDBFactory.
  mojo::Remote<blink::mojom::IDBFactory> factory_remote;
  mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
      checker_remote;
  BindIndexedDBFactory(std::move(checker_remote),
                       factory_remote.BindNewPipeAndPassReceiver(),
                       ToBucketInfo(bucket_locator));

  // Don't create a backing store if one doesn't exist.
  {
    base::test::TestFuture<std::vector<blink::mojom::IDBNameAndVersionPtr>,
                           blink::mojom::IDBErrorPtr>
        info_future;
    factory_remote->GetDatabaseInfo(info_future.GetCallback());
    ASSERT_TRUE(info_future.Wait());
    EXPECT_FALSE(factory()->GetBucketContext(bucket_locator.id));
  }

  // Now create a database and thus the backing store.
  MockMojoIndexedDBFactoryClient client;
  MockMojoIndexedDBDatabaseCallbacks database_callbacks;
  base::RunLoop run_loop;
  // It's necessary to hang onto the database connection or the connection
  // will shut itself down and the backing store will close on its own.
  mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_database;
  EXPECT_CALL(client, MockedOpenSuccess)
      .WillOnce(
          testing::DoAll(MoveArgPointee<0>(&pending_database),
                         ::base::test::RunClosure(run_loop.QuitClosure())));
  mojo::AssociatedRemote<blink::mojom::IDBTransaction> transaction_remote;
  factory_remote->Open(client.CreateInterfacePtrAndBind(),
                       database_callbacks.CreateInterfacePtrAndBind(), u"db",
                       /*version=*/0,
                       transaction_remote.BindNewEndpointAndPassReceiver(),
                       /*transaction_id=*/1);
  run_loop.Run();

  // GetDatabaseInfo didn't create the factory, so it shouldn't close it.
  {
    base::test::TestFuture<std::vector<blink::mojom::IDBNameAndVersionPtr>,
                           blink::mojom::IDBErrorPtr>
        info_future;
    factory_remote->GetDatabaseInfo(info_future.GetCallback());
    ASSERT_TRUE(info_future.Wait());

    EXPECT_TRUE(factory()->GetBucketContext(bucket_locator.id));
    EXPECT_FALSE(factory()->GetBucketContext(bucket_locator.id)->IsClosing());
  }
}

TEST_F(IndexedDBFactoryTest, QuotaErrorOnDiskFull) {
  FakeLevelDBFactory fake_ldb_factory({}, "indexed-db");
  fake_ldb_factory.EnqueueNextOpenLevelDBStateResult(
      nullptr, leveldb::Status::IOError("Disk is full."), true);
  SetUpContextWithFactories(&fake_ldb_factory,
                            base::DefaultClock::GetInstance());

  // Bind the IDBFactory.
  const blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://localhost:81");
  auto bucket_locator = storage::BucketLocator();
  bucket_locator.storage_key = storage_key;
  mojo::Remote<blink::mojom::IDBFactory> factory_remote;
  mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
      checker_remote;
  BindIndexedDBFactory(std::move(checker_remote),
                       factory_remote.BindNewPipeAndPassReceiver(),
                       ToBucketInfo(bucket_locator));

  // Expect an error when opening.
  MockMojoIndexedDBFactoryClient client;
  MockMojoIndexedDBDatabaseCallbacks database_callbacks;
  base::RunLoop run_loop;
  EXPECT_CALL(client, Error)
      .WillOnce(
          testing::DoAll(::base::test::RunClosure(run_loop.QuitClosure())));
  mojo::AssociatedRemote<blink::mojom::IDBTransaction> transaction_remote;
  factory_remote->Open(client.CreateInterfacePtrAndBind(),
                       database_callbacks.CreateInterfacePtrAndBind(), u"db",
                       /*version=*/1,
                       transaction_remote.BindNewEndpointAndPassReceiver(),
                       /*transaction_id=*/1);
  run_loop.Run();

  ASSERT_EQ(1U, quota_manager()->write_error_tracker().size());
  EXPECT_EQ(storage_key, quota_manager()->write_error_tracker().begin()->first);
  EXPECT_EQ(1, quota_manager()->write_error_tracker().begin()->second);
}

TEST_F(IndexedDBFactoryTest, NotifyQuotaOnDatabaseError) {
  SetUpContext();
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

TEST_F(IndexedDBFactoryTest, DatabaseFailedOpen) {
  SetUpContext();
  const blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://localhost:81");
  auto bucket_locator = storage::BucketLocator();
  bucket_locator.storage_key = storage_key;
  const std::u16string db_name(u"db");

  // Bind the IDBFactory.
  mojo::Remote<blink::mojom::IDBFactory> factory_remote;
  mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
      checker_remote;
  BindIndexedDBFactory(std::move(checker_remote),
                       factory_remote.BindNewPipeAndPassReceiver(),
                       ToBucketInfo(bucket_locator));

  // Open at version 2.
  {
    const int64_t db_version = 2;
    MockMojoIndexedDBFactoryClient client;
    MockMojoIndexedDBDatabaseCallbacks database_callbacks;
    base::RunLoop run_loop;
    EXPECT_CALL(client, MockedUpgradeNeeded)
        .WillOnce(
            testing::DoAll(::base::test::RunClosure(run_loop.QuitClosure())));
    mojo::AssociatedRemote<blink::mojom::IDBTransaction> transaction_remote;
    factory_remote->Open(client.CreateInterfacePtrAndBind(),
                         database_callbacks.CreateInterfacePtrAndBind(),
                         db_name, db_version,
                         transaction_remote.BindNewEndpointAndPassReceiver(),
                         /*transaction_id=*/1);
    run_loop.Run();
  }

  // Open at version < 2, which will fail.
  {
    const int64_t db_version = 1;
    base::RunLoop run_loop;
    MockMojoIndexedDBFactoryClient client;
    MockMojoIndexedDBDatabaseCallbacks database_callbacks;
    EXPECT_CALL(client, Error)
        .WillOnce(::base::test::RunClosure(run_loop.QuitClosure()));
    mojo::AssociatedRemote<blink::mojom::IDBTransaction> transaction_remote;
    factory_remote->Open(client.CreateInterfacePtrAndBind(),
                         database_callbacks.CreateInterfacePtrAndBind(),
                         db_name, db_version,
                         transaction_remote.BindNewEndpointAndPassReceiver(),
                         /*transaction_id=*/2);
    run_loop.Run();
    EXPECT_FALSE(factory()->IsDatabaseOpen(bucket_locator, db_name));
  }
}

// Test for `IndexedDBDataFormatVersion`.
TEST_F(IndexedDBFactoryTest, DataLoss) {
  SetUpContext();
  const blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://localhost:81");
  auto bucket_locator = storage::BucketLocator();
  bucket_locator.storage_key = storage_key;
  const std::u16string db_name(u"test_db");

  // Bind the IDBFactory.
  mojo::Remote<blink::mojom::IDBFactory> factory_remote;
  mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
      checker_remote;
  BindIndexedDBFactory(std::move(checker_remote),
                       factory_remote.BindNewPipeAndPassReceiver(),
                       ToBucketInfo(bucket_locator));

  // Set a data format version and create a new database. No data loss.
  {
    base::AutoReset<IndexedDBDataFormatVersion> override_version(
        &IndexedDBDataFormatVersion::GetMutableCurrentForTesting(),
        IndexedDBDataFormatVersion(3, 4));
    MockMojoIndexedDBFactoryClient client;
    MockMojoIndexedDBDatabaseCallbacks database_callbacks;
    base::RunLoop run_loop;
    EXPECT_CALL(client, MockedUpgradeNeeded(
                            _, _, blink::mojom::IDBDataLoss::None, _, _))
        .WillOnce(
            testing::DoAll(::base::test::RunClosure(run_loop.QuitClosure())));
    mojo::AssociatedRemote<blink::mojom::IDBTransaction> transaction_remote;
    factory_remote->Open(client.CreateInterfacePtrAndBind(),
                         database_callbacks.CreateInterfacePtrAndBind(),
                         db_name, /*version=*/1,
                         transaction_remote.BindNewEndpointAndPassReceiver(),
                         /*transaction_id=*/1);
    run_loop.Run();

    // This step is necessary to make sure the backing store is closed so that
    // the second `Open` will initialize it with the new (older) data format
    // version. Without this step, the same `IndexedDBBackingStore` is reused
    // because it's kept around for 2 seconds after the last connection is
    // dropped.
    base::RunLoop run_loop2;
    context_->ForceClose(
        bucket_locator.id,
        storage::mojom::ForceCloseReason::FORCE_CLOSE_BACKING_STORE_FAILURE,
        run_loop2.QuitClosure());
    run_loop2.Run();
  }

  // Set an older data format version and try to reopen said database. Expect
  // total data loss.
  {
    base::AutoReset<IndexedDBDataFormatVersion> override_version(
        &IndexedDBDataFormatVersion::GetMutableCurrentForTesting(),
        IndexedDBDataFormatVersion(3, 3));
    base::RunLoop run_loop;
    MockMojoIndexedDBFactoryClient client;
    MockMojoIndexedDBDatabaseCallbacks database_callbacks;
    EXPECT_CALL(client, MockedUpgradeNeeded(
                            _, _, blink::mojom::IDBDataLoss::Total, _, _))
        .WillOnce(
            testing::DoAll(::base::test::RunClosure(run_loop.QuitClosure())));
    mojo::AssociatedRemote<blink::mojom::IDBTransaction> transaction_remote;
    factory_remote->Open(client.CreateInterfacePtrAndBind(),
                         database_callbacks.CreateInterfacePtrAndBind(),
                         db_name, /*version=*/1,
                         transaction_remote.BindNewEndpointAndPassReceiver(),
                         /*transaction_id=*/2);
    run_loop.Run();
  }
}

}  // namespace content
