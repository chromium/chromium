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
#include "components/services/storage/indexed_db/locks/partitioned_lock_manager.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_database.h"
#include "components/services/storage/privileged/mojom/indexed_db_control.mojom-test-utils.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "content/browser/indexed_db/indexed_db_bucket_context.h"
#include "content/browser/indexed_db/indexed_db_class_factory.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_data_format_version.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "content/browser/indexed_db/indexed_db_pre_close_task_queue.h"
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
using testing::_;
using url::Origin;

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
  LevelDBLock(leveldb::Env* env, std::unique_ptr<leveldb::FileLock> lock)
      : env_(env), lock_(std::move(lock)) {}

  LevelDBLock(const LevelDBLock&) = delete;
  LevelDBLock& operator=(const LevelDBLock&) = delete;

  ~LevelDBLock() {
    if (env_) {
      // The call to UnlockFile assumes ownership of the lock.
      env_->UnlockFile(lock_.release());
    }
  }

 private:
  raw_ptr<leveldb::Env> env_ = nullptr;
  std::unique_ptr<leveldb::FileLock> lock_;
};

std::unique_ptr<LevelDBLock> LockForTesting(const base::FilePath& file_name) {
  leveldb::Env* env = IndexedDBClassFactory::GetLevelDBOptions().env;
  base::FilePath lock_path = file_name.AppendASCII("LOCK");
  leveldb::FileLock* lock = nullptr;
  leveldb::Status status = env->LockFile(lock_path.AsUTF8Unsafe(), &lock);
  if (!status.ok())
    return nullptr;
  DCHECK(lock);
  return std::make_unique<LevelDBLock>(
      env, std::unique_ptr<leveldb::FileLock>(lock));
}

storage::BucketInfo ToBucketInfo(const storage::BucketLocator& bucket_locator) {
  storage::BucketInfo bucket_info;
  bucket_info.id = bucket_locator.id;
  bucket_info.storage_key = bucket_locator.storage_key;
  bucket_info.name = storage::kDefaultBucketName;
  return bucket_info;
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
        context_(std::make_unique<IndexedDBContextImpl>(
            temp_dir_.GetPath(),
            quota_manager_proxy_.get(),
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

  void SetUpInMemoryContext() {
    context_ = std::make_unique<IndexedDBContextImpl>(
        base::FilePath(), quota_manager_proxy_.get(),
        /*blob_storage_context=*/mojo::NullRemote(),
        /*file_system_access_context=*/mojo::NullRemote(),
        base::SequencedTaskRunner::GetCurrentDefault(),
        base::SequencedTaskRunner::GetCurrentDefault());
  }

  void RunPostedTasks() {
    base::RunLoop loop;
    context_->IDBTaskRunner()->PostTask(FROM_HERE, loop.QuitClosure());
    loop.Run();
  }

  void TearDown() override {
    if (context_ && !context_->IsInMemoryContext()) {
      for (auto bucket_locator : context_->GetAllBuckets()) {
        context_->DeleteBucketData(bucket_locator, base::DoNothing());
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

  bool DeleteForStorageKeySync(blink::StorageKey key) {
    base::test::TestFuture<bool> success;
    context()->DeleteForStorageKey(key, success.GetCallback());
    return success.Get();
  }

  void BindIndexedDBFactory(
      mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
          checker_remote,
      mojo::PendingReceiver<blink::mojom::IDBFactory> receiver,
      storage::QuotaErrorOr<storage::BucketInfo> bucket_info) {
    context()->BindIndexedDBImpl(std::move(checker_remote),
                                 base::UnguessableToken(), std::move(receiver),
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
    mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
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

  IndexedDBBucketContext& GetOrCreateBucketContext(
      const storage::BucketInfo& bucket,
      const base::FilePath& data_directory) {
    return context_->GetOrCreateBucketContext(bucket, data_directory);
  }

  storage::BucketInfo GetOrCreateBucket(
      const storage::BucketInitParams& params) {
    base::test::TestFuture<storage::QuotaErrorOr<storage::BucketInfo>> future;
    quota_manager_proxy_->UpdateOrCreateBucket(
        params, base::SingleThreadTaskRunner::GetCurrentDefault(),
        future.GetCallback());
    return future.Take().value();
  }

  IndexedDBBucketContextHandle CreateBucketHandle(
      absl::optional<storage::BucketLocator> bucket_locator = absl::nullopt) {
    if (!bucket_locator) {
      const blink::StorageKey storage_key =
          blink::StorageKey::CreateFromStringForTesting("http://localhost:81");
      bucket_locator = storage::BucketLocator();
      bucket_locator->storage_key = storage_key;
    }
    IndexedDBBucketContextHandle bucket_context_handle(
        context_->GetOrCreateBucketContext(
            ToBucketInfo(*bucket_locator),
            context()->GetDataPath(*bucket_locator)));
    bucket_context_handle->InitBackingStoreIfNeeded(/*create_if_missing=*/true);
    return bucket_context_handle;
  }

  void VerifyBucketContext(
      const storage::BucketId& id,
      bool expected_context_exists,
      std::optional<bool> expected_backing_store_exists = std::nullopt) {
    IndexedDBBucketContext* context = context_->GetBucketContextForTesting(id);
    if (!expected_context_exists) {
      EXPECT_FALSE(context);
      EXPECT_FALSE(expected_backing_store_exists.has_value());
    } else {
      ASSERT_TRUE(context);
      if (expected_backing_store_exists.has_value()) {
        EXPECT_EQ(*expected_backing_store_exists, !!context->backing_store());
      }
    }
  }

 protected:
  IndexedDBContextImpl* context() const { return context_.get(); }
  base::test::ScopedFeatureList scoped_feature_list_;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_dir_;
  scoped_refptr<storage::MockSpecialStoragePolicy> special_storage_policy_;
  scoped_refptr<storage::MockQuotaManager> quota_manager_;
  scoped_refptr<storage::MockQuotaManagerProxy> quota_manager_proxy_;
  std::unique_ptr<IndexedDBContextImpl> context_;
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

  IndexedDBContextImpl::Shutdown(std::move(context_));
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

  IndexedDBContextImpl::Shutdown(std::move(context_));
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
          [](IndexedDBContextImpl* context, storage::BucketInfo* bucket_info) {
            context->GetBucketContextForTesting(bucket_info->id)
                ->OnDatabaseError(
                    leveldb::Status::NotSupported("operation not supported"),
                    {});
          },
          context(), &bucket_info),
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

TEST_P(IndexedDBTest, BasicFactoryCreationAndTearDown) {
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

  GetOrCreateBucketContext(bucket_1, context()->GetDataPath(bucket_locator_1))
      .InitBackingStoreIfNeeded(true);

  GetOrCreateBucketContext(bucket_2, context()->GetDataPath(bucket_locator_2))
      .InitBackingStoreIfNeeded(true);

  GetOrCreateBucketContext(bucket_3, context()->GetDataPath(bucket_locator_3))
      .InitBackingStoreIfNeeded(true);

  GetOrCreateBucketContext(bucket_4, context()->GetDataPath(bucket_locator_4))
      .InitBackingStoreIfNeeded(true);

  GetOrCreateBucketContext(bucket_5, context()->GetDataPath(bucket_locator_5))
      .InitBackingStoreIfNeeded(true);

  int64_t bucket_size_1 = base::ComputeDirectorySize(file_1.DirName());
  int64_t bucket_size_4 = base::ComputeDirectorySize(file_4.DirName());
  int64_t bucket_size_5 = base::ComputeDirectorySize(file_5.DirName());

  if (IsThirdPartyStoragePartitioningEnabled()) {
    // If third party storage partitioning is on, additional space is taken
    // by supporting files for the independent buckets.
    EXPECT_NE(bucket_size_1, bucket_size_4);
  }
  EXPECT_NE(bucket_size_1, bucket_size_5);

  if (IsThirdPartyStoragePartitioningEnabled()) {
    EXPECT_EQ(5ul, context_->GetOpenBucketIdsForTesting().size());
  } else {
    EXPECT_EQ(4ul, context_->GetOpenBucketIdsForTesting().size());
  }
}

TEST_P(IndexedDBTest, CloseSequenceStarts) {
  IndexedDBBucketContextHandle bucket_context_handle = CreateBucketHandle();
  const storage::BucketId bucket_id =
      bucket_context_handle->bucket_locator().id;
  bucket_context_handle.Release();

  VerifyBucketContext(bucket_id, /*expected_context_exists=*/true,
                      /*expected_backing_store_exists=*/true);
  EXPECT_TRUE(context_->GetBucketContextForTesting(bucket_id)->IsClosing());

  context_->ForceClose(bucket_id, {}, base::DoNothing());
  RunPostedTasks();
  VerifyBucketContext(bucket_id, /*expected_context_exists=*/false);
}

TEST_P(IndexedDBTest, ImmediateClose) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      kIDBCloseImmediatelySwitch);
  IndexedDBBucketContextHandle bucket_context_handle = CreateBucketHandle();
  const storage::BucketId bucket_id =
      bucket_context_handle->bucket_locator().id;
  bucket_context_handle.Release();

  VerifyBucketContext(bucket_id, /*expected_context_exists=*/true);
  RunPostedTasks();
  VerifyBucketContext(bucket_id, /*expected_context_exists=*/false);
}

// Similar to the above, but installs a receiver which prevents the bucket
// context from being destroyed.
TEST_P(IndexedDBTest, CloseWithReceiversActive) {
  // Create bucket context.
  IndexedDBBucketContextHandle bucket_context_handle = CreateBucketHandle();
  const storage::BucketId bucket_id =
      bucket_context_handle->bucket_locator().id;
  // Connect an IDBFactory mojo client.
  mojo::Remote<blink::mojom::IDBFactory> factory_remote;
  mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
      checker_remote;
  bucket_context_handle->AddReceiver(
      std::move(checker_remote), /*client_token=*/{},
      factory_remote.BindNewPipeAndPassReceiver());

  // The bucket context and the backing store should exist.
  VerifyBucketContext(bucket_id, /*expected_context_exists=*/true,
                      /*expected_backing_store_exists=*/true);

  // The last handle to the bucket context is released and the grace period
  // elapses.
  bucket_context_handle.Release();
  task_environment_.FastForwardBy(base::Seconds(2));

  // This destroys the backing store, but the bucket context itself still
  // exists...
  VerifyBucketContext(bucket_id, /*expected_context_exists=*/true,
                      /*expected_backing_store_exists=*/false);

  // ...until the last mojo client is disconnected.
  factory_remote.reset();
  task_environment_.RunUntilIdle();

  VerifyBucketContext(bucket_id, /*expected_context_exists=*/false);
}

// Similar to the above, but reverses the order of receiver disconnection and
// handle destruction.
TEST_P(IndexedDBTest, CloseWithReceiversInactive) {
  // Create bucket context.
  IndexedDBBucketContextHandle bucket_context_handle = CreateBucketHandle();
  const storage::BucketId bucket_id =
      bucket_context_handle->bucket_locator().id;
  // Connect an IDBFactory mojo client.
  mojo::Remote<blink::mojom::IDBFactory> factory_remote;
  mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
      checker_remote;
  bucket_context_handle->AddReceiver(
      std::move(checker_remote), /*client_token=*/{},
      factory_remote.BindNewPipeAndPassReceiver());

  // The bucket context and the backing store should exist.
  VerifyBucketContext(bucket_id, /*expected_context_exists=*/true,
                      /*expected_backing_store_exists=*/true);

  // The last mojo client is disconnected.
  factory_remote.reset();
  task_environment_.RunUntilIdle();

  // The bucket context and the backing store should still exist.
  VerifyBucketContext(bucket_id, /*expected_context_exists=*/true,
                      /*expected_backing_store_exists=*/true);

  // The last handle to the bucket context is released and the grace period
  // elapses.
  bucket_context_handle.Release();
  task_environment_.FastForwardBy(base::Seconds(2));

  VerifyBucketContext(bucket_id, /*expected_context_exists=*/false);
}

TEST_P(IndexedDBTest, PreCloseTasksStart) {
  {
    // Open a connection & immediately release it to cause the closing sequence
    // to start.
    IndexedDBBucketContextHandle bucket_context_handle = CreateBucketHandle();
    storage::BucketId bucket_id = bucket_context_handle->bucket_locator().id;

    mojo::Remote<blink::mojom::IDBFactory> factory_remote;
    mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
        checker_remote;
    BindIndexedDBFactory(std::move(checker_remote),
                         factory_remote.BindNewPipeAndPassReceiver(),
                         ToBucketInfo(bucket_context_handle->bucket_locator()));

    bucket_context_handle.Release();

    VerifyBucketContext(bucket_id, /*expected_context_exists=*/true,
                        /*expected_backing_store_exists=*/true);
    EXPECT_TRUE(context_->GetBucketContextForTesting(bucket_id)->IsClosing());

    EXPECT_EQ(IndexedDBBucketContext::ClosingState::kPreCloseGracePeriod,
              context_->GetBucketContextForTesting(bucket_id)->closing_stage());

    task_environment_.FastForwardBy(base::Seconds(2));

    // The factory should be closed, as the pre close tasks are delayed.
    VerifyBucketContext(bucket_id, /*expected_context_exists=*/true,
                        /*expected_backing_store_exists=*/false);
  }

  // Move the clock to run the tasks in the next close sequence.
  // NOTE: The constants rate-limiting sweeps and compaction are currently the
  // same. This test may need to be restructured if these values diverge.
  task_environment_.FastForwardBy(
      IndexedDBBucketContext::kMaxEarliestGlobalSweepFromNow);

  {
    // Open a connection & immediately release it to cause the closing sequence
    // to start again.
    IndexedDBBucketContextHandle bucket_context_handle = CreateBucketHandle();
    storage::BucketId bucket_id = bucket_context_handle->bucket_locator().id;
    bucket_context_handle.Release();

    // Manually execute the timer so that the PreCloseTaskList task doesn't also
    // run.
    context_->GetBucketContextForTesting(bucket_id)->close_timer()->FireNow();

    // The pre-close tasks should be running now.
    ASSERT_TRUE(context_->GetBucketContextForTesting(bucket_id));
    EXPECT_EQ(IndexedDBBucketContext::ClosingState::kRunningPreCloseTasks,
              context_->GetBucketContextForTesting(bucket_id)->closing_stage());
    ASSERT_TRUE(context_->GetBucketContextForTesting(bucket_id)
                    ->pre_close_task_queue());
    EXPECT_TRUE(context_->GetBucketContextForTesting(bucket_id)
                    ->pre_close_task_queue()
                    ->started());
  }

  {
    // Stop sweep by opening a connection.
    IndexedDBBucketContextHandle bucket_context_handle = CreateBucketHandle();
    storage::BucketId bucket_id = bucket_context_handle->bucket_locator().id;
    EXPECT_FALSE(bucket_context_handle->pre_close_task_queue());

    // Move clock forward to trigger next sweep, but storage key has longer
    // sweep minimum, so no tasks should execute.
    task_environment_.FastForwardBy(
        IndexedDBBucketContext::kMaxEarliestGlobalSweepFromNow);

    bucket_context_handle.Release();
    ASSERT_TRUE(context_->GetBucketContextForTesting(bucket_id));
    EXPECT_EQ(IndexedDBBucketContext::ClosingState::kPreCloseGracePeriod,
              context_->GetBucketContextForTesting(bucket_id)->closing_stage());

    // Manually execute the timer so that the PreCloseTaskList task doesn't also
    // run.
    context_->GetBucketContextForTesting(bucket_id)->close_timer()->FireNow();
    VerifyBucketContext(bucket_id, /*expected_context_exists=*/true,
                        /*expected_backing_store_exists=*/true);

    RunPostedTasks();
    VerifyBucketContext(bucket_id, /*expected_context_exists=*/false);
  }

  {
    //  Finally, move the clock forward so the storage key should allow a sweep.
    task_environment_.FastForwardBy(
        IndexedDBBucketContext::kMaxEarliestBucketSweepFromNow);
    IndexedDBBucketContextHandle bucket_context_handle = CreateBucketHandle();
    storage::BucketId bucket_id = bucket_context_handle->bucket_locator().id;
    bucket_context_handle.Release();
    context_->GetBucketContextForTesting(bucket_id)->close_timer()->FireNow();

    ASSERT_TRUE(context_->GetBucketContextForTesting(bucket_id));
    EXPECT_EQ(IndexedDBBucketContext::ClosingState::kRunningPreCloseTasks,
              context_->GetBucketContextForTesting(bucket_id)->closing_stage());
    ASSERT_TRUE(context_->GetBucketContextForTesting(bucket_id)
                    ->pre_close_task_queue());
    EXPECT_TRUE(context_->GetBucketContextForTesting(bucket_id)
                    ->pre_close_task_queue()
                    ->started());
  }
}

TEST_P(IndexedDBTest, TombstoneSweeperTiming) {
  // Open a connection.
  IndexedDBBucketContextHandle bucket_context_handle = CreateBucketHandle();
  EXPECT_FALSE(bucket_context_handle->ShouldRunTombstoneSweeper());

  // Move the clock to run the tasks in the next close sequence.
  task_environment_.FastForwardBy(
      IndexedDBBucketContext::kMaxEarliestGlobalSweepFromNow);

  EXPECT_TRUE(bucket_context_handle->ShouldRunTombstoneSweeper());

  // Move clock forward to trigger next sweep, but storage key has longer
  // sweep minimum, so no tasks should execute.
  task_environment_.FastForwardBy(
      IndexedDBBucketContext::kMaxEarliestGlobalSweepFromNow);

  EXPECT_FALSE(bucket_context_handle->ShouldRunTombstoneSweeper());

  //  Finally, move the clock forward so the storage key should allow a sweep.
  task_environment_.FastForwardBy(
      IndexedDBBucketContext::kMaxEarliestBucketSweepFromNow);

  EXPECT_TRUE(bucket_context_handle->ShouldRunTombstoneSweeper());
}

TEST_P(IndexedDBTest, CompactionTaskTiming) {
  // Open a connection.
  IndexedDBBucketContextHandle bucket_context_handle = CreateBucketHandle();
  bucket_context_handle->InitBackingStoreIfNeeded(/*create_if_missing=*/true);
  EXPECT_FALSE(bucket_context_handle->ShouldRunCompaction());

  // Move the clock to run the tasks in the next close sequence.
  task_environment_.FastForwardBy(
      IndexedDBBucketContext::kMaxEarliestGlobalCompactionFromNow);

  EXPECT_TRUE(bucket_context_handle->ShouldRunCompaction());

  // Move clock forward to trigger next compaction, but storage key has longer
  // compaction minimum, so no tasks should execute.
  task_environment_.FastForwardBy(
      IndexedDBBucketContext::kMaxEarliestGlobalCompactionFromNow);

  EXPECT_FALSE(bucket_context_handle->ShouldRunCompaction());

  // Finally, move the clock forward so the storage key should allow a
  // compaction.
  task_environment_.FastForwardBy(
      IndexedDBBucketContext::kMaxEarliestBucketCompactionFromNow);

  EXPECT_TRUE(bucket_context_handle->ShouldRunCompaction());
}

TEST_P(IndexedDBTest, InMemoryFactoriesStay) {
  SetUpInMemoryContext();
  ASSERT_TRUE(context()->IsInMemoryContext());

  IndexedDBBucketContextHandle bucket_context_handle = CreateBucketHandle();
  storage::BucketLocator bucket_locator =
      bucket_context_handle->bucket_locator();

  EXPECT_TRUE(bucket_context_handle->backing_store()->in_memory());
  bucket_context_handle.Release();

  EXPECT_TRUE(context_->GetBucketContextForTesting(bucket_locator.id));
  EXPECT_FALSE(
      context_->GetBucketContextForTesting(bucket_locator.id)->IsClosing());

  context_->ForceClose(
      bucket_locator.id,
      storage::mojom::ForceCloseReason::FORCE_CLOSE_INTERNALS_PAGE,
      base::DoNothing());
  VerifyBucketContext(bucket_locator.id, /*expected_context_exists=*/true,
                      /*expected_backing_store_exists=*/true);

  context_->ForceClose(
      bucket_locator.id,
      storage::mojom::ForceCloseReason::FORCE_CLOSE_DELETE_ORIGIN,
      base::DoNothing());
  VerifyBucketContext(bucket_locator.id, /*expected_context_exists=*/false);
}

TEST_P(IndexedDBTest, TooLongOrigin) {
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

  IndexedDBBucketContextHandle bucket_context_handle(GetOrCreateBucketContext(
      ToBucketInfo(bucket_locator), context()->GetDataPath(bucket_locator)));
  leveldb::Status s;
  std::tie(s, std::ignore, std::ignore) =
      bucket_context_handle->InitBackingStoreIfNeeded(
          /*create_if_missing=*/true);

  EXPECT_TRUE(s.IsIOError());
}

TEST_P(IndexedDBTest, FactoryForceClose) {
  IndexedDBBucketContextHandle bucket_context_handle = CreateBucketHandle();
  storage::BucketLocator bucket_locator =
      bucket_context_handle->bucket_locator();

  bucket_context_handle->ForceClose(/*doom=*/false);
  bucket_context_handle.Release();

  VerifyBucketContext(bucket_locator.id, /*expected_context_exists=*/true,
                      /*expected_backing_store_exists=*/true);
  RunPostedTasks();
  VerifyBucketContext(bucket_locator.id, /*expected_context_exists=*/false);
}

// Tests that the backing store is closed when the connection is closed during
// upgrade.
TEST_P(IndexedDBTest, ConnectionCloseDuringUpgrade) {
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

  EXPECT_TRUE(context_->GetBucketContextForTesting(bucket_locator.id));
  EXPECT_FALSE(
      context_->GetBucketContextForTesting(bucket_locator.id)->IsClosing());

  // Drop the connection.
  pending_database.reset();
  factory_remote.FlushForTesting();
  EXPECT_TRUE(
      context_->GetBucketContextForTesting(bucket_locator.id)->IsClosing());
}

TEST_P(IndexedDBTest, DeleteDatabase) {
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

    // Backing store shouldn't exist.
    EXPECT_FALSE(context_->GetBucketContextForTesting(bucket_locator.id)
                     ->backing_store());
  }

  // Now create a database and thus the backing store.
  {
    MockMojoIndexedDBFactoryClient client;
    MockMojoIndexedDBDatabaseCallbacks database_callbacks;
    base::RunLoop run_loop;
    EXPECT_CALL(client, MockedOpenSuccess)
        .WillOnce(::base::test::RunClosure(run_loop.QuitClosure()));
    mojo::AssociatedRemote<blink::mojom::IDBTransaction> transaction_remote;
    factory_remote->Open(client.CreateInterfacePtrAndBind(),
                         database_callbacks.CreateInterfacePtrAndBind(), u"db",
                         /*version=*/0,
                         transaction_remote.BindNewEndpointAndPassReceiver(),
                         /*transaction_id=*/1);
    run_loop.Run();
  }

  // Delete the database now that the backing store actually exists.
  {
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
    ASSERT_TRUE(context_->GetBucketContextForTesting(bucket_locator.id));
    EXPECT_TRUE(
        context_->GetBucketContextForTesting(bucket_locator.id)->IsClosing());
  }
}

TEST_P(IndexedDBTest, GetDatabaseNames_NoFactory) {
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
    EXPECT_FALSE(context_->GetBucketContextForTesting(bucket_locator.id)
                     ->backing_store());
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

    EXPECT_TRUE(context_->GetBucketContextForTesting(bucket_locator.id));
    EXPECT_FALSE(
        context_->GetBucketContextForTesting(bucket_locator.id)->IsClosing());
  }
}

TEST_P(IndexedDBTest, QuotaErrorOnDiskFull) {
  leveldb_env::SetDBFactoryForTesting(base::BindRepeating(
      [](const leveldb_env::Options& options, const std::string& name,
         std::unique_ptr<leveldb::DB>* dbptr) {
        return leveldb_env::MakeIOError("foobar", "disk full",
                                        leveldb_env::MethodID::kCreateDir,
                                        base::File::FILE_ERROR_NO_SPACE);
      }));

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

  // A disk full error results in an error reported to the quota system.
  ASSERT_EQ(1U, quota_manager_->write_error_tracker().size());
  EXPECT_EQ(storage_key, quota_manager_->write_error_tracker().begin()->first);
  EXPECT_EQ(1, quota_manager_->write_error_tracker().begin()->second);

  leveldb_env::SetDBFactoryForTesting({});
}

TEST_P(IndexedDBTest, DatabaseFailedOpen) {
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
    IndexedDBBucketContext* bucket_context =
        context_->GetBucketContextForTesting(bucket_locator.id);
    ASSERT_TRUE(bucket_context);
    EXPECT_FALSE(
        base::Contains(bucket_context->GetDatabasesForTesting(), db_name));
  }
}

// Test for `IndexedDBDataFormatVersion`.
TEST_P(IndexedDBTest, DataLoss) {
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
