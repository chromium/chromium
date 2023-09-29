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
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/default_clock.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_manager.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_database.h"
#include "components/services/storage/privileged/mojom/indexed_db_control.mojom-test-utils.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "content/browser/indexed_db/indexed_db_bucket_context.h"
#include "content/browser/indexed_db/indexed_db_class_factory.h"
#include "content/browser/indexed_db/indexed_db_client_state_checker_wrapper.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_factory.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "content/browser/indexed_db/mock_indexed_db_database_callbacks.h"
#include "content/browser/indexed_db/mock_indexed_db_factory_client.h"
#include "content/browser/indexed_db/mock_mojo_indexed_db_database_callbacks.h"
#include "content/browser/indexed_db/mock_mojo_indexed_db_factory_client.h"
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
#include "third_party/leveldatabase/src/include/leveldb/env.h"

using blink::IndexedDBDatabaseMetadata;

namespace content {
namespace {

ACTION_TEMPLATE(MoveArgPointee,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(out)) {
  *out = std::move(*::testing::get<k>(args));
}

base::FilePath CreateAndReturnTempDir(base::ScopedTempDir* temp_dir) {
  CHECK(temp_dir->CreateUniqueTempDir());
  return temp_dir->GetPath();
}

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
  raw_ptr<leveldb::FileLock, DanglingUntriaged> lock_ = nullptr;
};

std::unique_ptr<LevelDBLock> LockForTesting(const base::FilePath& file_name) {
  leveldb::Env* env = IndexedDBClassFactory::GetLevelDBOptions().env;
  base::FilePath lock_path = file_name.AppendASCII("LOCK");
  leveldb::FileLock* lock = nullptr;
  leveldb::Status status = env->LockFile(lock_path.AsUTF8Unsafe(), &lock);
  if (!status.ok())
    return nullptr;
  DCHECK(lock);
  return std::make_unique<LevelDBLock>(env, lock);
}

}  // namespace

class IndexedDBTest
    : public testing::Test,
      // The first boolean toggles the Storage Partitioning feature. The second
      // boolean controls the type of StorageKey to run the test on (first or
      // third party).
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  blink::StorageKey kNormalFirstPartyStorageKey;
  storage::BucketLocator kNormalFirstPartyBucketLocator;
  blink::StorageKey kSessionOnlyFirstPartyStorageKey;
  storage::BucketLocator kSessionOnlyFirstPartyBucketLocator;
  blink::StorageKey kSessionOnlySubdomainFirstPartyStorageKey;
  storage::BucketLocator kSessionOnlySubdomainFirstPartyBucketLocator;
  blink::StorageKey kNormalThirdPartyStorageKey;
  storage::BucketLocator kNormalThirdPartyBucketLocator;
  blink::StorageKey kSessionOnlyThirdPartyStorageKey;
  storage::BucketLocator kSessionOnlyThirdPartyBucketLocator;
  blink::StorageKey kSessionOnlySubdomainThirdPartyStorageKey;
  storage::BucketLocator kSessionOnlySubdomainThirdPartyBucketLocator;
  blink::StorageKey kInvertedNormalThirdPartyStorageKey;
  storage::BucketLocator kInvertedNormalThirdPartyBucketLocator;
  blink::StorageKey kInvertedSessionOnlyThirdPartyStorageKey;
  storage::BucketLocator kInvertedSessionOnlyThirdPartyBucketLocator;
  blink::StorageKey kInvertedSessionOnlySubdomainThirdPartyStorageKey;
  storage::BucketLocator kInvertedSessionOnlySubdomainThirdPartyBucketLocator;

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
        blink::StorageKey::CreateFromStringForTesting("http://normal.com/");
    storage::BucketInfo bucket_info = InitBucket(kNormalFirstPartyStorageKey);
    kNormalFirstPartyBucketLocator = bucket_info.ToBucketLocator();

    kSessionOnlyFirstPartyStorageKey =
        blink::StorageKey::CreateFromStringForTesting(
            "http://session-only.com/");
    bucket_info = InitBucket(kSessionOnlyFirstPartyStorageKey);
    kSessionOnlyFirstPartyBucketLocator = bucket_info.ToBucketLocator();

    kSessionOnlySubdomainFirstPartyStorageKey =
        blink::StorageKey::CreateFromStringForTesting(
            "http://subdomain.session-only.com/");
    bucket_info = InitBucket(kSessionOnlySubdomainFirstPartyStorageKey);
    kSessionOnlySubdomainFirstPartyBucketLocator =
        bucket_info.ToBucketLocator();

    kNormalThirdPartyStorageKey = blink::StorageKey::Create(
        url::Origin::Create(GURL("http://normal.com/")),
        net::SchemefulSite(GURL("http://rando.com/")),
        blink::mojom::AncestorChainBit::kCrossSite);
    bucket_info = InitBucket(kNormalThirdPartyStorageKey);
    kNormalThirdPartyBucketLocator = bucket_info.ToBucketLocator();

    kSessionOnlyThirdPartyStorageKey = blink::StorageKey::Create(
        url::Origin::Create(GURL("http://session-only.com/")),
        net::SchemefulSite(GURL("http://rando.com/")),
        blink::mojom::AncestorChainBit::kCrossSite);
    bucket_info = InitBucket(kSessionOnlyThirdPartyStorageKey);
    kSessionOnlyThirdPartyBucketLocator = bucket_info.ToBucketLocator();

    kSessionOnlySubdomainThirdPartyStorageKey = blink::StorageKey::Create(
        url::Origin::Create(GURL("http://subdomain.session-only.com/")),
        net::SchemefulSite(GURL("http://rando.com/")),
        blink::mojom::AncestorChainBit::kCrossSite);
    bucket_info = InitBucket(kSessionOnlySubdomainThirdPartyStorageKey);
    kSessionOnlySubdomainThirdPartyBucketLocator =
        bucket_info.ToBucketLocator();

    kInvertedNormalThirdPartyStorageKey = blink::StorageKey::Create(
        url::Origin::Create(GURL("http://rando.com/")),
        net::SchemefulSite(GURL("http://normal.com/")),
        blink::mojom::AncestorChainBit::kCrossSite);
    bucket_info = InitBucket(kInvertedNormalThirdPartyStorageKey);
    kInvertedNormalThirdPartyBucketLocator = bucket_info.ToBucketLocator();

    kInvertedSessionOnlyThirdPartyStorageKey = blink::StorageKey::Create(
        url::Origin::Create(GURL("http://rando.com/")),
        net::SchemefulSite(GURL("http://session-only.com/")),
        blink::mojom::AncestorChainBit::kCrossSite);
    bucket_info = InitBucket(kInvertedSessionOnlyThirdPartyStorageKey);
    kInvertedSessionOnlyThirdPartyBucketLocator = bucket_info.ToBucketLocator();

    kInvertedSessionOnlySubdomainThirdPartyStorageKey =
        blink::StorageKey::Create(
            url::Origin::Create(GURL("http://rando.com/")),
            net::SchemefulSite(GURL("http://subdomain.session-only.com/")),
            blink::mojom::AncestorChainBit::kCrossSite);
    bucket_info = InitBucket(kInvertedSessionOnlySubdomainThirdPartyStorageKey);
    kInvertedSessionOnlySubdomainThirdPartyBucketLocator =
        bucket_info.ToBucketLocator();

    std::vector<storage::mojom::StoragePolicyUpdatePtr> policy_updates;
    policy_updates.emplace_back(storage::mojom::StoragePolicyUpdate::New(
        url::Origin::Create(GURL("http://subdomain.session-only.com")),
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
        EXPECT_TRUE(DeleteForStorageKeySync(bucket_locator.storage_key));
      }
    }

    // Wait for mojo pipes to flush or there may be leaks.
    task_environment_.RunUntilIdle();

    if (temp_dir_.IsValid())
      ASSERT_TRUE(temp_dir_.Delete());
  }

  base::FilePath GetFilePathForTesting(
      const storage::BucketLocator& bucket_locator) {
    base::test::TestFuture<const base::FilePath&> path_future;
    context()->GetFilePathForTesting(bucket_locator, path_future.GetCallback());
    return path_future.Take();
  }

  bool IsThirdPartyStoragePartitioningEnabled() {
    return std::get<0>(GetParam());
  }

  scoped_refptr<IndexedDBClientStateCheckerWrapper>
  CreateTestClientStateWrapper() {
    mojo::PendingAssociatedRemote<storage::mojom::IndexedDBClientStateChecker>
        remote;
    return base::MakeRefCounted<IndexedDBClientStateCheckerWrapper>(
        std::move(remote));
  }

  bool DeleteForStorageKeySync(blink::StorageKey key) {
    base::test::TestFuture<bool> success;
    context()->DeleteForStorageKey(key, success.GetCallback());
    return success.Get();
  }

  void BindIndexedDBFactory(
      mojo::PendingAssociatedRemote<storage::mojom::IndexedDBClientStateChecker>
          checker_remote,
      mojo::PendingReceiver<blink::mojom::IDBFactory> receiver,
      storage::QuotaErrorOr<storage::BucketInfo> bucket_info) {
    context()->BindIndexedDBImpl(std::move(checker_remote), std::move(receiver),
                                 bucket_info);
  }

  blink::StorageKey GetTestStorageKey() {
    const bool first_party = std::get<1>(GetParam());
    return first_party
               ? blink::StorageKey::CreateFromStringForTesting("http://test/")
               : blink::StorageKey::Create(
                     url::Origin::Create(GURL("http://test/")),
                     net::SchemefulSite(GURL("http://rando/")),
                     blink::mojom::AncestorChainBit::kCrossSite);
  }

  // Opens a database connection, runs `action`, and verifies that the
  // connection was forced closed.
  void VerifyForcedClosedCalled(base::OnceClosure action,
                                storage::BucketInfo* out_info = nullptr) {
    storage::BucketInfo bucket_info = InitBucket(GetTestStorageKey());
    if (out_info) {
      *out_info = bucket_info;
    }
    storage::BucketLocator bucket_locator = bucket_info.ToBucketLocator();
    base::FilePath test_path = GetFilePathForTesting(bucket_locator);

    // Bind the IDBFactory.
    mojo::PendingAssociatedRemote<storage::mojom::IndexedDBClientStateChecker>
        checker_remote;
    BindIndexedDBFactory(std::move(checker_remote),
                         factory_remote_.BindNewPipeAndPassReceiver(),
                         bucket_info);

    // Open new connection/database, wait for success.
    MockMojoIndexedDBFactoryClient client;
    MockMojoIndexedDBDatabaseCallbacks database_callbacks;
    base::RunLoop run_loop;
    // It's necessary to hang onto the database connection or the connection
    // will shut itself down and there will be no `ForcedClosed()`.
    mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_database;
    EXPECT_CALL(client, MockedOpenSuccess)
        .WillOnce(
            testing::DoAll(MoveArgPointee<0>(&pending_database),
                           ::base::test::RunClosure(run_loop.QuitClosure())));
    mojo::AssociatedRemote<blink::mojom::IDBTransaction> transaction_remote;
    factory_remote_->Open(client.CreateInterfacePtrAndBind(),
                          database_callbacks.CreateInterfacePtrAndBind(),
                          u"opendb", /*version=*/0,
                          transaction_remote.BindNewEndpointAndPassReceiver(),
                          /*host_transaction_id=*/0);
    run_loop.Run();
    EXPECT_TRUE(base::DirectoryExists(test_path));

    // Expect that deleting the data force closes the open database connection.
    base::RunLoop run_loop2;
    EXPECT_CALL(database_callbacks, ForcedClose())
        .WillOnce(::base::test::RunClosure(run_loop2.QuitClosure()));
    std::move(action).Run();
    run_loop2.Run();
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
  mojo::Remote<blink::mojom::IDBFactory> factory_remote_;
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    IndexedDBTest,
    // See class comment for meaning of params. Tests in this suite don't use
    // the second param.
    testing::Combine(testing::Bool(), testing::Values(true)));

TEST_P(IndexedDBTest, ClearSessionOnlyDatabases) {
  base::FilePath normal_path_first_party;
  base::FilePath session_only_path_first_party;
  base::FilePath session_only_subdomain_path_first_party;
  base::FilePath normal_path_third_party;
  base::FilePath session_only_path_third_party;
  base::FilePath session_only_subdomain_path_third_party;
  base::FilePath inverted_normal_path_third_party;
  base::FilePath inverted_session_only_path_third_party;
  base::FilePath inverted_session_only_subdomain_path_third_party;

  normal_path_first_party =
      GetFilePathForTesting(kNormalFirstPartyBucketLocator);
  session_only_path_first_party =
      GetFilePathForTesting(kSessionOnlyFirstPartyBucketLocator);
  session_only_subdomain_path_first_party =
      GetFilePathForTesting(kSessionOnlySubdomainFirstPartyBucketLocator);
  normal_path_third_party =
      GetFilePathForTesting(kNormalThirdPartyBucketLocator);
  session_only_path_third_party =
      GetFilePathForTesting(kSessionOnlyThirdPartyBucketLocator);
  session_only_subdomain_path_third_party =
      GetFilePathForTesting(kSessionOnlySubdomainThirdPartyBucketLocator);
  inverted_normal_path_third_party =
      GetFilePathForTesting(kInvertedNormalThirdPartyBucketLocator);
  inverted_session_only_path_third_party =
      GetFilePathForTesting(kInvertedSessionOnlyThirdPartyBucketLocator);
  inverted_session_only_subdomain_path_third_party = GetFilePathForTesting(
      kInvertedSessionOnlySubdomainThirdPartyBucketLocator);
  if (IsThirdPartyStoragePartitioningEnabled()) {
    EXPECT_NE(normal_path_first_party, normal_path_third_party);
    EXPECT_NE(session_only_path_first_party, session_only_path_third_party);
    EXPECT_NE(session_only_subdomain_path_first_party,
              session_only_subdomain_path_third_party);
    EXPECT_NE(inverted_normal_path_third_party,
              inverted_session_only_path_third_party);
    EXPECT_NE(inverted_normal_path_third_party,
              inverted_session_only_subdomain_path_third_party);
  } else {
    EXPECT_EQ(normal_path_first_party, normal_path_third_party);
    EXPECT_EQ(session_only_path_first_party, session_only_path_third_party);
    EXPECT_EQ(session_only_subdomain_path_first_party,
              session_only_subdomain_path_third_party);
    EXPECT_EQ(inverted_normal_path_third_party,
              inverted_session_only_path_third_party);
    EXPECT_EQ(inverted_normal_path_third_party,
              inverted_session_only_subdomain_path_third_party);
  }

  ASSERT_TRUE(base::CreateDirectory(normal_path_first_party));
  ASSERT_TRUE(base::CreateDirectory(session_only_path_first_party));
  ASSERT_TRUE(base::CreateDirectory(session_only_subdomain_path_first_party));
  ASSERT_TRUE(base::CreateDirectory(normal_path_third_party));
  ASSERT_TRUE(base::CreateDirectory(session_only_path_third_party));
  ASSERT_TRUE(base::CreateDirectory(session_only_subdomain_path_third_party));
  ASSERT_TRUE(base::CreateDirectory(inverted_normal_path_third_party));
  ASSERT_TRUE(base::CreateDirectory(inverted_session_only_path_third_party));
  ASSERT_TRUE(
      base::CreateDirectory(inverted_session_only_subdomain_path_third_party));

  base::RunLoop run_loop;
  context()->ForceInitializeFromFilesForTesting(run_loop.QuitClosure());
  run_loop.Run();

  context()->Shutdown();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(base::DirectoryExists(normal_path_first_party));
  EXPECT_TRUE(base::DirectoryExists(session_only_path_first_party));
  EXPECT_FALSE(base::DirectoryExists(session_only_subdomain_path_first_party));
  EXPECT_TRUE(base::DirectoryExists(normal_path_third_party));
  EXPECT_TRUE(base::DirectoryExists(session_only_path_third_party));
  EXPECT_FALSE(base::DirectoryExists(session_only_subdomain_path_third_party));
  EXPECT_TRUE(base::DirectoryExists(inverted_normal_path_third_party));
  // When storage partitioning is enabled these will be deleted because they
  // have a matching top-level site, but otherwise they won't be because the
  // deletion logic only considers the origin.
  if (IsThirdPartyStoragePartitioningEnabled()) {
    EXPECT_FALSE(base::DirectoryExists(inverted_session_only_path_third_party));
    EXPECT_FALSE(base::DirectoryExists(
        inverted_session_only_subdomain_path_third_party));
  } else {
    EXPECT_TRUE(base::DirectoryExists(inverted_session_only_path_third_party));
    EXPECT_TRUE(base::DirectoryExists(
        inverted_session_only_subdomain_path_third_party));
  }
}

TEST_P(IndexedDBTest, SetForceKeepSessionState) {
  base::FilePath normal_path_first_party;
  base::FilePath session_only_path_first_party;
  base::FilePath normal_path_third_party;
  base::FilePath session_only_path_third_party;

  // Save session state. This should bypass the destruction-time deletion.
  context()->SetForceKeepSessionState();

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

// Tests that parameterize whether they act on first or third party storage key
// buckets.
using IndexedDBTestFirstOrThirdParty = IndexedDBTest;

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    IndexedDBTestFirstOrThirdParty,
    // See base class comment for meaning of params. Tests in this suite operate
    // against both first and third party storage keys.
    testing::Combine(testing::Bool(), testing::Bool()));

// Verifies that the IDB connection is force closed and the directory is deleted
// when the bucket is deleted.
TEST_P(IndexedDBTestFirstOrThirdParty, ForceCloseOpenDatabasesOnDelete) {
  storage::BucketInfo bucket_info;
  VerifyForcedClosedCalled(
      base::BindOnce(
          base::IgnoreResult(&IndexedDBTest::DeleteForStorageKeySync),
          base::Unretained(this), GetTestStorageKey()),
      &bucket_info);
  // Additionally, the directory should be deleted.
  base::FilePath test_path =
      GetFilePathForTesting(bucket_info.ToBucketLocator());
  EXPECT_FALSE(base::DirectoryExists(test_path));
}

// Verifies that the IDB connection is force closed when the backing store has
// an error.
TEST_P(IndexedDBTestFirstOrThirdParty, ForceCloseOpenDatabasesOnCommitFailure) {
  storage::BucketInfo bucket_info;
  VerifyForcedClosedCalled(
      base::BindOnce(
          [](IndexedDBFactory* factory, storage::BucketInfo* bucket_info) {
            factory->HandleBackingStoreFailure(bucket_info->ToBucketLocator());
          },
          context()->GetIDBFactory(), &bucket_info),
      &bucket_info);
}

// Verifies that the IDB connection is force closed when the database is deleted
// via the mojo API.
TEST_P(IndexedDBTestFirstOrThirdParty,
       ForceCloseOpenDatabasesOnDeleteDatabase) {
  storage::BucketInfo bucket_info;
  VerifyForcedClosedCalled(
      base::BindOnce(
          [](mojo::Remote<blink::mojom::IDBFactory>* factory_remote) {
            MockMojoIndexedDBFactoryClient delete_client;
            (*factory_remote)
                ->DeleteDatabase(delete_client.CreateInterfacePtrAndBind(),
                                 u"opendb",
                                 /*force_close=*/true);
          },
          &this->factory_remote_),
      &bucket_info);
  base::FilePath test_path =
      GetFilePathForTesting(bucket_info.ToBucketLocator());
  EXPECT_TRUE(base::DirectoryExists(test_path));
}

// Verifies that the IDB connection is force closed when the context is
// destroyed.
TEST_P(IndexedDBTestFirstOrThirdParty,
       ForceCloseOpenDatabasesOnContextDestroyed) {
  storage::BucketInfo bucket_info;
  VerifyForcedClosedCalled(
      base::BindOnce(&IndexedDBFactory::ContextDestroyed,
                     base::Unretained(context()->GetIDBFactory())),
      &bucket_info);
  EXPECT_FALSE(context()->GetIDBFactory()->GetBucketContext(bucket_info.id));
}

TEST_P(IndexedDBTestFirstOrThirdParty, DeleteFailsIfDirectoryLocked) {
  const blink::StorageKey kTestStorageKey = GetTestStorageKey();
  storage::BucketInfo bucket_info = InitBucket(kTestStorageKey);
  storage::BucketLocator bucket_locator = bucket_info.ToBucketLocator();

  base::FilePath test_path = GetFilePathForTesting(bucket_locator);
  ASSERT_TRUE(base::CreateDirectory(test_path));

  auto lock = LockForTesting(test_path);
  ASSERT_TRUE(lock);

  base::test::TestFuture<bool> success_future;
  context()->IDBTaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        context()->DeleteForStorageKey(kTestStorageKey,
                                       success_future.GetCallback());
      }));
  EXPECT_FALSE(success_future.Get());

  EXPECT_TRUE(base::DirectoryExists(test_path));
}

TEST(PartitionedLockManager, TestRangeDifferences) {
  PartitionedLockId lock_id_db1;
  PartitionedLockId lock_id_db2;
  PartitionedLockId lock_id_db1_os1;
  PartitionedLockId lock_id_db1_os2;
  for (int64_t i = 0; i < 512; ++i) {
    lock_id_db1 = GetDatabaseLockId(
        base::ASCIIToUTF16(base::StringPrintf("%" PRIx64, i)));
    lock_id_db2 = GetDatabaseLockId(
        base::ASCIIToUTF16(base::StringPrintf("%" PRIx64, i + 1)));
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
