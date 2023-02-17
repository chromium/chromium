// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <utility>

#include "base/barrier_closure.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/default_clock.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_manager.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_database.h"
#include "components/services/storage/privileged/mojom/indexed_db_control.mojom-test-utils.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "content/browser/indexed_db/indexed_db_bucket_state.h"
#include "content/browser/indexed_db/indexed_db_client_state_checker_wrapper.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_factory.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "content/browser/indexed_db/indexed_db_leveldb_env.h"
#include "content/browser/indexed_db/mock_indexed_db_callbacks.h"
#include "content/browser/indexed_db/mock_indexed_db_database_callbacks.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/features.h"
#include "net/base/schemeful_site.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

using blink::IndexedDBDatabaseMetadata;

namespace content {
namespace {

base::FilePath CreateAndReturnTempDir(base::ScopedTempDir* temp_dir) {
  CHECK(temp_dir->CreateUniqueTempDir());
  return temp_dir->GetPath();
}

void CreateAndBindTransactionPlaceholder(
    base::WeakPtr<IndexedDBTransaction> transaction) {}

class LevelDBLock {
 public:
  LevelDBLock() = default;
  LevelDBLock(leveldb::Env* env, leveldb::FileLock* lock)
      : env_(env), lock_(lock) {}

  LevelDBLock(const LevelDBLock&) = delete;
  LevelDBLock& operator=(const LevelDBLock&) = delete;

  ~LevelDBLock() {
    if (env_)
      env_->UnlockFile(lock_);
  }

 private:
  raw_ptr<leveldb::Env> env_ = nullptr;
  raw_ptr<leveldb::FileLock> lock_ = nullptr;
};

std::unique_ptr<LevelDBLock> LockForTesting(const base::FilePath& file_name) {
  leveldb::Env* env = IndexedDBLevelDBEnv::Get();
  base::FilePath lock_path = file_name.AppendASCII("LOCK");
  leveldb::FileLock* lock = nullptr;
  leveldb::Status status = env->LockFile(lock_path.AsUTF8Unsafe(), &lock);
  if (!status.ok())
    return nullptr;
  DCHECK(lock);
  return std::make_unique<LevelDBLock>(env, lock);
}

}  // namespace

class IndexedDBTest : public testing::Test,
                      public testing::WithParamInterface<bool> {
 public:
  blink::StorageKey kNormalFirstPartyStorageKey;
  storage::BucketLocator kNormalFirstPartyBucketLocator;
  blink::StorageKey kSessionOnlyFirstPartyStorageKey;
  storage::BucketLocator kSessionOnlyFirstPartyBucketLocator;
  blink::StorageKey kNormalThirdPartyStorageKey;
  storage::BucketLocator kNormalThirdPartyBucketLocator;
  blink::StorageKey kSessionOnlyThirdPartyStorageKey;
  storage::BucketLocator kSessionOnlyThirdPartyBucketLocator;
  blink::StorageKey kInvertedNormalThirdPartyStorageKey;
  storage::BucketLocator kInvertedNormalThirdPartyBucketLocator;
  blink::StorageKey kInvertedSessionOnlyThirdPartyStorageKey;
  storage::BucketLocator kInvertedSessionOnlyThirdPartyBucketLocator;

  IndexedDBTest()
      : special_storage_policy_(
            base::MakeRefCounted<storage::MockSpecialStoragePolicy>()),
        quota_manager_(base::MakeRefCounted<storage::MockQuotaManager>(
            /*is_incognito=*/false,
            CreateAndReturnTempDir(&temp_dir_),
            base::SingleThreadTaskRunner::GetCurrentDefault(),
            special_storage_policy_)),
        quota_manager_proxy_(
            base::MakeRefCounted<storage::MockQuotaManagerProxy>(
                quota_manager_.get(),
                base::SequencedTaskRunner::GetCurrentDefault())),
        context_(base::MakeRefCounted<IndexedDBContextImpl>(
            temp_dir_.GetPath(),
            quota_manager_proxy_.get(),
            base::DefaultClock::GetInstance(),
            /*blob_storage_context=*/mojo::NullRemote(),
            /*file_system_access_context=*/mojo::NullRemote(),
            base::SequencedTaskRunner::GetCurrentDefault(),
            base::SequencedTaskRunner::GetCurrentDefault())) {
    scoped_feature_list_.InitWithFeatureState(
        net::features::kThirdPartyStoragePartitioning,
        IsThirdPartyStoragePartitioningEnabled());

    kNormalFirstPartyStorageKey =
        blink::StorageKey::CreateFromStringForTesting("http://normal/");
    storage::BucketInfo bucket_info = InitBucket(kNormalFirstPartyStorageKey);
    kNormalFirstPartyBucketLocator = bucket_info.ToBucketLocator();

    kSessionOnlyFirstPartyStorageKey =
        blink::StorageKey::CreateFromStringForTesting("http://session-only/");
    bucket_info = InitBucket(kSessionOnlyFirstPartyStorageKey);
    kSessionOnlyFirstPartyBucketLocator = bucket_info.ToBucketLocator();

    kNormalThirdPartyStorageKey =
        blink::StorageKey::Create(url::Origin::Create(GURL("http://normal/")),
                                  net::SchemefulSite(GURL("http://rando/")),
                                  blink::mojom::AncestorChainBit::kCrossSite);
    bucket_info = InitBucket(kNormalThirdPartyStorageKey);
    kNormalThirdPartyBucketLocator = bucket_info.ToBucketLocator();

    kSessionOnlyThirdPartyStorageKey = blink::StorageKey::Create(
        url::Origin::Create(GURL("http://session-only/")),
        net::SchemefulSite(GURL("http://rando/")),
        blink::mojom::AncestorChainBit::kCrossSite);
    bucket_info = InitBucket(kSessionOnlyThirdPartyStorageKey);
    kSessionOnlyThirdPartyBucketLocator = bucket_info.ToBucketLocator();

    kInvertedNormalThirdPartyStorageKey =
        blink::StorageKey::Create(url::Origin::Create(GURL("http://rando/")),
                                  net::SchemefulSite(GURL("http://normal/")),
                                  blink::mojom::AncestorChainBit::kCrossSite);
    bucket_info = InitBucket(kInvertedNormalThirdPartyStorageKey);
    kInvertedNormalThirdPartyBucketLocator = bucket_info.ToBucketLocator();

    kInvertedSessionOnlyThirdPartyStorageKey = blink::StorageKey::Create(
        url::Origin::Create(GURL("http://rando/")),
        net::SchemefulSite(GURL("http://session-only/")),
        blink::mojom::AncestorChainBit::kCrossSite);
    bucket_info = InitBucket(kInvertedSessionOnlyThirdPartyStorageKey);
    kInvertedSessionOnlyThirdPartyBucketLocator = bucket_info.ToBucketLocator();

    std::vector<storage::mojom::StoragePolicyUpdatePtr> policy_updates;
    policy_updates.emplace_back(storage::mojom::StoragePolicyUpdate::New(
        kSessionOnlyFirstPartyStorageKey.origin(),
        /*should_purge_on_shutdown=*/true));
    context_->ApplyPolicyUpdates(std::move(policy_updates));
  }

  IndexedDBTest(const IndexedDBTest&) = delete;
  IndexedDBTest& operator=(const IndexedDBTest&) = delete;

  ~IndexedDBTest() override = default;

  storage::BucketInfo InitBucket(const blink::StorageKey& storage_key) {
    storage::BucketInfo bucket;
    quota_manager_->UpdateOrCreateBucket(
        storage::BucketInitParams::ForDefaultBucket(storage_key),
        base::BindOnce(
            [](storage::BucketInfo* info,
               storage::QuotaErrorOr<storage::BucketInfo> bucket_info) {
              *info = bucket_info.value();
            },
            &bucket));
    return bucket;
  }

  void RunPostedTasks() {
    base::RunLoop loop;
    context_->IDBTaskRunner()->PostTask(FROM_HERE, loop.QuitClosure());
    loop.Run();
  }

  void TearDown() override {
    if (context_ && !context_->IsInMemoryContext()) {
      IndexedDBFactory* factory = context_->GetIDBFactory();

      // Loop through all open buckets, and force close them, and request
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

    if (temp_dir_.IsValid())
      ASSERT_TRUE(temp_dir_.Delete());
  }

  base::FilePath GetFilePathForTesting(
      const storage::BucketLocator& bucket_locator) {
    base::FilePath path;
    base::RunLoop run_loop;
    context()->GetFilePathForTesting(
        bucket_locator,
        base::BindLambdaForTesting([&](const base::FilePath& async_path) {
          path = async_path;
          run_loop.Quit();
        }));
    run_loop.Run();
    return path;
  }

  bool IsThirdPartyStoragePartitioningEnabled() { return GetParam(); }

  scoped_refptr<IndexedDBClientStateCheckerWrapper>
  CreateTestClientStateWrapper() {
    mojo::PendingAssociatedRemote<storage::mojom::IndexedDBClientStateChecker>
        remote;
    return base::MakeRefCounted<IndexedDBClientStateCheckerWrapper>(
        std::move(remote));
  }

 protected:
  IndexedDBContextImpl* context() const { return context_.get(); }
  base::test::ScopedFeatureList scoped_feature_list_;

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<storage::MockSpecialStoragePolicy> special_storage_policy_;
  scoped_refptr<storage::MockQuotaManager> quota_manager_;
  scoped_refptr<storage::MockQuotaManagerProxy> quota_manager_proxy_;
  scoped_refptr<IndexedDBContextImpl> context_;
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    IndexedDBTest,
    testing::Bool());

TEST_P(IndexedDBTest, ClearSessionOnlyDatabases) {
  base::FilePath normal_path_first_party;
  base::FilePath session_only_path_first_party;
  base::FilePath normal_path_third_party;
  base::FilePath session_only_path_third_party;
  base::FilePath inverted_normal_path_third_party;
  base::FilePath inverted_session_only_path_third_party;

  normal_path_first_party =
      GetFilePathForTesting(kNormalFirstPartyBucketLocator);
  session_only_path_first_party =
      GetFilePathForTesting(kSessionOnlyFirstPartyBucketLocator);
  normal_path_third_party =
      GetFilePathForTesting(kNormalThirdPartyBucketLocator);
  session_only_path_third_party =
      GetFilePathForTesting(kSessionOnlyThirdPartyBucketLocator);
  inverted_normal_path_third_party =
      GetFilePathForTesting(kInvertedNormalThirdPartyBucketLocator);
  inverted_session_only_path_third_party =
      GetFilePathForTesting(kInvertedSessionOnlyThirdPartyBucketLocator);
  if (IsThirdPartyStoragePartitioningEnabled()) {
    EXPECT_NE(normal_path_first_party, normal_path_third_party);
    EXPECT_NE(session_only_path_first_party, session_only_path_third_party);
    EXPECT_NE(inverted_normal_path_third_party,
              inverted_session_only_path_third_party);
  } else {
    EXPECT_EQ(normal_path_first_party, normal_path_third_party);
    EXPECT_EQ(session_only_path_first_party, session_only_path_third_party);
    EXPECT_EQ(inverted_normal_path_third_party,
              inverted_session_only_path_third_party);
  }

  ASSERT_TRUE(base::CreateDirectory(normal_path_first_party));
  ASSERT_TRUE(base::CreateDirectory(session_only_path_first_party));
  ASSERT_TRUE(base::CreateDirectory(normal_path_third_party));
  ASSERT_TRUE(base::CreateDirectory(session_only_path_third_party));
  ASSERT_TRUE(base::CreateDirectory(inverted_normal_path_third_party));
  ASSERT_TRUE(base::CreateDirectory(inverted_session_only_path_third_party));

  base::RunLoop run_loop;
  context()->ForceInitializeFromFilesForTesting(run_loop.QuitClosure());
  run_loop.Run();

  context()->Shutdown();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(base::DirectoryExists(normal_path_first_party));
  EXPECT_FALSE(base::DirectoryExists(session_only_path_first_party));
  EXPECT_TRUE(base::DirectoryExists(normal_path_third_party));
  EXPECT_FALSE(base::DirectoryExists(session_only_path_third_party));
  EXPECT_TRUE(base::DirectoryExists(inverted_normal_path_third_party));
  if (IsThirdPartyStoragePartitioningEnabled()) {
    EXPECT_FALSE(base::DirectoryExists(inverted_session_only_path_third_party));
  } else {
    EXPECT_TRUE(base::DirectoryExists(inverted_session_only_path_third_party));
  }
}

TEST_P(IndexedDBTest, SetForceKeepSessionState) {
  base::FilePath normal_path_first_party;
  base::FilePath session_only_path_first_party;
  base::FilePath normal_path_third_party;
  base::FilePath session_only_path_third_party;

  // Save session state. This should bypass the destruction-time deletion.
  context()->SetForceKeepSessionState();
  base::RunLoop().RunUntilIdle();

  normal_path_first_party =
      GetFilePathForTesting(kNormalFirstPartyBucketLocator);
  session_only_path_first_party =
      GetFilePathForTesting(kSessionOnlyFirstPartyBucketLocator);
  normal_path_third_party =
      GetFilePathForTesting(kNormalThirdPartyBucketLocator);
  session_only_path_third_party =
      GetFilePathForTesting(kSessionOnlyThirdPartyBucketLocator);
  if (IsThirdPartyStoragePartitioningEnabled()) {
    EXPECT_NE(normal_path_first_party, normal_path_third_party);
    EXPECT_NE(session_only_path_first_party, session_only_path_third_party);
  } else {
    EXPECT_EQ(normal_path_first_party, normal_path_third_party);
    EXPECT_EQ(session_only_path_first_party, session_only_path_third_party);
  }

  ASSERT_TRUE(base::CreateDirectory(normal_path_first_party));
  ASSERT_TRUE(base::CreateDirectory(session_only_path_first_party));
  ASSERT_TRUE(base::CreateDirectory(normal_path_third_party));
  ASSERT_TRUE(base::CreateDirectory(session_only_path_third_party));

  context()->ForceInitializeFromFilesForTesting(base::DoNothing());
  base::RunLoop().RunUntilIdle();

  context()->Shutdown();
  base::RunLoop().RunUntilIdle();

  // No data was cleared because of SetForceKeepSessionState.
  EXPECT_TRUE(base::DirectoryExists(normal_path_first_party));
  EXPECT_TRUE(base::DirectoryExists(session_only_path_first_party));
  EXPECT_TRUE(base::DirectoryExists(normal_path_third_party));
  EXPECT_TRUE(base::DirectoryExists(session_only_path_third_party));
}

class ForceCloseDBCallbacks : public IndexedDBCallbacks {
 public:
  ForceCloseDBCallbacks(scoped_refptr<IndexedDBContextImpl> idb_context,
                        const storage::BucketInfo& bucket_info)
      : IndexedDBCallbacks(nullptr,
                           bucket_info,
                           mojo::NullAssociatedRemote(),
                           idb_context->IDBTaskRunner()),
        idb_context_(idb_context),
        bucket_locator_(bucket_info.ToBucketLocator()) {}

  ForceCloseDBCallbacks(const ForceCloseDBCallbacks&) = delete;
  ForceCloseDBCallbacks& operator=(const ForceCloseDBCallbacks&) = delete;

  void OnSuccess() override {}
  void OnSuccess(std::unique_ptr<IndexedDBConnection> connection,
                 const IndexedDBDatabaseMetadata& metadata) override {
    connection_ = std::move(connection);
    idb_context_->ConnectionOpened(bucket_locator_);
  }

  IndexedDBConnection* connection() { return connection_.get(); }

 protected:
  ~ForceCloseDBCallbacks() override = default;

 private:
  scoped_refptr<IndexedDBContextImpl> idb_context_;
  storage::BucketLocator bucket_locator_;
  std::unique_ptr<IndexedDBConnection> connection_;
};

TEST_P(IndexedDBTest, ForceCloseOpenDatabasesOnDeleteFirstParty) {
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFromStringForTesting("http://test/");
  storage::BucketInfo bucket_info = InitBucket(kTestStorageKey);
  storage::BucketLocator bucket_locator = bucket_info.ToBucketLocator();

  auto open_db_callbacks =
      base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();
  auto closed_db_callbacks =
      base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();
  auto open_callbacks =
      base::MakeRefCounted<ForceCloseDBCallbacks>(context(), bucket_info);
  auto closed_callbacks =
      base::MakeRefCounted<ForceCloseDBCallbacks>(context(), bucket_info);
  base::FilePath test_path = GetFilePathForTesting(bucket_locator);

  const int64_t host_transaction_id = 0;
  const int64_t version = 0;

  IndexedDBFactory* factory = context()->GetIDBFactory();

  auto create_transaction_callback1 =
      base::BindOnce(&CreateAndBindTransactionPlaceholder);
  factory->Open(u"opendb",
                std::make_unique<IndexedDBPendingConnection>(
                    open_callbacks, open_db_callbacks, host_transaction_id,
                    version, std::move(create_transaction_callback1)),
                bucket_locator, context()->GetDataPath(bucket_locator),
                CreateTestClientStateWrapper());
  EXPECT_TRUE(base::DirectoryExists(test_path));

  auto create_transaction_callback2 =
      base::BindOnce(&CreateAndBindTransactionPlaceholder);
  factory->Open(u"closeddb",
                std::make_unique<IndexedDBPendingConnection>(
                    closed_callbacks, closed_db_callbacks, host_transaction_id,
                    version, std::move(create_transaction_callback2)),
                bucket_locator, context()->GetDataPath(bucket_locator),
                CreateTestClientStateWrapper());
  RunPostedTasks();
  ASSERT_TRUE(closed_callbacks->connection());
  closed_callbacks->connection()->AbortTransactionsAndClose(
      IndexedDBConnection::CloseErrorHandling::kAbortAllReturnLastError);
  RunPostedTasks();

  context()->ForceClose(
      bucket_locator.id,
      storage::mojom::ForceCloseReason::FORCE_CLOSE_DELETE_ORIGIN,
      base::DoNothing());
  EXPECT_TRUE(open_db_callbacks->forced_close_called());
  EXPECT_FALSE(closed_db_callbacks->forced_close_called());

  RunPostedTasks();

  bool success = false;
  storage::mojom::IndexedDBControlAsyncWaiter waiter(context());
  waiter.DeleteForStorageKey(kTestStorageKey, &success);
  EXPECT_TRUE(success);

  EXPECT_FALSE(base::DirectoryExists(test_path));
}

TEST_P(IndexedDBTest, ForceCloseOpenDatabasesOnDeleteThirdParty) {
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::Create(url::Origin::Create(GURL("http://test/")),
                                net::SchemefulSite(GURL("http://rando/")),
                                blink::mojom::AncestorChainBit::kCrossSite);
  storage::BucketInfo bucket_info = InitBucket(kTestStorageKey);
  storage::BucketLocator bucket_locator = bucket_info.ToBucketLocator();

  auto open_db_callbacks =
      base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();
  auto closed_db_callbacks =
      base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();
  auto open_callbacks =
      base::MakeRefCounted<ForceCloseDBCallbacks>(context(), bucket_info);
  auto closed_callbacks =
      base::MakeRefCounted<ForceCloseDBCallbacks>(context(), bucket_info);
  base::FilePath test_path = GetFilePathForTesting(bucket_locator);

  const int64_t host_transaction_id = 0;
  const int64_t version = 0;

  IndexedDBFactory* factory = context()->GetIDBFactory();

  auto create_transaction_callback1 =
      base::BindOnce(&CreateAndBindTransactionPlaceholder);
  factory->Open(u"opendb",
                std::make_unique<IndexedDBPendingConnection>(
                    open_callbacks, open_db_callbacks, host_transaction_id,
                    version, std::move(create_transaction_callback1)),
                bucket_locator, context()->GetDataPath(bucket_locator),
                CreateTestClientStateWrapper());
  EXPECT_TRUE(base::DirectoryExists(test_path));

  auto create_transaction_callback2 =
      base::BindOnce(&CreateAndBindTransactionPlaceholder);
  factory->Open(u"closeddb",
                std::make_unique<IndexedDBPendingConnection>(
                    closed_callbacks, closed_db_callbacks, host_transaction_id,
                    version, std::move(create_transaction_callback2)),
                bucket_locator, context()->GetDataPath(bucket_locator),
                CreateTestClientStateWrapper());
  RunPostedTasks();
  ASSERT_TRUE(closed_callbacks->connection());
  closed_callbacks->connection()->AbortTransactionsAndClose(
      IndexedDBConnection::CloseErrorHandling::kAbortAllReturnLastError);
  RunPostedTasks();

  context()->ForceClose(
      bucket_locator.id,
      storage::mojom::ForceCloseReason::FORCE_CLOSE_DELETE_ORIGIN,
      base::DoNothing());
  EXPECT_TRUE(open_db_callbacks->forced_close_called());
  EXPECT_FALSE(closed_db_callbacks->forced_close_called());

  RunPostedTasks();

  bool success = false;
  storage::mojom::IndexedDBControlAsyncWaiter waiter(context());
  waiter.DeleteForStorageKey(kTestStorageKey, &success);
  EXPECT_TRUE(success);

  EXPECT_FALSE(base::DirectoryExists(test_path));
}

TEST_P(IndexedDBTest, DeleteFailsIfDirectoryLockedFirstParty) {
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFromStringForTesting("http://test/");
  storage::BucketInfo bucket_info = InitBucket(kTestStorageKey);
  storage::BucketLocator bucket_locator = bucket_info.ToBucketLocator();

  base::FilePath test_path = GetFilePathForTesting(bucket_locator);
  ASSERT_TRUE(base::CreateDirectory(test_path));

  auto lock = LockForTesting(test_path);
  ASSERT_TRUE(lock);

  bool success = false;
  base::RunLoop loop;
  context()->IDBTaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        storage::mojom::IndexedDBControlAsyncWaiter waiter(context());
        waiter.DeleteForStorageKey(kTestStorageKey, &success);
        loop.Quit();
      }));
  loop.Run();
  EXPECT_FALSE(success);

  EXPECT_TRUE(base::DirectoryExists(test_path));
}

TEST_P(IndexedDBTest, DeleteFailsIfDirectoryLockedThirdParty) {
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::Create(url::Origin::Create(GURL("http://test/")),
                                net::SchemefulSite(GURL("http://rando/")),
                                blink::mojom::AncestorChainBit::kCrossSite);
  storage::BucketInfo bucket_info = InitBucket(kTestStorageKey);
  storage::BucketLocator bucket_locator = bucket_info.ToBucketLocator();

  base::FilePath test_path = GetFilePathForTesting(bucket_locator);
  ASSERT_TRUE(base::CreateDirectory(test_path));

  auto lock = LockForTesting(test_path);
  ASSERT_TRUE(lock);

  bool success = false;
  base::RunLoop loop;
  context()->IDBTaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        storage::mojom::IndexedDBControlAsyncWaiter waiter(context());
        waiter.DeleteForStorageKey(kTestStorageKey, &success);
        loop.Quit();
      }));
  loop.Run();
  EXPECT_FALSE(success);

  EXPECT_TRUE(base::DirectoryExists(test_path));
}

TEST_P(IndexedDBTest, ForceCloseOpenDatabasesOnCommitFailureFirstParty) {
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFromStringForTesting("http://test/");
  auto bucket_locator = storage::BucketLocator();
  bucket_locator.id = storage::BucketId::FromUnsafeValue(5);
  bucket_locator.storage_key = kTestStorageKey;

  auto* factory = static_cast<IndexedDBFactory*>(context()->GetIDBFactory());

  const int64_t transaction_id = 1;

  auto callbacks = base::MakeRefCounted<MockIndexedDBCallbacks>();
  auto db_callbacks = base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();
  auto create_transaction_callback1 =
      base::BindOnce(&CreateAndBindTransactionPlaceholder);
  auto connection = std::make_unique<IndexedDBPendingConnection>(
      callbacks, db_callbacks, transaction_id,
      IndexedDBDatabaseMetadata::DEFAULT_VERSION,
      std::move(create_transaction_callback1));
  factory->Open(u"db", std::move(connection), bucket_locator,
                context()->GetDataPath(bucket_locator),
                CreateTestClientStateWrapper());
  RunPostedTasks();

  ASSERT_TRUE(callbacks->connection());

  // ConnectionOpened() is usually called by the dispatcher.
  context()->ConnectionOpened(bucket_locator);

  EXPECT_TRUE(factory->IsBackingStoreOpen(bucket_locator));

  // Simulate the write failure.
  leveldb::Status status = leveldb::Status::IOError("Simulated failure");
  factory->HandleBackingStoreFailure(bucket_locator);

  EXPECT_TRUE(db_callbacks->forced_close_called());
  EXPECT_FALSE(factory->IsBackingStoreOpen(bucket_locator));
}

TEST_P(IndexedDBTest, ForceCloseOpenDatabasesOnCommitFailureThirdParty) {
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::Create(url::Origin::Create(GURL("http://test/")),
                                net::SchemefulSite(GURL("http://rando/")),
                                blink::mojom::AncestorChainBit::kCrossSite);
  auto bucket_locator = storage::BucketLocator();
  bucket_locator.id = storage::BucketId::FromUnsafeValue(5);
  bucket_locator.storage_key = kTestStorageKey;

  auto* factory = static_cast<IndexedDBFactory*>(context()->GetIDBFactory());

  const int64_t transaction_id = 1;

  auto callbacks = base::MakeRefCounted<MockIndexedDBCallbacks>();
  auto db_callbacks = base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();
  auto create_transaction_callback1 =
      base::BindOnce(&CreateAndBindTransactionPlaceholder);
  auto connection = std::make_unique<IndexedDBPendingConnection>(
      callbacks, db_callbacks, transaction_id,
      IndexedDBDatabaseMetadata::DEFAULT_VERSION,
      std::move(create_transaction_callback1));
  factory->Open(u"db", std::move(connection), bucket_locator,
                context()->GetDataPath(bucket_locator),
                CreateTestClientStateWrapper());
  RunPostedTasks();

  ASSERT_TRUE(callbacks->connection());

  // ConnectionOpened() is usually called by the dispatcher.
  context()->ConnectionOpened(bucket_locator);

  EXPECT_TRUE(factory->IsBackingStoreOpen(bucket_locator));

  // Simulate the write failure.
  leveldb::Status status = leveldb::Status::IOError("Simulated failure");
  factory->HandleBackingStoreFailure(bucket_locator);

  EXPECT_TRUE(db_callbacks->forced_close_called());
  EXPECT_FALSE(factory->IsBackingStoreOpen(bucket_locator));
}

TEST(PartitionedLockManager, TestRangeDifferences) {
  PartitionedLockId lock_id_db1;
  PartitionedLockId lock_id_db2;
  PartitionedLockId lock_id_db1_os1;
  PartitionedLockId lock_id_db1_os2;
  for (int64_t i = 0; i < 512; ++i) {
    lock_id_db1 = GetDatabaseLockId(i);
    lock_id_db2 = GetDatabaseLockId(i + 1);
    lock_id_db1_os1 = GetObjectStoreLockId(i, i);
    lock_id_db1_os2 = GetObjectStoreLockId(i, i + 1);
    EXPECT_NE(lock_id_db1, lock_id_db2);
    EXPECT_NE(lock_id_db1, lock_id_db1_os1);
    EXPECT_NE(lock_id_db1, lock_id_db1_os2);
    EXPECT_NE(lock_id_db1_os1, lock_id_db1_os2);
    EXPECT_NE(lock_id_db1_os1, lock_id_db2);
    EXPECT_NE(lock_id_db1_os2, lock_id_db2);
  }
}

}  // namespace content
