// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include <inttypes.h>

#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <set>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/clamped_math.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_id.h"
#include "components/services/storage/privileged/cpp/bucket_client_info.h"
#include "components/services/storage/privileged/mojom/indexed_db_control.mojom.h"
#include "components/services/storage/public/cpp/buckets/bucket_id.h"
#include "components/services/storage/public/cpp/buckets/bucket_info.h"
#include "components/services/storage/public/cpp/buckets/bucket_init_params.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/cpp/buckets/constants.h"
#include "components/services/storage/public/cpp/quota_error_or.h"
#include "components/services/storage/public/mojom/storage_policy_update.mojom.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_data_format_version.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "content/browser/indexed_db/instance/backing_store.h"
#include "content/browser/indexed_db/instance/backing_store_pre_close_task_queue.h"
#include "content/browser/indexed_db/instance/bucket_context.h"
#include "content/browser/indexed_db/instance/bucket_context_handle.h"
#include "content/browser/indexed_db/instance/connection.h"
#include "content/browser/indexed_db/mock_mojo_indexed_db_database_callbacks.h"
#include "content/browser/indexed_db/mock_mojo_indexed_db_factory_client.h"
#include "content/public/common/content_features.h"
#include "env_chromium.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "net/base/features.h"
#include "net/base/schemeful_site.h"
#include "storage/browser/test/mock_quota_manager.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key_path.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_metadata.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-shared.h"
#include "third_party/blink/public/mojom/storage_key/ancestor_chain_bit.mojom-shared.h"
#include "url/gurl.h"
#include "url/origin.h"

using base::test::RunClosure;
using blink::IndexedDBDatabaseMetadata;
using blink::IndexedDBIndexKeys;
using blink::IndexedDBKey;
using storage::BucketLocator;
using testing::_;
using testing::StrictMock;
using url::Origin;

namespace content::indexed_db {
namespace {

constexpr char16_t kDatabaseName[] = u"db";
constexpr char kOrigin[] = "https://www.example.com";

// TODO(crbug.com/41417435): Replace with common converter.
url::Origin ToOrigin(const std::string& url) {
  return url::Origin::Create(GURL(url));
}

MATCHER_P(IsAssociatedInterfacePtrInfoValid,
          tf,
          std::string(negation ? "isn't" : "is") + " " +
              std::string(tf ? "valid" : "invalid")) {
  return tf == arg->is_valid();
}

ACTION_P(QuitLoop, run_loop) {
  run_loop->Quit();
}

ACTION_TEMPLATE(MoveArgPointee,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(out)) {
  *out = std::move(*::testing::get<k>(args));
}

base::FilePath CreateAndReturnTempDir(base::ScopedTempDir* temp_dir) {
  CHECK(temp_dir->CreateUniqueTempDir());
  return temp_dir->GetPath();
}

storage::BucketInfo ToBucketInfo(const BucketLocator& bucket_locator) {
  storage::BucketInfo bucket_info;
  bucket_info.id = bucket_locator.id;
  bucket_info.storage_key = bucket_locator.storage_key;
  bucket_info.name = storage::kDefaultBucketName;
  return bucket_info;
}

// Stores data specific to a connection.
struct TestDatabaseConnection {
  TestDatabaseConnection() = default;

  TestDatabaseConnection(scoped_refptr<base::SequencedTaskRunner> task_runner,
                         url::Origin origin,
                         std::u16string db_name,
                         int64_t version,
                         int64_t upgrade_txn_id)
      : task_runner(std::move(task_runner)),
        origin(std::move(origin)),
        db_name(std::move(db_name)),
        version(version),
        upgrade_txn_id(upgrade_txn_id),
        open_callbacks(std::make_unique<StrictMock<MockMojoFactoryClient>>()),
        connection_callbacks(
            std::make_unique<StrictMock<MockMojoDatabaseCallbacks>>()) {}

  TestDatabaseConnection(const TestDatabaseConnection&) = delete;
  TestDatabaseConnection& operator=(const TestDatabaseConnection&) = delete;

  TestDatabaseConnection(TestDatabaseConnection&&) noexcept = default;
  TestDatabaseConnection& operator=(TestDatabaseConnection&&) noexcept =
      default;

  ~TestDatabaseConnection() = default;

  void Open(blink::mojom::IDBFactory* factory) {
    factory->Open(
        open_callbacks->CreateInterfacePtrAndBind(),
        connection_callbacks->CreateInterfacePtrAndBind(), db_name, version,
        version_change_transaction.BindNewEndpointAndPassReceiver(task_runner),
        upgrade_txn_id, /*priority=*/0);
    // ForcedClose is called on shutdown and depending on ordering and timing
    // may or may not happen, which is fine.
    EXPECT_CALL(*connection_callbacks, ForcedClose())
        .Times(testing::AnyNumber());
  }

  scoped_refptr<base::SequencedTaskRunner> task_runner;
  url::Origin origin;
  std::u16string db_name;
  int64_t version;
  int64_t upgrade_txn_id;

  mojo::AssociatedRemote<blink::mojom::IDBDatabase> database;
  mojo::AssociatedRemote<blink::mojom::IDBTransaction>
      version_change_transaction;

  std::unique_ptr<MockMojoFactoryClient> open_callbacks;
  std::unique_ptr<MockMojoDatabaseCallbacks> connection_callbacks;
};

class TestIndexedDBObserver : public storage::mojom::IndexedDBObserver {
 public:
  explicit TestIndexedDBObserver(
      mojo::PendingReceiver<storage::mojom::IndexedDBObserver> receiver)
      : receiver_(this, std::move(receiver)) {}

  void OnIndexedDBListChanged(const BucketLocator& bucket_locator) override {
    ++notify_list_changed_count;
  }

  void OnIndexedDBContentChanged(
      const BucketLocator& bucket_locator,
      const std::u16string& database_name,
      const std::u16string& object_store_name) override {
    ++notify_content_changed_count;
  }

  int notify_list_changed_count = 0;
  int notify_content_changed_count = 0;

 private:
  mojo::Receiver<storage::mojom::IndexedDBObserver> receiver_;
};

}  // namespace

class IndexedDBTest
    : public testing::Test,
      // The first boolean toggles the Storage Partitioning feature. The second
      // boolean controls the type of StorageKey to run the test on (first or
      // third party).
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  blink::StorageKey kNormalFirstPartyStorageKey;
  BucketLocator kNormalFirstPartyBucketLocator;
  blink::StorageKey kSessionOnlyFirstPartyStorageKey;
  BucketLocator kSessionOnlyFirstPartyBucketLocator;
  blink::StorageKey kSessionOnlySubdomainFirstPartyStorageKey;
  BucketLocator kSessionOnlySubdomainFirstPartyBucketLocator;
  blink::StorageKey kNormalThirdPartyStorageKey;
  BucketLocator kNormalThirdPartyBucketLocator;
  blink::StorageKey kSessionOnlyThirdPartyStorageKey;
  BucketLocator kSessionOnlyThirdPartyBucketLocator;
  blink::StorageKey kSessionOnlySubdomainThirdPartyStorageKey;
  BucketLocator kSessionOnlySubdomainThirdPartyBucketLocator;
  blink::StorageKey kInvertedNormalThirdPartyStorageKey;
  BucketLocator kInvertedNormalThirdPartyBucketLocator;
  blink::StorageKey kInvertedSessionOnlyThirdPartyStorageKey;
  BucketLocator kInvertedSessionOnlyThirdPartyBucketLocator;
  blink::StorageKey kInvertedSessionOnlySubdomainThirdPartyStorageKey;
  BucketLocator kInvertedSessionOnlySubdomainThirdPartyBucketLocator;

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
            base::SequencedTaskRunner::GetCurrentDefault())) {
    scoped_feature_list_.InitWithFeatureStates(
        {{net::features::kThirdPartyStoragePartitioning,
          IsThirdPartyStoragePartitioningEnabled()}});

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
        base::SequencedTaskRunner::GetCurrentDefault());
  }

  void RunPostedTasks() {
    base::RunLoop loop;
    context_->IDBTaskRunner()->PostTask(FROM_HERE, loop.QuitClosure());
    loop.Run();
  }

  void TearDown() override {
    factory_remote_.reset();

    if (context_ && !context_->in_memory()) {
      std::set<BucketLocator> buckets = context_->bucket_set_;
      for (const BucketLocator& bucket_locator : buckets) {
        context_->DeleteBucketData(bucket_locator, base::DoNothing());
      }

      while (!context_->GetOpenBucketIdsForTesting().empty()) {
        RunPostedTasks();
      }
    }

    if (temp_dir_.IsValid()) {
      ASSERT_TRUE(temp_dir_.Delete());
    }
  }

  base::FilePath GetFilePathForTesting(const BucketLocator& bucket_locator) {
    base::test::TestFuture<const base::FilePath&> path_future;
    context()->GetFilePathForTesting(bucket_locator, path_future.GetCallback());
    return path_future.Take();
  }

  bool IsThirdPartyStoragePartitioningEnabled() {
    return std::get<0>(GetParam());
  }

  bool DeleteBucket(const storage::BucketInfo* bucket_info) {
    base::test::TestFuture<blink::mojom::QuotaStatusCode> result_code;
    context()->DeleteBucketData(bucket_info->ToBucketLocator(),
                                result_code.GetCallback());
    return result_code.Get() == blink::mojom::QuotaStatusCode::kOk;
  }

  void BindFactory(
      mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
          checker_remote,
      mojo::PendingReceiver<blink::mojom::IDBFactory> receiver,
      storage::QuotaErrorOr<storage::BucketInfo> bucket_info) {
    context()->BindIndexedDBImpl(storage::BucketClientInfo{},
                                 std::move(checker_remote), std::move(receiver),
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
    BucketLocator bucket_locator = bucket_info.ToBucketLocator();
    base::FilePath test_path = GetFilePathForTesting(bucket_locator);

    // Bind the IDBFactory.
    mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
        checker_remote;
    BindFactory(std::move(checker_remote),
                factory_remote_.BindNewPipeAndPassReceiver(), bucket_info);

    // Open new connection/database, wait for success.
    MockMojoFactoryClient client;
    MockMojoDatabaseCallbacks database_callbacks;
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
                          /*host_transaction_id=*/0, /*priority=*/0);
    run_loop.Run();
    EXPECT_TRUE(base::DirectoryExists(test_path));

    // Expect that deleting the data force closes the open database connection.
    base::RunLoop run_loop2;
    EXPECT_CALL(database_callbacks, ForcedClose())
        .WillOnce(::base::test::RunClosure(run_loop2.QuitClosure()));
    std::move(action).Run();
    run_loop2.Run();
  }

  BucketContext& GetOrCreateBucketContext(
      const storage::BucketInfo& bucket,
      const base::FilePath& data_directory) {
    context_->EnsureBucketContext(bucket, data_directory);
    return *GetBucketContext(bucket.id);
  }

  BucketContext* GetBucketContext(storage::BucketId id) {
    auto* sequence_bound = context_->GetBucketContextForTesting(id);
    if (!sequence_bound) {
      return nullptr;
    }
    base::test::TestFuture<BucketContext*> future;
    sequence_bound->AsyncCall(&BucketContext::GetReferenceForTesting)
        .Then(future.GetCallback());
    return future.Get();
  }

  storage::BucketInfo GetOrCreateBucket(
      const storage::BucketInitParams& params) {
    base::test::TestFuture<storage::QuotaErrorOr<storage::BucketInfo>> future;
    quota_manager_proxy_->UpdateOrCreateBucket(
        params, base::SingleThreadTaskRunner::GetCurrentDefault(),
        future.GetCallback());
    return future.Take().value();
  }

  BucketContextHandle CreateBucketHandle(
      std::optional<storage::BucketLocator> bucket_locator = std::nullopt) {
    if (!bucket_locator) {
      const blink::StorageKey storage_key =
          blink::StorageKey::CreateFromStringForTesting("http://localhost:81");
      bucket_locator = BucketLocator();
      bucket_locator->storage_key = storage_key;
    }
    context_->EnsureBucketContext(ToBucketInfo(*bucket_locator),
                                  context()->GetDataPath(*bucket_locator));
    BucketContextHandle bucket_context_handle(
        *GetBucketContext(bucket_locator->id));
    bucket_context_handle->InitBackingStoreIfNeeded(
        /*create_if_missing=*/true);
    return bucket_context_handle;
  }

  void VerifyBucketContextWaitIfNeeded(const storage::BucketId& id,
                                       bool expected_context_exists) {
    while (expected_context_exists != context_->BucketContextExists(id)) {
      RunPostedTasks();
    }
    VerifyBucketContext(id, expected_context_exists);
  }

  void VerifyBucketContext(
      const storage::BucketId& id,
      bool expected_context_exists,
      std::optional<bool> expected_backing_store_exists = std::nullopt) {
    BucketContext* context = GetBucketContext(id);
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
    testing::Combine(
        /*enable third party storage partitioning*/ testing::Bool(),
        testing::Values(true)),
    [](const testing::TestParamInfo<IndexedDBTest::ParamType>& info) {
      std::string name = std::get<0>(info.param) ? "WithStoragePartitioning_"
                                                 : "NoStoragePartitioning_";
      return name;
    });

TEST_P(IndexedDBTest, CloseConnectionBeforeUpgrade) {
  const int64_t kDBVersion = 1;
  const int64_t kTransactionId = 1;
  std::unique_ptr<TestDatabaseConnection> connection;
  IndexedDBDatabaseMetadata metadata;
  mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_database;

  // Bind the IDBFactory.
  mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
      checker_remote;
  mojo::Remote<blink::mojom::IDBFactory> bounded_factory_remote;
  BindFactory(std::move(checker_remote),
              bounded_factory_remote.BindNewPipeAndPassReceiver(),
              storage::BucketInfo());

  base::RunLoop loop;
  connection = std::make_unique<TestDatabaseConnection>(
      context()->IDBTaskRunner(), url::Origin::Create(GURL(kOrigin)),
      kDatabaseName, kDBVersion, kTransactionId);
  EXPECT_CALL(
      *connection->open_callbacks,
      MockedUpgradeNeeded(IsAssociatedInterfacePtrInfoValid(true),
                          IndexedDBDatabaseMetadata::NO_VERSION,
                          blink::mojom::IDBDataLoss::None, std::string(""), _))
      .WillOnce(testing::DoAll(MoveArgPointee<0>(&pending_database),
                               testing::SaveArg<4>(&metadata),
                               QuitLoop(&loop)));

  connection->Open(bounded_factory_remote.get());

  loop.Run();

  EXPECT_TRUE(pending_database.is_valid());
  EXPECT_EQ(connection->version, metadata.version);
  EXPECT_EQ(connection->db_name, metadata.name);

  // Close the connection to finish the test nicely.
  connection.reset();
}

TEST_P(IndexedDBTest, CloseAfterUpgrade) {
  const int64_t kDBVersion = 1;
  const int64_t kTransactionId = 1;
  const int64_t kObjectStoreId = 10;
  const char16_t kObjectStoreName[] = u"os";
  std::unique_ptr<TestDatabaseConnection> connection;
  IndexedDBDatabaseMetadata metadata;
  mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_database;

  // Bind the IDBFactory.
  mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
      checker_remote;
  mojo::Remote<blink::mojom::IDBFactory> bounded_factory_remote;
  BindFactory(std::move(checker_remote),
              bounded_factory_remote.BindNewPipeAndPassReceiver(),
              storage::BucketInfo());

  base::RunLoop loop;
  // Open connection.
  connection = std::make_unique<TestDatabaseConnection>(
      context()->IDBTaskRunner(), ToOrigin(kOrigin), kDatabaseName, kDBVersion,
      kTransactionId);

  EXPECT_CALL(
      *connection->open_callbacks,
      MockedUpgradeNeeded(IsAssociatedInterfacePtrInfoValid(true),
                          IndexedDBDatabaseMetadata::NO_VERSION,
                          blink::mojom::IDBDataLoss::None, std::string(""), _))
      .WillOnce(testing::DoAll(MoveArgPointee<0>(&pending_database),
                               testing::SaveArg<4>(&metadata),
                               QuitLoop(&loop)));

  // Queue open request message.
  connection->Open(bounded_factory_remote.get());

  loop.Run();

  ASSERT_TRUE(pending_database.is_valid());
  EXPECT_EQ(connection->version, metadata.version);
  EXPECT_EQ(connection->db_name, metadata.name);

  base::RunLoop loop2;
  base::RepeatingClosure quit_closure2 =
      base::BarrierClosure(2, loop2.QuitClosure());

  ::testing::InSequence dummy;
  EXPECT_CALL(*connection->connection_callbacks, Complete(kTransactionId))
      .Times(1)
      .WillOnce(RunClosure(quit_closure2));
  EXPECT_CALL(*connection->open_callbacks,
              MockedOpenSuccess(IsAssociatedInterfacePtrInfoValid(false), _))
      .Times(1)
      .WillOnce(RunClosure(std::move(quit_closure2)));

  connection->database.Bind(std::move(pending_database));
  ASSERT_TRUE(connection->database.is_bound());
  ASSERT_TRUE(connection->version_change_transaction.is_bound());
  connection->version_change_transaction->CreateObjectStore(
      kObjectStoreId, kObjectStoreName, blink::IndexedDBKeyPath(), false);
  connection->version_change_transaction->Commit(0);

  loop2.Run();

  // Close the connections to finish the test nicely.
  connection.reset();
}

// TODO(crbug.com/40813013): Test is flaky on Mac in debug.
#if BUILDFLAG(IS_MAC) && !defined(NDEBUG)
#define MAYBE_OpenNewConnectionWhileUpgrading \
  DISABLED_OpenNewConnectionWhileUpgrading
#else
#define MAYBE_OpenNewConnectionWhileUpgrading OpenNewConnectionWhileUpgrading
#endif
TEST_P(IndexedDBTest, MAYBE_OpenNewConnectionWhileUpgrading) {
  const int64_t kDBVersion = 1;
  const int64_t kTransactionId = 1;
  const int64_t kObjectStoreId = 10;
  const char16_t kObjectStoreName[] = u"os";
  std::unique_ptr<TestDatabaseConnection> connection1;
  mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_database1;
  IndexedDBDatabaseMetadata metadata1;

  // Bind the IDBFactory.
  mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
      checker_remote;
  mojo::Remote<blink::mojom::IDBFactory> bounded_factory_remote;
  BindFactory(std::move(checker_remote),
              bounded_factory_remote.BindNewPipeAndPassReceiver(),
              storage::BucketInfo());

  base::RunLoop loop;
  // Open connection 1, and expect the upgrade needed.
  connection1 = std::make_unique<TestDatabaseConnection>(
      context()->IDBTaskRunner(), url::Origin::Create(GURL(kOrigin)),
      kDatabaseName, kDBVersion, kTransactionId);

  EXPECT_CALL(
      *connection1->open_callbacks,
      MockedUpgradeNeeded(IsAssociatedInterfacePtrInfoValid(true),
                          IndexedDBDatabaseMetadata::NO_VERSION,
                          blink::mojom::IDBDataLoss::None, std::string(""), _))
      .WillOnce(testing::DoAll(MoveArgPointee<0>(&pending_database1),
                               testing::SaveArg<4>(&metadata1),
                               QuitLoop(&loop)));

  // Queue open request message.
  connection1->Open(bounded_factory_remote.get());

  loop.Run();

  std::unique_ptr<TestDatabaseConnection> connection2;
  mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_database2;
  IndexedDBDatabaseMetadata metadata2;

  base::RunLoop loop2;
  base::RepeatingClosure quit_closure2 =
      base::BarrierClosure(3, loop2.QuitClosure());

  connection2 = std::make_unique<TestDatabaseConnection>(
      context()->IDBTaskRunner(), ToOrigin(kOrigin), kDatabaseName, kDBVersion,
      0);

  // Check that we're called in order and the second connection gets it's
  // database after the first connection completes.
  ::testing::InSequence dummy;
  EXPECT_CALL(*connection1->connection_callbacks, Complete(kTransactionId))
      .Times(1)
      .WillOnce(RunClosure(quit_closure2));
  EXPECT_CALL(*connection1->open_callbacks,
              MockedOpenSuccess(IsAssociatedInterfacePtrInfoValid(false), _))
      .Times(1)
      .WillOnce(RunClosure(quit_closure2));
  EXPECT_CALL(*connection2->open_callbacks,
              MockedOpenSuccess(IsAssociatedInterfacePtrInfoValid(true), _))
      .WillOnce(testing::DoAll(MoveArgPointee<0>(&pending_database2),
                               testing::SaveArg<1>(&metadata2),
                               RunClosure(std::move(quit_closure2))));

  connection1->database.Bind(std::move(pending_database1));
  ASSERT_TRUE(connection1->database.is_bound());
  ASSERT_TRUE(connection1->version_change_transaction.is_bound());

  // Open connection 2, but expect that we won't be called back.
  connection2->Open(bounded_factory_remote.get());

  // Create object store.
  connection1->version_change_transaction->CreateObjectStore(
      kObjectStoreId, kObjectStoreName, blink::IndexedDBKeyPath(), false);
  connection1->version_change_transaction->Commit(0);

  loop2.Run();

  EXPECT_TRUE(pending_database2.is_valid());
  EXPECT_EQ(connection2->version, metadata2.version);
  EXPECT_EQ(connection2->db_name, metadata2.name);

  // Close the connections to finish the test nicely.
  connection1.reset();
  connection2.reset();
}

MATCHER_P(IsCallbackError, error_code, "") {
  if (arg->is_error_result() &&
      arg->get_error_result()->error_code == error_code) {
    return true;
  }
  return false;
}

// See https://crbug.com/989723 for more context, this test seems to flake.
TEST_P(IndexedDBTest, DISABLED_PutWithInvalidBlob) {
  const int64_t kDBVersion = 1;
  const int64_t kTransactionId = 1;
  const int64_t kObjectStoreId = 10;
  const char16_t kObjectStoreName[] = u"os";
  std::unique_ptr<TestDatabaseConnection> connection;
  IndexedDBDatabaseMetadata metadata;
  mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_database;

  // Bind the IDBFactory.
  mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
      checker_remote;
  mojo::Remote<blink::mojom::IDBFactory> bounded_factory_remote;
  BindFactory(std::move(checker_remote),
              bounded_factory_remote.BindNewPipeAndPassReceiver(),
              storage::BucketInfo());

  base::RunLoop loop;
  // Open connection.
  connection = std::make_unique<TestDatabaseConnection>(
      context()->IDBTaskRunner(), url::Origin::Create(GURL(kOrigin)),
      kDatabaseName, kDBVersion, kTransactionId);

  EXPECT_CALL(
      *connection->open_callbacks,
      MockedUpgradeNeeded(IsAssociatedInterfacePtrInfoValid(true),
                          IndexedDBDatabaseMetadata::NO_VERSION,
                          blink::mojom::IDBDataLoss::None, std::string(""), _))
      .WillOnce(testing::DoAll(MoveArgPointee<0>(&pending_database),
                               testing::SaveArg<4>(&metadata),
                               QuitLoop(&loop)));

  // Queue open request message.
  connection->Open(bounded_factory_remote.get());
  loop.Run();

  ASSERT_TRUE(pending_database.is_valid());
  EXPECT_EQ(connection->version, metadata.version);
  EXPECT_EQ(connection->db_name, metadata.name);

  base::MockCallback<blink::mojom::IDBTransaction::PutCallback> put_callback;

  base::RunLoop loop2;
  base::RepeatingClosure quit_closure2 =
      base::BarrierClosure(3, loop2.QuitClosure());

  ::testing::InSequence dummy;

  EXPECT_CALL(put_callback,
              Run(IsCallbackError(blink::mojom::IDBException::kUnknownError)))
      .Times(1)
      .WillOnce(RunClosure(quit_closure2));

  EXPECT_CALL(
      *connection->connection_callbacks,
      Abort(kTransactionId, blink::mojom::IDBException::kUnknownError, _))
      .Times(1)
      .WillOnce(RunClosure(quit_closure2));

  EXPECT_CALL(*connection->open_callbacks,
              Error(blink::mojom::IDBException::kAbortError, _))
      .Times(1)
      .WillOnce(RunClosure(std::move(quit_closure2)));

  connection->database.Bind(std::move(pending_database));
  ASSERT_TRUE(connection->database.is_bound());
  ASSERT_TRUE(connection->version_change_transaction.is_bound());
  connection->version_change_transaction->CreateObjectStore(
      kObjectStoreId, kObjectStoreName, blink::IndexedDBKeyPath(), false);
  // Call Put with an invalid blob.
  std::vector<blink::mojom::IDBExternalObjectPtr> external_objects;
  mojo::PendingRemote<blink::mojom::Blob> blob;
  // Ignore the result of InitWithNewPipeAndPassReceiver, to end up with
  // an invalid blob.
  std::ignore = blob.InitWithNewPipeAndPassReceiver();
  external_objects.push_back(blink::mojom::IDBExternalObject::NewBlobOrFile(
      blink::mojom::IDBBlobInfo::New(std::move(blob), std::u16string(), 100,
                                     nullptr)));

  std::string value = "hello";
  const char* value_data = value.data();
  std::vector<uint8_t> value_vector(value_data, value_data + value.length());

  auto new_value = blink::mojom::IDBValue::New();
  new_value->bits = std::move(value_vector);
  new_value->external_objects = std::move(external_objects);

  connection->version_change_transaction->Put(
      kObjectStoreId, std::move(new_value), IndexedDBKey(u"hello"),
      blink::mojom::IDBPutMode::AddOnly, std::vector<IndexedDBIndexKeys>(),
      put_callback.Get());
  connection->version_change_transaction->Commit(0);

  loop2.Run();

  // Close the connection to finish the test nicely.
  connection.reset();
}

// Flaky: crbug.com/772067
TEST_P(IndexedDBTest, DISABLED_NotifyIndexedDBListChanged) {
  const int64_t kDBVersion1 = 1;
  const int64_t kDBVersion2 = 2;
  const int64_t kDBVersion3 = 3;
  const int64_t kTransactionId1 = 1;
  const int64_t kTransactionId2 = 2;
  const int64_t kTransactionId3 = 3;
  const int64_t kObjectStoreId = 10;
  const int64_t kIndexId = 100;
  const char16_t kObjectStoreName[] = u"os";
  const char16_t kIndexName[] = u"index";

  mojo::PendingReceiver<storage::mojom::IndexedDBObserver> receiver;
  mojo::PendingRemote<storage::mojom::IndexedDBObserver> remote;
  TestIndexedDBObserver observer(remote.InitWithNewPipeAndPassReceiver());
  context()->AddObserver(std::move(remote));

  // Bind the IDBFactory.
  mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
      checker_remote;
  mojo::Remote<blink::mojom::IDBFactory> bounded_factory_remote;
  BindFactory(std::move(checker_remote),
              bounded_factory_remote.BindNewPipeAndPassReceiver(),
              storage::BucketInfo());

  // Open connection 1.
  std::unique_ptr<TestDatabaseConnection> connection1;

  IndexedDBDatabaseMetadata metadata1;
  mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_database1;
  EXPECT_EQ(0, observer.notify_list_changed_count);
  {
    base::RunLoop loop;
    connection1 = std::make_unique<TestDatabaseConnection>(
        context()->IDBTaskRunner(), ToOrigin(kOrigin), kDatabaseName,
        kDBVersion1, kTransactionId1);

    EXPECT_CALL(
        *connection1->open_callbacks,
        MockedUpgradeNeeded(IsAssociatedInterfacePtrInfoValid(true),
                            IndexedDBDatabaseMetadata::NO_VERSION,
                            blink::mojom::IDBDataLoss::None, std::string(), _))
        .WillOnce(testing::DoAll(MoveArgPointee<0>(&pending_database1),
                                 testing::SaveArg<4>(&metadata1),
                                 QuitLoop(&loop)));

    // Queue open request message.
    connection1->Open(bounded_factory_remote.get());

    loop.Run();
  }
  EXPECT_TRUE(pending_database1.is_valid());
  EXPECT_EQ(connection1->version, metadata1.version);
  EXPECT_EQ(connection1->db_name, metadata1.name);

  {
    ::testing::InSequence dummy;
    base::RunLoop loop;
    base::RepeatingClosure quit_closure =
        base::BarrierClosure(2, loop.QuitClosure());

    // Create object store and index.
    connection1->database.Bind(std::move(pending_database1));
    ASSERT_TRUE(connection1->database.is_bound());
    ASSERT_TRUE(connection1->version_change_transaction.is_bound());

    EXPECT_CALL(*connection1->connection_callbacks, Complete(kTransactionId1))
        .Times(1)
        .WillOnce(RunClosure(quit_closure));
    EXPECT_CALL(*connection1->open_callbacks,
                MockedOpenSuccess(IsAssociatedInterfacePtrInfoValid(false), _))
        .Times(1)
        .WillOnce(RunClosure(std::move(quit_closure)));

    ASSERT_TRUE(connection1->database.is_bound());
    connection1->version_change_transaction->CreateObjectStore(
        kObjectStoreId, kObjectStoreName, blink::IndexedDBKeyPath(), false);
    connection1->database->CreateIndex(kTransactionId1, kObjectStoreId,
                                       kIndexId, kIndexName,
                                       blink::IndexedDBKeyPath(), false, false);
    connection1->version_change_transaction->Commit(0);

    loop.Run();
  }

  EXPECT_EQ(2, observer.notify_list_changed_count);

  // Open connection 2.
  std::unique_ptr<TestDatabaseConnection> connection2;

  IndexedDBDatabaseMetadata metadata2;
  mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_database2;
  {
    ::testing::InSequence dummy;
    base::RunLoop loop;
    base::RepeatingClosure quit_closure =
        base::BarrierClosure(2, loop.QuitClosure());

    connection2 = std::make_unique<TestDatabaseConnection>(
        context()->IDBTaskRunner(), url::Origin::Create(GURL(kOrigin)),
        kDatabaseName, kDBVersion2, kTransactionId2);

    EXPECT_CALL(*connection2->open_callbacks,
                MockedUpgradeNeeded(
                    IsAssociatedInterfacePtrInfoValid(true), kDBVersion1,
                    blink::mojom::IDBDataLoss::None, std::string(), _))
        .WillOnce(testing::DoAll(MoveArgPointee<0>(&pending_database2),
                                 testing::SaveArg<4>(&metadata2),
                                 QuitLoop(&loop)));

    // Queue open request message.
    connection2->Open(bounded_factory_remote.get());

    loop.Run();
  }
  EXPECT_TRUE(pending_database2.is_valid());
  EXPECT_EQ(connection2->version, metadata2.version);
  EXPECT_EQ(connection2->db_name, metadata2.name);

  {
    ::testing::InSequence dummy;
    base::RunLoop loop;
    base::RepeatingClosure quit_closure =
        base::BarrierClosure(2, loop.QuitClosure());

    // Delete index.
    connection2->database.Bind(std::move(pending_database2));
    ASSERT_TRUE(connection2->database.is_bound());
    ASSERT_TRUE(connection2->version_change_transaction.is_bound());

    EXPECT_CALL(*connection2->connection_callbacks, Complete(kTransactionId2))
        .Times(1)
        .WillOnce(RunClosure(quit_closure));
    EXPECT_CALL(*connection2->open_callbacks,
                MockedOpenSuccess(IsAssociatedInterfacePtrInfoValid(false), _))
        .Times(1)
        .WillOnce(RunClosure(std::move(quit_closure)));

    ASSERT_TRUE(connection2->database.is_bound());
    connection2->database->DeleteIndex(kTransactionId2, kObjectStoreId,
                                       kIndexId);
    connection2->version_change_transaction->Commit(0);

    loop.Run();
  }
  EXPECT_EQ(3, observer.notify_list_changed_count);

  // Open connection 3.
  std::unique_ptr<TestDatabaseConnection> connection3;

  IndexedDBDatabaseMetadata metadata3;
  mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_database3;
  {
    ::testing::InSequence dummy;
    base::RunLoop loop;
    connection3 = std::make_unique<TestDatabaseConnection>(
        context()->IDBTaskRunner(), ToOrigin(kOrigin), kDatabaseName,
        kDBVersion3, kTransactionId3);

    EXPECT_CALL(*connection3->open_callbacks,
                MockedUpgradeNeeded(
                    IsAssociatedInterfacePtrInfoValid(true), kDBVersion2,
                    blink::mojom::IDBDataLoss::None, std::string(), _))
        .WillOnce(testing::DoAll(MoveArgPointee<0>(&pending_database3),
                                 testing::SaveArg<4>(&metadata3),
                                 QuitLoop(&loop)));

    // Queue open request message.
    connection3->Open(bounded_factory_remote.get());

    loop.Run();
  }
  EXPECT_TRUE(pending_database3.is_valid());
  EXPECT_EQ(connection3->version, metadata3.version);
  EXPECT_EQ(connection3->db_name, metadata3.name);

  {
    ::testing::InSequence dummy;
    base::RunLoop loop;
    base::RepeatingClosure quit_closure =
        base::BarrierClosure(2, loop.QuitClosure());

    // Delete object store.
    connection3->database.Bind(std::move(pending_database3));
    ASSERT_TRUE(connection3->database.is_bound());
    ASSERT_TRUE(connection3->version_change_transaction.is_bound());

    EXPECT_CALL(*connection3->connection_callbacks, Complete(kTransactionId3))
        .Times(1)
        .WillOnce(RunClosure(quit_closure));
    EXPECT_CALL(*connection3->open_callbacks,
                MockedOpenSuccess(IsAssociatedInterfacePtrInfoValid(false), _))
        .Times(1)
        .WillOnce(RunClosure(std::move(quit_closure)));

    ASSERT_TRUE(connection3->database.is_bound());
    connection3->version_change_transaction->DeleteObjectStore(kObjectStoreId);
    connection3->version_change_transaction->Commit(0);

    loop.Run();
  }
  EXPECT_EQ(4, observer.notify_list_changed_count);

  // Close the connections to finish the test nicely.
  connection1.reset();
  connection2.reset();
  connection3.reset();
}

MATCHER(IsSuccessKey, "") {
  return arg->is_key();
}

// The test is flaky. See https://crbug.com/324111895
TEST_P(IndexedDBTest, DISABLED_NotifyIndexedDBContentChanged) {
  const int64_t kDBVersion1 = 1;
  const int64_t kDBVersion2 = 2;
  const int64_t kTransactionId1 = 1;
  const int64_t kTransactionId2 = 2;
  const int64_t kObjectStoreId = 10;
  const char16_t kObjectStoreName[] = u"os";

  mojo::PendingReceiver<storage::mojom::IndexedDBObserver> receiver;
  mojo::PendingRemote<storage::mojom::IndexedDBObserver> remote;
  TestIndexedDBObserver observer(remote.InitWithNewPipeAndPassReceiver());
  context()->AddObserver(std::move(remote));
  EXPECT_EQ(0, observer.notify_list_changed_count);
  EXPECT_EQ(0, observer.notify_content_changed_count);

  std::unique_ptr<TestDatabaseConnection> connection1;
  IndexedDBDatabaseMetadata metadata1;
  mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_database1;

  // Bind the IDBFactory.
  mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
      checker_remote;
  mojo::Remote<blink::mojom::IDBFactory> bounded_factory_remote;
  BindFactory(std::move(checker_remote),
              bounded_factory_remote.BindNewPipeAndPassReceiver(),
              storage::BucketInfo());

  base::RunLoop loop;
  // Open connection 1.
  connection1 = std::make_unique<TestDatabaseConnection>(
      context()->IDBTaskRunner(), url::Origin::Create(GURL(kOrigin)),
      kDatabaseName, kDBVersion1, kTransactionId1);

  EXPECT_CALL(
      *connection1->open_callbacks,
      MockedUpgradeNeeded(IsAssociatedInterfacePtrInfoValid(true),
                          IndexedDBDatabaseMetadata::NO_VERSION,
                          blink::mojom::IDBDataLoss::None, std::string(), _))
      .WillOnce(testing::DoAll(MoveArgPointee<0>(&pending_database1),
                               testing::SaveArg<4>(&metadata1),
                               QuitLoop(&loop)));

  // Queue open request message.
  connection1->Open(bounded_factory_remote.get());

  loop.Run();

  EXPECT_TRUE(pending_database1.is_valid());
  EXPECT_EQ(connection1->version, metadata1.version);
  EXPECT_EQ(connection1->db_name, metadata1.name);

  base::MockCallback<blink::mojom::IDBTransaction::PutCallback> put_callback;

  // Add object store entry.
  base::RunLoop loop2;
  base::RepeatingClosure quit_closure2 =
      base::BarrierClosure(3, loop2.QuitClosure());
  ::testing::InSequence dummy;

  EXPECT_CALL(put_callback, Run(IsSuccessKey()))
      .Times(1)
      .WillOnce(RunClosure(quit_closure2));
  EXPECT_CALL(*connection1->connection_callbacks, Complete(kTransactionId1))
      .Times(1)
      .WillOnce(RunClosure(quit_closure2));
  EXPECT_CALL(*connection1->open_callbacks,
              MockedOpenSuccess(IsAssociatedInterfacePtrInfoValid(false), _))
      .Times(1)
      .WillOnce(RunClosure(std::move(quit_closure2)));

  connection1->database.Bind(std::move(pending_database1));
  ASSERT_TRUE(connection1->database.is_bound());
  ASSERT_TRUE(connection1->version_change_transaction.is_bound());
  connection1->version_change_transaction->CreateObjectStore(
      kObjectStoreId, kObjectStoreName, blink::IndexedDBKeyPath(), false);

  std::string value = "value";
  const char* value_data = value.data();
  std::vector<uint8_t> value_vector(value_data, value_data + value.length());

  auto new_value = blink::mojom::IDBValue::New();
  new_value->bits = std::move(value_vector);

  connection1->version_change_transaction->Put(
      kObjectStoreId, std::move(new_value), IndexedDBKey(u"key"),
      blink::mojom::IDBPutMode::AddOnly, std::vector<IndexedDBIndexKeys>(),
      put_callback.Get());
  connection1->version_change_transaction->Commit(0);

  loop2.Run();

  EXPECT_EQ(2, observer.notify_list_changed_count);
  EXPECT_EQ(1, observer.notify_content_changed_count);

  connection1.reset();

  std::unique_ptr<TestDatabaseConnection> connection2;
  IndexedDBDatabaseMetadata metadata2;
  mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_database2;

  // Open connection 2.
  base::RunLoop loop4;
  connection2 = std::make_unique<TestDatabaseConnection>(
      context()->IDBTaskRunner(), ToOrigin(kOrigin), kDatabaseName, kDBVersion2,
      kTransactionId2);

  EXPECT_CALL(
      *connection2->open_callbacks,
      MockedUpgradeNeeded(IsAssociatedInterfacePtrInfoValid(true), kDBVersion1,
                          blink::mojom::IDBDataLoss::None, std::string(), _))
      .WillOnce(testing::DoAll(MoveArgPointee<0>(&pending_database2),
                               testing::SaveArg<4>(&metadata2),
                               QuitLoop(&loop4)));

  // Queue open request message.
  connection2->Open(bounded_factory_remote.get());

  loop4.Run();

  EXPECT_TRUE(pending_database2.is_valid());
  EXPECT_EQ(connection2->version, metadata2.version);
  EXPECT_EQ(connection2->db_name, metadata2.name);

  // Clear object store.
  base::RunLoop loop5;
  base::RepeatingClosure quit_closure5 =
      base::BarrierClosure(3, loop5.QuitClosure());

  EXPECT_CALL(*connection2->connection_callbacks, Complete(kTransactionId2))
      .Times(1)
      .WillOnce(RunClosure(quit_closure5));
  EXPECT_CALL(*connection2->open_callbacks,
              MockedOpenSuccess(IsAssociatedInterfacePtrInfoValid(false), _))
      .Times(1)
      .WillOnce(RunClosure(quit_closure5));

  connection2->database.Bind(std::move(pending_database2));
  ASSERT_TRUE(connection2->database.is_bound());
  ASSERT_TRUE(connection2->version_change_transaction.is_bound());
  connection2->database->Clear(kTransactionId2, kObjectStoreId,
                               base::IgnoreArgs<bool>(quit_closure5));
  connection2->version_change_transaction->Commit(0);

  loop5.Run();

  // +2 list changed, one for the transaction, the other for the ~DatabaseImpl
  EXPECT_EQ(4, observer.notify_list_changed_count);
  EXPECT_EQ(2, observer.notify_content_changed_count);

  // Close the connection to finish the test nicely.
  connection2.reset();
}

// The test is flaky. See https://crbug.com/324282438
TEST_P(IndexedDBTest, DISABLED_DatabaseOperationSequencing) {
  const int64_t kDBVersion = 1;
  const int64_t kTransactionId = 1;
  const std::u16string kObjectStoreName1 = u"os1";
  const std::u16string kObjectStoreName2 = u"os2";
  const std::u16string kObjectStoreName3 = u"os3";

  std::unique_ptr<TestDatabaseConnection> connection;
  IndexedDBDatabaseMetadata metadata;
  mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_database;

  // Bind the IDBFactory.
  mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
      checker_remote;
  mojo::Remote<blink::mojom::IDBFactory> bounded_factory_remote;
  BindFactory(std::move(checker_remote),
              bounded_factory_remote.BindNewPipeAndPassReceiver(),
              storage::BucketInfo());

  // Open the connection, which will initiate the "upgrade" transaction.
  base::RunLoop loop;
  // Open connection.
  connection = std::make_unique<TestDatabaseConnection>(
      context()->IDBTaskRunner(), ToOrigin(kOrigin), kDatabaseName, kDBVersion,
      kTransactionId);

  EXPECT_CALL(
      *connection->open_callbacks,
      MockedUpgradeNeeded(IsAssociatedInterfacePtrInfoValid(true),
                          IndexedDBDatabaseMetadata::NO_VERSION,
                          blink::mojom::IDBDataLoss::None, std::string(""), _))
      .WillOnce(testing::DoAll(MoveArgPointee<0>(&pending_database),
                               testing::SaveArg<4>(&metadata),
                               QuitLoop(&loop)));

  // Queue open request message.
  connection->Open(bounded_factory_remote.get());

  loop.Run();

  ASSERT_TRUE(pending_database.is_valid());
  EXPECT_EQ(connection->version, metadata.version);
  EXPECT_EQ(connection->db_name, metadata.name);

  EXPECT_EQ(0ULL, metadata.object_stores.size());

  // Within the "upgrade" transaction, create/delete/create object store. This
  // should leave only one store around if everything is processed in the
  // correct order.
  IndexedDBDatabaseMetadata metadata2;
  int64_t object_store_id = 1001;

  base::RunLoop loop2;
  base::RepeatingClosure quit_closure2 =
      base::BarrierClosure(2, loop2.QuitClosure());
  ::testing::InSequence dummy;

  EXPECT_CALL(*connection->connection_callbacks, Complete(kTransactionId))
      .Times(1)
      .WillOnce(RunClosure(quit_closure2));
  EXPECT_CALL(*connection->open_callbacks,
              MockedOpenSuccess(IsAssociatedInterfacePtrInfoValid(false), _))
      .Times(1)
      .WillOnce(testing::DoAll(testing::SaveArg<1>(&metadata2),
                               RunClosure(std::move(quit_closure2))));

  connection->database.Bind(std::move(pending_database));
  ASSERT_TRUE(connection->database.is_bound());
  ASSERT_TRUE(connection->version_change_transaction.is_bound());

  // This will cause a CreateObjectStoreOperation to be queued and
  // run synchronously...
  connection->version_change_transaction->CreateObjectStore(
      ++object_store_id, kObjectStoreName1, blink::IndexedDBKeyPath(),
      /*auto_increment=*/false);

  // The following operations will queue operations, but the
  // operations will run asynchronously.

  // First, delete the previous store. Ensure that this succeeds
  // even if the previous action completed synchronously.
  connection->version_change_transaction->DeleteObjectStore(object_store_id);

  // Ensure that a create/delete pair where both parts are queued
  // succeeds.
  connection->version_change_transaction->CreateObjectStore(
      ++object_store_id, kObjectStoreName2, blink::IndexedDBKeyPath(),
      /*auto_increment=*/false);
  connection->version_change_transaction->DeleteObjectStore(object_store_id);

  // This store is left over, just to verify that everything
  // ran correctly.
  connection->version_change_transaction->CreateObjectStore(
      ++object_store_id, kObjectStoreName3, blink::IndexedDBKeyPath(),
      /*auto_increment=*/false);

  connection->version_change_transaction->Commit(0);

  loop2.Run();

  EXPECT_EQ(1ULL, metadata2.object_stores.size());
  EXPECT_EQ(metadata2.object_stores[object_store_id].name, kObjectStoreName3);

  // Close the connection to finish the test nicely.
  connection.reset();
}

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
    testing::Combine(
        /*enable third party storage partitioning*/ testing::Bool(),
        /*test with third party storage key*/ testing::Bool()));

// Verifies that the IDB connection is force closed and the directory is deleted
// when the bucket is deleted.
TEST_P(IndexedDBTestFirstOrThirdParty, ForceCloseOpenDatabasesOnDelete) {
  storage::BucketInfo bucket_info;
  VerifyForcedClosedCalled(
      base::BindOnce(base::IgnoreResult(&IndexedDBTest::DeleteBucket),
                     base::Unretained(this), &bucket_info),
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
                ->AsyncCall(&BucketContext::OnDatabaseError)
                .WithArgs(Status::NotSupported("operation not supported"),
                          std::string());
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
            MockMojoFactoryClient delete_client;
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
  BucketLocator bucket_locator_1 = bucket_1.ToBucketLocator();
  auto file_1 =
      context_->GetLevelDBPath(bucket_locator_1).AppendASCII("1.json");
  ASSERT_TRUE(CreateDirectory(file_1.DirName()));
  ASSERT_TRUE(base::WriteFile(file_1, std::string(10, 'a')));

  const blink::StorageKey storage_key_2 =
      blink::StorageKey::CreateFromStringForTesting("http://localhost:82");
  storage::BucketInfo bucket_2 = GetOrCreateBucket(
      storage::BucketInitParams::ForDefaultBucket(storage_key_2));
  BucketLocator bucket_locator_2 = bucket_2.ToBucketLocator();
  auto file_2 =
      context_->GetLevelDBPath(bucket_locator_2).AppendASCII("2.json");
  ASSERT_TRUE(CreateDirectory(file_2.DirName()));
  ASSERT_TRUE(base::WriteFile(file_2, std::string(100, 'a')));

  const blink::StorageKey storage_key_3 =
      blink::StorageKey::CreateFromStringForTesting("http://localhost2:82");
  storage::BucketInfo bucket_3 = GetOrCreateBucket(
      storage::BucketInitParams::ForDefaultBucket(storage_key_3));
  BucketLocator bucket_locator_3 = bucket_3.ToBucketLocator();
  auto file_3 =
      context_->GetLevelDBPath(bucket_locator_3).AppendASCII("3.json");
  ASSERT_TRUE(CreateDirectory(file_3.DirName()));
  ASSERT_TRUE(base::WriteFile(file_3, std::string(1000, 'a')));

  const blink::StorageKey storage_key_4 = blink::StorageKey::Create(
      storage_key_1.origin(), net::SchemefulSite(storage_key_3.origin()),
      blink::mojom::AncestorChainBit::kCrossSite);
  storage::BucketInfo bucket_4 = GetOrCreateBucket(
      storage::BucketInitParams::ForDefaultBucket(storage_key_4));
  BucketLocator bucket_locator_4 = bucket_4.ToBucketLocator();
  auto file_4 =
      context_->GetLevelDBPath(bucket_locator_4).AppendASCII("4.json");
  ASSERT_TRUE(CreateDirectory(file_4.DirName()));
  ASSERT_TRUE(base::WriteFile(file_4, std::string(10000, 'a')));

  const blink::StorageKey storage_key_5 = storage_key_1;
  storage::BucketInitParams params(storage_key_5, "inbox");
  storage::BucketInfo bucket_5 = GetOrCreateBucket(params);
  BucketLocator bucket_locator_5 = bucket_5.ToBucketLocator();
  auto file_5 =
      context_->GetLevelDBPath(bucket_locator_5).AppendASCII("5.json");
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
  BucketContextHandle bucket_context_handle = CreateBucketHandle();
  const storage::BucketId bucket_id =
      bucket_context_handle->bucket_locator().id;
  bucket_context_handle.Release();

  VerifyBucketContext(bucket_id, /*expected_context_exists=*/true,
                      /*expected_backing_store_exists=*/true);
  EXPECT_TRUE(GetBucketContext(bucket_id)->IsClosing());

  context_->ForceClose(bucket_id, {}, base::DoNothing());
  VerifyBucketContextWaitIfNeeded(bucket_id, /*expected_context_exists=*/false);
}

// Similar to the above, but installs a receiver which prevents the bucket
// context from being destroyed.
TEST_P(IndexedDBTest, CloseWithReceiversActive) {
  // Create bucket context.
  BucketContextHandle bucket_context_handle = CreateBucketHandle();
  const storage::BucketId bucket_id =
      bucket_context_handle->bucket_locator().id;
  // Connect an IDBFactory mojo client.
  mojo::Remote<blink::mojom::IDBFactory> factory_remote;
  mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
      checker_remote;
  bucket_context_handle->AddReceiver(
      storage::BucketClientInfo{}, std::move(checker_remote),
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
  BucketContextHandle bucket_context_handle = CreateBucketHandle();
  const storage::BucketId bucket_id =
      bucket_context_handle->bucket_locator().id;
  // Connect an IDBFactory mojo client.
  mojo::Remote<blink::mojom::IDBFactory> factory_remote;
  mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
      checker_remote;
  bucket_context_handle->AddReceiver(
      storage::BucketClientInfo{}, std::move(checker_remote),
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
    BucketContextHandle bucket_context_handle = CreateBucketHandle();
    storage::BucketId bucket_id = bucket_context_handle->bucket_locator().id;

    mojo::Remote<blink::mojom::IDBFactory> factory_remote;
    mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
        checker_remote;
    BindFactory(std::move(checker_remote),
                factory_remote.BindNewPipeAndPassReceiver(),
                ToBucketInfo(bucket_context_handle->bucket_locator()));

    bucket_context_handle.Release();

    VerifyBucketContext(bucket_id, /*expected_context_exists=*/true,
                        /*expected_backing_store_exists=*/true);
    EXPECT_TRUE(GetBucketContext(bucket_id)->IsClosing());

    EXPECT_EQ(BucketContext::ClosingState::kPreCloseGracePeriod,
              GetBucketContext(bucket_id)->closing_stage());

    task_environment_.FastForwardBy(base::Seconds(2));

    // The factory should be closed, as the pre close tasks are delayed.
    VerifyBucketContext(bucket_id, /*expected_context_exists=*/true,
                        /*expected_backing_store_exists=*/false);
  }

  // Move the clock to run the tasks in the next close sequence.
  // NOTE: The constants rate-limiting sweeps and compaction are currently the
  // same. This test may need to be restructured if these values diverge.
  task_environment_.FastForwardBy(
      BucketContext::kMaxEarliestGlobalSweepFromNow);

  // Note that once the closing sequence has started, as is the case in the next
  // block, and if the test does anything to spin the message loop, such as
  // using a future to get the bucket context, the bucket context will be
  // destroyed. For that reason, the bucket context pointer is stored in
  // `bucket_context`.
  BucketContext* bucket_context = nullptr;

  {
    // Open a connection & immediately release it to cause the closing sequence
    // to start again.
    BucketContextHandle bucket_context_handle = CreateBucketHandle();
    bucket_context = bucket_context_handle.bucket_context();
    bucket_context_handle.Release();

    // Manually execute the timer so that the PreCloseTaskList task doesn't also
    // run.
    bucket_context->close_timer()->FireNow();

    // The pre-close tasks should be running now.
    EXPECT_EQ(BucketContext::ClosingState::kRunningPreCloseTasks,
              bucket_context->closing_stage());
    ASSERT_TRUE(bucket_context->pre_close_task_queue());
    EXPECT_TRUE(bucket_context->pre_close_task_queue()->started());
  }

  {
    // Stop sweep by opening a connection.
    BucketContextHandle bucket_context_handle(*bucket_context);
    storage::BucketId bucket_id = bucket_context_handle->bucket_locator().id;
    EXPECT_FALSE(bucket_context_handle->pre_close_task_queue());

    // Move clock forward to trigger next sweep, but storage key has longer
    // sweep minimum, so no tasks should execute.
    task_environment_.FastForwardBy(
        BucketContext::kMaxEarliestGlobalSweepFromNow);

    bucket_context_handle.Release();
    EXPECT_EQ(BucketContext::ClosingState::kPreCloseGracePeriod,
              bucket_context->closing_stage());

    // Manually execute the timer so that the PreCloseTaskList task doesn't also
    // run.
    bucket_context->close_timer()->FireNow();
    ASSERT_TRUE(context_->BucketContextExists(bucket_id));
    EXPECT_TRUE(!!bucket_context->backing_store());

    VerifyBucketContextWaitIfNeeded(bucket_id,
                                    /*expected_context_exists=*/false);
  }

  {
    //  Finally, move the clock forward so the storage key should allow a sweep.
    task_environment_.FastForwardBy(
        BucketContext::kMaxEarliestBucketSweepFromNow);
    BucketContextHandle bucket_context_handle = CreateBucketHandle();
    bucket_context = bucket_context_handle.bucket_context();
    storage::BucketId bucket_id = bucket_context_handle->bucket_locator().id;
    bucket_context_handle.Release();
    bucket_context->close_timer()->FireNow();

    ASSERT_TRUE(context_->BucketContextExists(bucket_id));
    EXPECT_EQ(BucketContext::ClosingState::kRunningPreCloseTasks,
              bucket_context->closing_stage());
    ASSERT_TRUE(bucket_context->pre_close_task_queue());
    EXPECT_TRUE(bucket_context->pre_close_task_queue()->started());
  }
}

TEST_P(IndexedDBTest, TombstoneSweeperTiming) {
  // Open a connection.
  BucketContextHandle bucket_context_handle = CreateBucketHandle();
  EXPECT_FALSE(bucket_context_handle->ShouldRunTombstoneSweeper());

  // Move the clock to run the tasks in the next close sequence.
  task_environment_.FastForwardBy(
      BucketContext::kMaxEarliestGlobalSweepFromNow);

  EXPECT_TRUE(bucket_context_handle->ShouldRunTombstoneSweeper());

  // Move clock forward to trigger next sweep, but storage key has longer
  // sweep minimum, so no tasks should execute.
  task_environment_.FastForwardBy(
      BucketContext::kMaxEarliestGlobalSweepFromNow);

  EXPECT_FALSE(bucket_context_handle->ShouldRunTombstoneSweeper());

  //  Finally, move the clock forward so the storage key should allow a sweep.
  task_environment_.FastForwardBy(
      BucketContext::kMaxEarliestBucketSweepFromNow);

  EXPECT_TRUE(bucket_context_handle->ShouldRunTombstoneSweeper());
}

TEST_P(IndexedDBTest, CompactionTaskTiming) {
  // Open a connection.
  BucketContextHandle bucket_context_handle = CreateBucketHandle();
  bucket_context_handle->InitBackingStoreIfNeeded(/*create_if_missing=*/true);
  EXPECT_FALSE(bucket_context_handle->ShouldRunCompaction());

  // Move the clock to run the tasks in the next close sequence.
  task_environment_.FastForwardBy(
      BucketContext::kMaxEarliestGlobalCompactionFromNow);

  EXPECT_TRUE(bucket_context_handle->ShouldRunCompaction());

  // Move clock forward to trigger next compaction, but storage key has longer
  // compaction minimum, so no tasks should execute.
  task_environment_.FastForwardBy(
      BucketContext::kMaxEarliestGlobalCompactionFromNow);

  EXPECT_FALSE(bucket_context_handle->ShouldRunCompaction());

  // Finally, move the clock forward so the storage key should allow a
  // compaction.
  task_environment_.FastForwardBy(
      BucketContext::kMaxEarliestBucketCompactionFromNow);

  EXPECT_TRUE(bucket_context_handle->ShouldRunCompaction());
}

TEST_P(IndexedDBTest, InMemoryFactoriesStay) {
  SetUpInMemoryContext();

  BucketContextHandle bucket_context_handle = CreateBucketHandle();
  BucketLocator bucket_locator = bucket_context_handle->bucket_locator();

  EXPECT_TRUE(bucket_context_handle->backing_store()->in_memory());
  BucketContext* bucket_context = bucket_context_handle.bucket_context();
  bucket_context_handle.Release();
  RunPostedTasks();

  EXPECT_TRUE(context_->BucketContextExists(bucket_locator.id));
  EXPECT_FALSE(bucket_context->IsClosing());

  context_->ForceClose(
      bucket_locator.id,
      storage::mojom::ForceCloseReason::FORCE_CLOSE_INTERNALS_PAGE,
      base::DoNothing());
  // Verify the in-memory factory sticks around. Since it would be destroyed
  // asynchronously, there's no reliable point in time to verify that
  // destruction *hasn't* happened, so just wait a bit before verifying.
  RunPostedTasks();
  RunPostedTasks();
  RunPostedTasks();

  VerifyBucketContext(bucket_locator.id, /*expected_context_exists=*/true,
                      /*expected_backing_store_exists=*/true);

  context_->ForceClose(
      bucket_locator.id,
      storage::mojom::ForceCloseReason::FORCE_CLOSE_DELETE_ORIGIN,
      base::DoNothing());
  VerifyBucketContextWaitIfNeeded(bucket_locator.id,
                                  /*expected_context_exists=*/false);
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
  BucketLocator bucket_locator = bucket_info.ToBucketLocator();

  BucketContextHandle bucket_context_handle(GetOrCreateBucketContext(
      ToBucketInfo(bucket_locator), context()->GetDataPath(bucket_locator)));
  Status s;
  std::tie(s, std::ignore, std::ignore) =
      bucket_context_handle->InitBackingStoreIfNeeded(
          /*create_if_missing=*/true);

  EXPECT_TRUE(s.IsIOError());
}

TEST_P(IndexedDBTest, FactoryForceClose) {
  BucketContextHandle bucket_context_handle = CreateBucketHandle();
  BucketLocator bucket_locator = bucket_context_handle->bucket_locator();

  bucket_context_handle->ForceClose(/*doom=*/false);
  BucketContext* bucket_context = bucket_context_handle.bucket_context();
  bucket_context_handle.Release();

  ASSERT_TRUE(context_->BucketContextExists(bucket_locator.id));
  EXPECT_TRUE(!!bucket_context->backing_store());
  VerifyBucketContextWaitIfNeeded(bucket_locator.id,
                                  /*expected_context_exists=*/false);
}

// This test aims to verify the behavior of
// BucketContext::Delegate::on_receiver_bounced.
TEST_P(IndexedDBTest, CloseThenAddReceiver) {
  const blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://localhost:81");
  auto bucket_locator = BucketLocator();
  bucket_locator.storage_key = storage_key;

  // Trigger the bucket context to be created.
  mojo::Remote<blink::mojom::IDBFactory> factory_remote1;
  BindFactory(
      mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>(),
      factory_remote1.BindNewPipeAndPassReceiver(),
      ToBucketInfo(bucket_locator));

  ASSERT_TRUE(context()->BucketContextExists(bucket_locator.id));

  // Remove the factory binding, and since there is no backing store yet, this
  // should trigger the destruction of the bucket context.
  factory_remote1.reset();

  // However, the bucket context still exists for now because shutdown is not
  // synchronous.
  ASSERT_TRUE(context()->BucketContextExists(bucket_locator.id));

  // Bind another IDB factory. It's important that this is called
  // synchronously because it will initially attempt to bind to the existing
  // bucket context above, but that fails in
  // BucketContext::AddReceiver().
  mojo::Remote<blink::mojom::IDBFactory> factory_remote2;
  BindFactory(
      mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>(),
      factory_remote2.BindNewPipeAndPassReceiver(),
      ToBucketInfo(bucket_locator));

  // Round trip a message through the new mojo pipe to verify that it is set
  // up correctly.
  factory_remote2.FlushForTesting();

  // It would be nice to re-verify that the new BucketContext is not the same
  // as the old one, but there's no good way to identify them through mojo and
  // no guarantee their memory addresses are different either.
}

// Tests that the backing store is closed when the connection is closed during
// upgrade.
TEST_P(IndexedDBTest, ConnectionCloseDuringUpgrade) {
  const blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://localhost:81");
  auto bucket_locator = BucketLocator();
  bucket_locator.storage_key = storage_key;

  // Bind the IDBFactory.
  mojo::Remote<blink::mojom::IDBFactory> factory_remote;
  mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
      checker_remote;
  BindFactory(std::move(checker_remote),
              factory_remote.BindNewPipeAndPassReceiver(),
              ToBucketInfo(bucket_locator));

  // Now create a database and thus the backing store.
  MockMojoFactoryClient client;
  MockMojoDatabaseCallbacks database_callbacks;
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
                       /*transaction_id=*/1, /*priority=*/0);
  run_loop.Run();

  ASSERT_TRUE(context_->BucketContextExists(bucket_locator.id));
  EXPECT_FALSE(GetBucketContext(bucket_locator.id)->IsClosing());

  // Drop the connection.
  pending_database.reset();
  factory_remote.FlushForTesting();
  EXPECT_TRUE(GetBucketContext(bucket_locator.id)->IsClosing());
}

TEST_P(IndexedDBTest, DeleteDatabase) {
  const blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://localhost:81");
  auto bucket_locator = BucketLocator();
  bucket_locator.storage_key = storage_key;

  // Bind the IDBFactory.
  mojo::Remote<blink::mojom::IDBFactory> factory_remote;
  mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
      checker_remote;
  BindFactory(std::move(checker_remote),
              factory_remote.BindNewPipeAndPassReceiver(),
              ToBucketInfo(bucket_locator));

  // Don't create a backing store if one doesn't exist.
  {
    // Delete db.
    MockMojoFactoryClient client;
    MockMojoDatabaseCallbacks database_callbacks;
    base::RunLoop run_loop;
    EXPECT_CALL(client, DeleteSuccess)
        .WillOnce(
            testing::DoAll(::base::test::RunClosure(run_loop.QuitClosure())));
    mojo::AssociatedRemote<blink::mojom::IDBTransaction> transaction_remote;
    factory_remote->DeleteDatabase(client.CreateInterfacePtrAndBind(), u"db",
                                   /*force_close=*/false);
    run_loop.Run();

    // Backing store shouldn't exist.
    ASSERT_TRUE(context_->BucketContextExists(bucket_locator.id));
    EXPECT_FALSE(GetBucketContext(bucket_locator.id)->backing_store());
  }

  // Now create a database and thus the backing store.
  {
    MockMojoFactoryClient client;
    MockMojoDatabaseCallbacks database_callbacks;
    base::RunLoop run_loop;
    EXPECT_CALL(client, MockedOpenSuccess)
        .WillOnce(::base::test::RunClosure(run_loop.QuitClosure()));
    mojo::AssociatedRemote<blink::mojom::IDBTransaction> transaction_remote;
    factory_remote->Open(client.CreateInterfacePtrAndBind(),
                         database_callbacks.CreateInterfacePtrAndBind(), u"db",
                         /*version=*/0,
                         transaction_remote.BindNewEndpointAndPassReceiver(),
                         /*transaction_id=*/1, /*priority=*/0);
    run_loop.Run();
  }

  // Delete the database now that the backing store actually exists.
  {
    MockMojoFactoryClient client;
    MockMojoDatabaseCallbacks database_callbacks;
    base::RunLoop run_loop;
    EXPECT_CALL(client, DeleteSuccess)
        .WillOnce(
            testing::DoAll(::base::test::RunClosure(run_loop.QuitClosure())));
    mojo::AssociatedRemote<blink::mojom::IDBTransaction> transaction_remote;
    factory_remote->DeleteDatabase(client.CreateInterfacePtrAndBind(), u"db",
                                   /*force_close=*/false);
    run_loop.Run();

    // Since there are no more references the factory should be closing.
    ASSERT_TRUE(context_->BucketContextExists(bucket_locator.id));
    EXPECT_TRUE(GetBucketContext(bucket_locator.id)->IsClosing());
  }
}

TEST_P(IndexedDBTest, GetDatabaseNames_NoFactory) {
  const blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://localhost:81");
  auto bucket_locator = BucketLocator();
  bucket_locator.storage_key = storage_key;

  // Bind the IDBFactory.
  mojo::Remote<blink::mojom::IDBFactory> factory_remote;
  mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
      checker_remote;
  BindFactory(std::move(checker_remote),
              factory_remote.BindNewPipeAndPassReceiver(),
              ToBucketInfo(bucket_locator));

  // Don't create a backing store if one doesn't exist.
  {
    base::test::TestFuture<std::vector<blink::mojom::IDBNameAndVersionPtr>,
                           blink::mojom::IDBErrorPtr>
        info_future;
    factory_remote->GetDatabaseInfo(info_future.GetCallback());
    ASSERT_TRUE(info_future.Wait());
    EXPECT_FALSE(GetBucketContext(bucket_locator.id)->backing_store());
  }

  // Now create a database and thus the backing store.
  MockMojoFactoryClient client;
  MockMojoDatabaseCallbacks database_callbacks;
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
                       /*transaction_id=*/1, /*priority=*/0);
  run_loop.Run();
  // GetDatabaseInfo didn't create the factory, so it shouldn't close it.
  {
    base::test::TestFuture<std::vector<blink::mojom::IDBNameAndVersionPtr>,
                           blink::mojom::IDBErrorPtr>
        info_future;
    factory_remote->GetDatabaseInfo(info_future.GetCallback());
    ASSERT_TRUE(info_future.Wait());

    ASSERT_TRUE(context_->BucketContextExists(bucket_locator.id));
    EXPECT_FALSE(GetBucketContext(bucket_locator.id)->IsClosing());
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
  auto bucket_locator = BucketLocator();
  bucket_locator.storage_key = storage_key;
  mojo::Remote<blink::mojom::IDBFactory> factory_remote;
  mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
      checker_remote;
  BindFactory(std::move(checker_remote),
              factory_remote.BindNewPipeAndPassReceiver(),
              ToBucketInfo(bucket_locator));

  // Expect an error when opening.
  MockMojoFactoryClient client;
  MockMojoDatabaseCallbacks database_callbacks;
  base::RunLoop run_loop;
  EXPECT_CALL(client, Error)
      .WillOnce(
          testing::DoAll(::base::test::RunClosure(run_loop.QuitClosure())));
  mojo::AssociatedRemote<blink::mojom::IDBTransaction> transaction_remote;
  factory_remote->Open(client.CreateInterfacePtrAndBind(),
                       database_callbacks.CreateInterfacePtrAndBind(), u"db",
                       /*version=*/1,
                       transaction_remote.BindNewEndpointAndPassReceiver(),
                       /*transaction_id=*/1, /*priority=*/0);
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
  auto bucket_locator = BucketLocator();
  bucket_locator.storage_key = storage_key;
  const std::u16string db_name(u"db");

  // Bind the IDBFactory.
  mojo::Remote<blink::mojom::IDBFactory> factory_remote;
  mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
      checker_remote;
  BindFactory(std::move(checker_remote),
              factory_remote.BindNewPipeAndPassReceiver(),
              ToBucketInfo(bucket_locator));

  // Open at version 2.
  {
    const int64_t db_version = 2;
    MockMojoFactoryClient client;
    MockMojoDatabaseCallbacks database_callbacks;
    base::RunLoop run_loop;
    EXPECT_CALL(client, MockedUpgradeNeeded)
        .WillOnce(
            testing::DoAll(::base::test::RunClosure(run_loop.QuitClosure())));
    mojo::AssociatedRemote<blink::mojom::IDBTransaction> transaction_remote;
    factory_remote->Open(client.CreateInterfacePtrAndBind(),
                         database_callbacks.CreateInterfacePtrAndBind(),
                         db_name, db_version,
                         transaction_remote.BindNewEndpointAndPassReceiver(),
                         /*transaction_id=*/1, /*priority=*/0);
    run_loop.Run();
  }

  // Open at version < 2, which will fail.
  {
    const int64_t db_version = 1;
    base::RunLoop run_loop;
    MockMojoFactoryClient client;
    MockMojoDatabaseCallbacks database_callbacks;
    EXPECT_CALL(client, Error)
        .WillOnce(::base::test::RunClosure(run_loop.QuitClosure()));
    mojo::AssociatedRemote<blink::mojom::IDBTransaction> transaction_remote;
    factory_remote->Open(client.CreateInterfacePtrAndBind(),
                         database_callbacks.CreateInterfacePtrAndBind(),
                         db_name, db_version,
                         transaction_remote.BindNewEndpointAndPassReceiver(),
                         /*transaction_id=*/2, /*priority=*/0);
    run_loop.Run();
    BucketContext* bucket_context = GetBucketContext(bucket_locator.id);
    ASSERT_TRUE(bucket_context);
    EXPECT_FALSE(
        base::Contains(bucket_context->GetDatabasesForTesting(), db_name));
  }
}

// Test for `IndexedDBDataFormatVersion`.
TEST_P(IndexedDBTest, DataLoss) {
  const blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://localhost:81");
  auto bucket_locator = BucketLocator();
  bucket_locator.storage_key = storage_key;
  const std::u16string db_name(u"test_db");

  // Bind the IDBFactory.
  mojo::Remote<blink::mojom::IDBFactory> factory_remote;
  mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
      checker_remote;
  BindFactory(std::move(checker_remote),
              factory_remote.BindNewPipeAndPassReceiver(),
              ToBucketInfo(bucket_locator));

  // Set a data format version and create a new database. No data loss.
  {
    base::AutoReset<IndexedDBDataFormatVersion> override_version(
        &IndexedDBDataFormatVersion::GetMutableCurrentForTesting(),
        IndexedDBDataFormatVersion(3, 4));
    MockMojoFactoryClient client;
    MockMojoDatabaseCallbacks database_callbacks;
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
                         /*transaction_id=*/1, /*priority=*/0);
    run_loop.Run();

    // This step is necessary to make sure the backing store is closed so that
    // the second `Open` will initialize it with the new (older) data format
    // version. Without this step, the same `BackingStore` is reused because
    // it's kept around for 2 seconds after the last connection is dropped.
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
    MockMojoFactoryClient client;
    MockMojoDatabaseCallbacks database_callbacks;
    EXPECT_CALL(client, MockedUpgradeNeeded(
                            _, _, blink::mojom::IDBDataLoss::Total, _, _))
        .WillOnce(
            testing::DoAll(::base::test::RunClosure(run_loop.QuitClosure())));
    mojo::AssociatedRemote<blink::mojom::IDBTransaction> transaction_remote;
    factory_remote->Open(client.CreateInterfacePtrAndBind(),
                         database_callbacks.CreateInterfacePtrAndBind(),
                         db_name, /*version=*/1,
                         transaction_remote.BindNewEndpointAndPassReceiver(),
                         /*transaction_id=*/2, /*priority=*/0);
    run_loop.Run();
  }
}

}  // namespace content::indexed_db
