// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <inttypes.h>

#include <cstdint>
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
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_file_util.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_id.h"
#include "components/services/storage/privileged/cpp/bucket_client_info.h"
#include "components/services/storage/privileged/mojom/indexed_db_client_state_checker.mojom.h"
#include "components/services/storage/privileged/mojom/indexed_db_control.mojom.h"
#include "components/services/storage/public/cpp/buckets/bucket_info.h"
#include "components/services/storage/public/cpp/buckets/bucket_init_params.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/cpp/buckets/constants.h"
#include "components/services/storage/public/cpp/quota_error_or.h"
#include "components/services/storage/public/mojom/storage_policy_update.mojom.h"
#include "content/browser/indexed_db/file_path_util.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_data_format_version.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "content/browser/indexed_db/indexed_db_reporting.h"
#include "content/browser/indexed_db/indexed_db_test_base.h"
#include "content/browser/indexed_db/instance/bucket_context.h"
#include "content/browser/indexed_db/instance/connection.h"
#include "content/browser/indexed_db/instance/leveldb/backing_store.h"
#include "content/browser/indexed_db/instance/mock_blob_storage_context.h"
#include "content/browser/indexed_db/instance/mock_file_system_access_context.h"
#include "content/browser/indexed_db/instance/sqlite/database_connection.h"
#include "content/browser/indexed_db/instance/test_blob_consumer.h"
#include "content/browser/indexed_db/mock_mojo_indexed_db_database_callbacks.h"
#include "content/browser/indexed_db/mock_mojo_indexed_db_factory_client.h"
#include "content/browser/indexed_db/status.h"
#include "env_chromium.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/features.h"
#include "net/base/schemeful_site.h"
#include "storage/browser/test/fake_blob.h"
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
using storage::BucketInfo;
using storage::BucketLocator;
using testing::_;

namespace content::indexed_db {
namespace {

constexpr char16_t kDatabaseName[] = u"db";

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

BucketInfo ToBucketInfo(const BucketLocator& bucket_locator) {
  BucketInfo bucket_info;
  bucket_info.id = bucket_locator.id;
  bucket_info.storage_key = bucket_locator.storage_key;
  if (bucket_locator.is_default) {
    bucket_info.name = storage::kDefaultBucketName;
  } else {
    bucket_info.name = "test_bucket_name";
  }
  return bucket_info;
}

// Helper to simplify opening an IDBDatabase connection via mojo interfaces.
// This keeps tests from needing to define a ton of local variables. Use
// `CreateDatabase()` instead if the held variables aside from `database` won't
// be necessary.
struct MojoConnectionHelper {
  MojoConnectionHelper()
      : MojoConnectionHelper(kDatabaseName,
                             /*version=*/1,
                             /*upgrade_txn_id=*/1) {}

  MojoConnectionHelper(std::u16string db_name,
                       int64_t version,
                       int64_t upgrade_txn_id)
      : db_name(std::move(db_name)),
        version(version),
        upgrade_txn_id(upgrade_txn_id) {}

  MojoConnectionHelper(const MojoConnectionHelper&) = delete;
  MojoConnectionHelper& operator=(const MojoConnectionHelper&) = delete;

  void Open(blink::mojom::IDBFactory* factory) {
    factory->Open(open_callbacks.CreateInterfacePtrAndBind(),
                  connection_callbacks.CreateInterfacePtrAndBind(), db_name,
                  version, vc_txn.BindNewEndpointAndPassReceiver(),
                  upgrade_txn_id, /*priority=*/0);
  }

  void OpenAndExpectUpgradeNeeded(
      blink::mojom::IDBFactory* factory,
      int64_t old_version = IndexedDBDatabaseMetadata::NO_VERSION) {
    base::RunLoop loop;
    mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_database;
    EXPECT_CALL(open_callbacks,
                MockedUpgradeNeeded(
                    IsAssociatedInterfacePtrInfoValid(true), old_version,
                    blink::mojom::IDBDataLoss::None, std::string(), _))
        .WillOnce(testing::DoAll(MoveArgPointee<0>(&pending_database),
                                 testing::SaveArg<4>(&metadata),
                                 QuitLoop(&loop)));
    Open(factory);
    loop.Run();
    ASSERT_TRUE(pending_database.is_valid());
    database.Bind(std::move(pending_database));
  }

  std::u16string db_name;
  int64_t version;
  int64_t upgrade_txn_id;

  mojo::AssociatedRemote<blink::mojom::IDBDatabase> database;
  mojo::AssociatedRemote<blink::mojom::IDBTransaction> vc_txn;

  IndexedDBDatabaseMetadata metadata;
  MockMojoFactoryClient open_callbacks;
  MockMojoDatabaseCallbacks connection_callbacks;
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

// Name subject to change pending further refactoring.
class IndexedDBTestBaseWithExtras : public IndexedDBTestBase {
 public:
  BucketLocator kNormalFirstPartyBucketLocator;
  BucketLocator kSessionOnlyFirstPartyBucketLocator;
  BucketLocator kNormalThirdPartyBucketLocator;
  BucketLocator kSessionOnlyThirdPartyBucketLocator;

  IndexedDBTestBaseWithExtras(bool use_default_buckets, bool use_sqlite)
      : IndexedDBTestBase(use_default_buckets, use_sqlite) {}

  IndexedDBTestBaseWithExtras(const IndexedDBTestBaseWithExtras&) = delete;
  IndexedDBTestBaseWithExtras& operator=(const IndexedDBTestBaseWithExtras&) =
      delete;

  ~IndexedDBTestBaseWithExtras() override = default;

  void SetUpStorageKeysForSessionOnlyTests() {
    kNormalFirstPartyBucketLocator =
        GetOrCreateBucket(
            blink::StorageKey::CreateFromStringForTesting("http://normal.com/"))
            .ToBucketLocator();

    kSessionOnlyFirstPartyBucketLocator =
        GetOrCreateBucket(blink::StorageKey::CreateFromStringForTesting(
                              "http://session-only.com/"))
            .ToBucketLocator();

    kNormalThirdPartyBucketLocator =
        GetOrCreateBucket(blink::StorageKey::Create(
                              url::Origin::Create(GURL("http://normal.com/")),
                              net::SchemefulSite(GURL("http://rando.com/")),
                              blink::mojom::AncestorChainBit::kCrossSite))
            .ToBucketLocator();

    kSessionOnlyThirdPartyBucketLocator =
        GetOrCreateBucket(
            blink::StorageKey::Create(
                url::Origin::Create(GURL("http://session-only.com/")),
                net::SchemefulSite(GURL("http://rando.com/")),
                blink::mojom::AncestorChainBit::kCrossSite))
            .ToBucketLocator();
  }

  void FastForwardToCloseStore() {
    if (IsSqliteBackingStoreEnabled()) {
      // The SQLite store has an added delay between when an individual database
      // connection is dropped and when store shutdown is initiated.
      task_environment_.FastForwardBy(
          sqlite::DatabaseConnection::GetDestructionGracePeriodForTesting());
    }
    task_environment_.FastForwardBy(
        BucketContext::GetBackingStoreGracePeriodForTesting());
  }

  bool IsThirdPartyStoragePartitioningEnabled() {
    // Enabled by default since 2023 for most platforms, but still off by
    // default for Android WebView.
    return base::FeatureList::IsEnabled(
        net::features::kThirdPartyStoragePartitioning);
  }

  bool DeleteBucket(const BucketInfo* bucket_info) {
    base::test::TestFuture<blink::mojom::QuotaStatusCode> result_code;
    context()->DeleteBucketData(bucket_info->ToBucketLocator(),
                                result_code.GetCallback());
    return result_code.Get() == blink::mojom::QuotaStatusCode::kOk;
  }

  // Opens a database connection, runs `action`, and verifies that the
  // connection was forced closed.
  void VerifyForcedClosedCalled(base::OnceClosure action,
                                BucketInfo* out_info = nullptr) {
    BucketInfo bucket_info = GetOrCreateBucket(GetTestStorageKey());
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
    std::optional<base::RunLoop> run_loop;
    run_loop.emplace();
    // It's necessary to hang onto the database connection or the connection
    // will shut itself down and there will be no `ForcedClosed()`.
    mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_database;
    EXPECT_CALL(client, MockedUpgradeNeeded)
        .WillOnce(testing::DoAll(MoveArgPointee<0>(&pending_database),
                                 RunClosure(run_loop->QuitClosure())));
    mojo::AssociatedRemote<blink::mojom::IDBTransaction> transaction_remote;
    factory_remote_->Open(client.CreateInterfacePtrAndBind(),
                          database_callbacks.CreateInterfacePtrAndBind(),
                          u"opendb",
                          blink::IndexedDBDatabaseMetadata::NO_VERSION,
                          transaction_remote.BindNewEndpointAndPassReceiver(),
                          /*host_transaction_id=*/0, /*priority=*/0);
    run_loop->Run();
    EXPECT_TRUE(base::DirectoryExists(test_path));

    // Expect that deleting the data force closes the open database connection.
    run_loop.emplace();
    EXPECT_CALL(database_callbacks, ForcedClose())
        .WillOnce(RunClosure(run_loop->QuitClosure()));
    std::move(action).Run();
    run_loop->Run();
  }

  void VerifyBucketContextWaitIfNeeded(const BucketLocator& bucket_locator,
                                       bool expected_context_exists) {
    while (expected_context_exists !=
           context()->BucketContextExists(bucket_locator)) {
      RunPostedTasks();
    }
    VerifyBucketContext(bucket_locator, expected_context_exists);
  }

  void VerifyBucketContext(
      const BucketLocator& bucket_locator,
      bool expected_context_exists,
      std::optional<bool> expected_backing_store_exists = std::nullopt) {
    BucketContext* bucket_context = GetBucketContext(bucket_locator.id);
    if (!expected_context_exists) {
      EXPECT_FALSE(bucket_context);
      EXPECT_FALSE(expected_backing_store_exists.has_value());
    } else {
      ASSERT_TRUE(bucket_context);
      if (expected_backing_store_exists.has_value()) {
        EXPECT_EQ(*expected_backing_store_exists,
                  !!bucket_context->backing_store());
      }
    }
  }
};

// Parametrized by SQLite (true) vs LevelDB (false) backing store.
class IndexedDBTest : public IndexedDBTestBaseWithExtras,
                      public testing::WithParamInterface<bool> {
 public:
  IndexedDBTest()
      : IndexedDBTestBaseWithExtras(/*use_default_buckets=*/true,
                                    /*use_sqlite=*/GetParam()) {}

  // Running tasks on the BucketContext will trigger the start of the close
  // timer if there are no open databases (among other conditions).
  void NudgeBackingStoreCloseLogic(BucketContext* bucket_context) {
    bucket_context->RunTasks();
  }
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    IndexedDBTest,
    /*use SQLite backing store*/ testing::Bool(),
    [](const testing::TestParamInfo<IndexedDBTest::ParamType>& info) {
      return info.param ? "SQLite" : "LevelDB";
    });

// Parametrized by "default" bucket (true) vs "non-default" bucket (false),
// and SQLite (true) vs LevelDB (false) backing store. These two bucket types
// use different base paths, which is relevant to code paths that manipulate
// files on disk.
class IndexedDBTestWithBucketType
    : public IndexedDBTestBaseWithExtras,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  IndexedDBTestWithBucketType()
      : IndexedDBTestBaseWithExtras(
            /*use_default_buckets=*/std::get<0>(GetParam()),
            /*use_sqlite=*/std::get<1>(GetParam())) {}
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    IndexedDBTestWithBucketType,
    testing::Combine(/*is_default_bucket=*/testing::Bool(),
                     /*use_sqlite=*/testing::Bool()),
    [](const testing::TestParamInfo<IndexedDBTestWithBucketType::ParamType>&
           info) {
      return std::string(std::get<0>(info.param) ? "Default" : "NonDefault") +
             "_" + std::string(std::get<1>(info.param) ? "SQLite" : "LevelDB");
    });

TEST_P(IndexedDBTest, CloseConnectionBeforeUpgrade) {
  IndexedDBDatabaseMetadata metadata;
  mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_database;

  // Bind the IDBFactory.
  mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
      checker_remote;
  mojo::Remote<blink::mojom::IDBFactory> bounded_factory_remote;
  BindFactory(std::move(checker_remote),
              bounded_factory_remote.BindNewPipeAndPassReceiver(),
              BucketInfo());

  base::RunLoop loop;
  MojoConnectionHelper mojo_helper;
  EXPECT_CALL(
      mojo_helper.open_callbacks,
      MockedUpgradeNeeded(IsAssociatedInterfacePtrInfoValid(true),
                          IndexedDBDatabaseMetadata::NO_VERSION,
                          blink::mojom::IDBDataLoss::None, std::string(""), _))
      .WillOnce(testing::DoAll(MoveArgPointee<0>(&pending_database),
                               testing::SaveArg<4>(&metadata),
                               QuitLoop(&loop)));

  mojo_helper.Open(bounded_factory_remote.get());

  loop.Run();

  EXPECT_TRUE(pending_database.is_valid());
  EXPECT_EQ(mojo_helper.version, metadata.version);
  EXPECT_EQ(mojo_helper.db_name, metadata.name);
}

TEST_P(IndexedDBTest, CloseAfterUpgrade) {
  const int64_t kObjectStoreId = 10;
  const char16_t kObjectStoreName[] = u"os";

  // Bind the IDBFactory.
  mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
      checker_remote;
  mojo::Remote<blink::mojom::IDBFactory> bounded_factory_remote;
  BindFactory(std::move(checker_remote),
              bounded_factory_remote.BindNewPipeAndPassReceiver(),
              BucketInfo());

  MojoConnectionHelper mojo_helper;
  mojo_helper.OpenAndExpectUpgradeNeeded(bounded_factory_remote.get());
  EXPECT_EQ(mojo_helper.version, mojo_helper.metadata.version);
  EXPECT_EQ(mojo_helper.db_name, mojo_helper.metadata.name);

  base::RunLoop loop;
  base::RepeatingClosure quit_closure =
      base::BarrierClosure(2, loop.QuitClosure());

  ::testing::InSequence dummy;
  EXPECT_CALL(mojo_helper.connection_callbacks,
              Complete(mojo_helper.upgrade_txn_id))
      .Times(1)
      .WillOnce(RunClosure(quit_closure));
  EXPECT_CALL(mojo_helper.open_callbacks,
              MockedOpenSuccess(IsAssociatedInterfacePtrInfoValid(false), _))
      .Times(1)
      .WillOnce(RunClosure(std::move(quit_closure)));

  mojo_helper.vc_txn->CreateObjectStore(kObjectStoreId, kObjectStoreName,
                                        blink::IndexedDBKeyPath(), false);
  mojo_helper.vc_txn->Commit(0);

  loop.Run();
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
  mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_database1;
  IndexedDBDatabaseMetadata metadata1;

  // Bind the IDBFactory.
  mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
      checker_remote;
  mojo::Remote<blink::mojom::IDBFactory> bounded_factory_remote;
  BindFactory(std::move(checker_remote),
              bounded_factory_remote.BindNewPipeAndPassReceiver(),
              BucketInfo());

  std::optional<base::RunLoop> loop;
  loop.emplace();
  // Open connection 1, and expect the upgrade needed.
  MojoConnectionHelper mojo_helper1(kDatabaseName, kDBVersion, kTransactionId);

  EXPECT_CALL(
      mojo_helper1.open_callbacks,
      MockedUpgradeNeeded(IsAssociatedInterfacePtrInfoValid(true),
                          IndexedDBDatabaseMetadata::NO_VERSION,
                          blink::mojom::IDBDataLoss::None, std::string(""), _))
      .WillOnce(testing::DoAll(MoveArgPointee<0>(&pending_database1),
                               testing::SaveArg<4>(&metadata1),
                               QuitLoop(&*loop)));

  // Queue open request message.
  mojo_helper1.Open(bounded_factory_remote.get());

  loop->Run();

  mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_database2;
  IndexedDBDatabaseMetadata metadata2;

  loop.emplace();
  base::RepeatingClosure quit_closure =
      base::BarrierClosure(3, loop->QuitClosure());

  MojoConnectionHelper mojo_helper2(kDatabaseName, kDBVersion, 0);

  // Check that we're called in order and the second connection gets it's
  // database after the first connection completes.
  ::testing::InSequence dummy;
  EXPECT_CALL(mojo_helper1.connection_callbacks, Complete(kTransactionId))
      .Times(1)
      .WillOnce(RunClosure(quit_closure));
  EXPECT_CALL(mojo_helper1.open_callbacks,
              MockedOpenSuccess(IsAssociatedInterfacePtrInfoValid(false), _))
      .Times(1)
      .WillOnce(RunClosure(quit_closure));
  EXPECT_CALL(mojo_helper2.open_callbacks,
              MockedOpenSuccess(IsAssociatedInterfacePtrInfoValid(true), _))
      .WillOnce(testing::DoAll(MoveArgPointee<0>(&pending_database2),
                               testing::SaveArg<1>(&metadata2),
                               RunClosure(std::move(quit_closure))));

  mojo_helper1.database.Bind(std::move(pending_database1));
  ASSERT_TRUE(mojo_helper1.database.is_bound());
  ASSERT_TRUE(mojo_helper1.vc_txn.is_bound());

  // Open connection 2, but expect that we won't be called back.
  mojo_helper2.Open(bounded_factory_remote.get());

  // Create object store.
  mojo_helper1.vc_txn->CreateObjectStore(kObjectStoreId, kObjectStoreName,
                                         blink::IndexedDBKeyPath(), false);
  mojo_helper1.vc_txn->Commit(0);

  loop->Run();

  EXPECT_TRUE(pending_database2.is_valid());
  EXPECT_EQ(mojo_helper2.version, metadata2.version);
  EXPECT_EQ(mojo_helper2.db_name, metadata2.name);
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
  const int64_t kObjectStoreId = 10;
  const char16_t kObjectStoreName[] = u"os";

  // Bind the IDBFactory.
  mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
      checker_remote;
  mojo::Remote<blink::mojom::IDBFactory> bounded_factory_remote;
  BindFactory(std::move(checker_remote),
              bounded_factory_remote.BindNewPipeAndPassReceiver(),
              BucketInfo());

  MojoConnectionHelper mojo_helper;
  mojo_helper.OpenAndExpectUpgradeNeeded(bounded_factory_remote.get());
  EXPECT_EQ(mojo_helper.version, mojo_helper.metadata.version);
  EXPECT_EQ(mojo_helper.db_name, mojo_helper.metadata.name);

  base::MockCallback<blink::mojom::IDBTransaction::PutCallback> put_callback;

  base::RunLoop loop;
  base::RepeatingClosure quit_closure =
      base::BarrierClosure(3, loop.QuitClosure());

  ::testing::InSequence dummy;

  EXPECT_CALL(put_callback,
              Run(IsCallbackError(blink::mojom::IDBException::kUnknownError)))
      .Times(1)
      .WillOnce(RunClosure(quit_closure));

  EXPECT_CALL(mojo_helper.connection_callbacks,
              Abort(mojo_helper.upgrade_txn_id,
                    blink::mojom::IDBException::kUnknownError, _))
      .Times(1)
      .WillOnce(RunClosure(quit_closure));

  EXPECT_CALL(mojo_helper.open_callbacks,
              Error(blink::mojom::IDBException::kAbortError, _))
      .Times(1)
      .WillOnce(RunClosure(std::move(quit_closure)));

  mojo_helper.vc_txn->CreateObjectStore(kObjectStoreId, kObjectStoreName,
                                        blink::IndexedDBKeyPath(), false);
  // Call Put with an invalid blob.
  std::vector<blink::mojom::IDBExternalObjectPtr> external_objects;
  mojo::PendingRemote<blink::mojom::Blob> blob;
  // Ignore the result of InitWithNewPipeAndPassReceiver, to end up with
  // an invalid blob.
  std::ignore = blob.InitWithNewPipeAndPassReceiver();
  external_objects.push_back(blink::mojom::IDBExternalObject::NewBlobOrFile(
      blink::mojom::IDBBlobInfo::New(std::move(blob), std::u16string(), 100,
                                     nullptr)));

  auto new_value = blink::mojom::IDBValue::New();
  new_value->bits = mojo_base::BigBuffer(base::as_byte_span("hello"));
  new_value->external_objects = std::move(external_objects);

  mojo_helper.vc_txn->Put(
      kObjectStoreId, std::move(new_value), IndexedDBKey(u"hello"),
      blink::mojom::IDBPutMode::AddOnly, std::vector<IndexedDBIndexKeys>(),
      put_callback.Get());
  mojo_helper.vc_txn->Commit(0);

  loop.Run();
}

// Regression test for crbug.com/461720662. When run under ASAN, the test
// verifies that a Transaction can be destroyed inside `Transaction::RunTasks`
// without causing UAF.
TEST_P(IndexedDBTest, InvalidObjectStoreId) {
  const int64_t kObjectStoreId = 10;
  const int64_t kIndexId = 100;
  const char16_t kObjectStoreName[] = u"os";
  const char16_t kIndexName[] = u"index";

  // Bind the IDBFactory.
  mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
      checker_remote;
  mojo::Remote<blink::mojom::IDBFactory> bounded_factory_remote;
  const BucketLocator bucket_locator = InitBucketContext()->bucket_locator();
  BindFactory(std::move(checker_remote),
              bounded_factory_remote.BindNewPipeAndPassReceiver(),
              ToBucketInfo(bucket_locator));

  MojoConnectionHelper mojo_helper;
  mojo_helper.OpenAndExpectUpgradeNeeded(bounded_factory_remote.get());

  {
    mojo::test::BadMessageObserver bad_message_observer;
    mojo_helper.vc_txn->CreateObjectStore(kObjectStoreId, kObjectStoreName,
                                          blink::IndexedDBKeyPath(), false);
    mojo_helper.database->CreateIndex(
        mojo_helper.upgrade_txn_id, kObjectStoreId + 123,
        blink::IndexedDBIndexMetadata(kIndexName, kIndexId,
                                      blink::IndexedDBKeyPath(), false, false));

    EXPECT_EQ("Invalid object_store_id or index_id.",
              bad_message_observer.WaitForBadMessage());
  }
}

TEST_P(IndexedDBTest, NotifyIndexedDBListChanged) {
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

  EXPECT_EQ(0, observer.notify_list_changed_count);
  EXPECT_EQ(0, observer.notify_content_changed_count);

  // Bind the IDBFactory.
  mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
      checker_remote;
  mojo::Remote<blink::mojom::IDBFactory> bounded_factory_remote;
  const BucketLocator bucket_locator = InitBucketContext()->bucket_locator();
  BindFactory(std::move(checker_remote),
              bounded_factory_remote.BindNewPipeAndPassReceiver(),
              ToBucketInfo(bucket_locator));

  // Open connection 1.
  auto mojo_helper = std::make_unique<MojoConnectionHelper>(
      kDatabaseName, kDBVersion1, kTransactionId1);
  EXPECT_EQ(0, observer.notify_list_changed_count);
  mojo_helper->OpenAndExpectUpgradeNeeded(bounded_factory_remote.get());
  EXPECT_EQ(mojo_helper->version, mojo_helper->metadata.version);
  EXPECT_EQ(mojo_helper->db_name, mojo_helper->metadata.name);

  {
    ::testing::InSequence dummy;
    base::RunLoop loop;
    base::RepeatingClosure quit_closure =
        base::BarrierClosure(2, loop.QuitClosure());

    EXPECT_CALL(mojo_helper->connection_callbacks, Complete(kTransactionId1))
        .Times(1)
        .WillOnce(RunClosure(quit_closure));
    EXPECT_CALL(mojo_helper->open_callbacks,
                MockedOpenSuccess(IsAssociatedInterfacePtrInfoValid(false), _))
        .Times(1)
        .WillOnce(RunClosure(std::move(quit_closure)));

    mojo_helper->vc_txn->CreateObjectStore(kObjectStoreId, kObjectStoreName,
                                           blink::IndexedDBKeyPath(), false);
    mojo_helper->database->CreateIndex(
        kTransactionId1, kObjectStoreId,
        blink::IndexedDBIndexMetadata(kIndexName, kIndexId,
                                      blink::IndexedDBKeyPath(), false, false));
    mojo_helper->vc_txn->Commit(0);

    loop.Run();
  }

  // 1 from backing store initialization and 1 from transaction commit.
  EXPECT_EQ(2, observer.notify_list_changed_count);

  // Connection need to be closed before opening another connection. Because if
  // one connection triggers a version change, it can affect other open
  // connections as well.
  mojo_helper = std::make_unique<MojoConnectionHelper>(
      kDatabaseName, kDBVersion2, kTransactionId2);
  mojo_helper->OpenAndExpectUpgradeNeeded(bounded_factory_remote.get(),
                                          kDBVersion1);
  EXPECT_EQ(mojo_helper->version, mojo_helper->metadata.version);
  EXPECT_EQ(mojo_helper->db_name, mojo_helper->metadata.name);

  {
    ::testing::InSequence dummy;
    base::RunLoop loop;
    base::RepeatingClosure quit_closure =
        base::BarrierClosure(2, loop.QuitClosure());

    EXPECT_CALL(mojo_helper->connection_callbacks, Complete(kTransactionId2))
        .Times(1)
        .WillOnce(RunClosure(quit_closure));
    EXPECT_CALL(mojo_helper->open_callbacks,
                MockedOpenSuccess(IsAssociatedInterfacePtrInfoValid(false), _))
        .Times(1)
        .WillOnce(RunClosure(std::move(quit_closure)));

    mojo_helper->database->DeleteIndex(kTransactionId2, kObjectStoreId,
                                       kIndexId);
    mojo_helper->vc_txn->Commit(0);

    loop.Run();
  }
  EXPECT_EQ(3, observer.notify_list_changed_count);

  // Open connection 3.
  mojo_helper = std::make_unique<MojoConnectionHelper>(
      kDatabaseName, kDBVersion3, kTransactionId3);
  mojo_helper->OpenAndExpectUpgradeNeeded(bounded_factory_remote.get(),
                                          kDBVersion2);
  EXPECT_EQ(mojo_helper->version, mojo_helper->metadata.version);
  EXPECT_EQ(mojo_helper->db_name, mojo_helper->metadata.name);

  {
    ::testing::InSequence dummy;
    base::RunLoop loop;
    base::RepeatingClosure quit_closure =
        base::BarrierClosure(2, loop.QuitClosure());

    EXPECT_CALL(mojo_helper->connection_callbacks, Complete(kTransactionId3))
        .Times(1)
        .WillOnce(RunClosure(quit_closure));
    EXPECT_CALL(mojo_helper->open_callbacks,
                MockedOpenSuccess(IsAssociatedInterfacePtrInfoValid(false), _))
        .Times(1)
        .WillOnce(RunClosure(std::move(quit_closure)));

    mojo_helper->vc_txn->DeleteObjectStore(kObjectStoreId);
    mojo_helper->vc_txn->Commit(0);

    loop.Run();
  }
  EXPECT_EQ(4, observer.notify_list_changed_count);
}

MATCHER(IsSuccessKey, "") {
  return arg->is_key();
}

TEST_P(IndexedDBTest, NotifyIndexedDBContentChanged) {
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

  // Bind the IDBFactory.
  mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
      checker_remote;
  mojo::Remote<blink::mojom::IDBFactory> bounded_factory_remote;

  const BucketLocator bucket_locator = InitBucketContext()->bucket_locator();
  BindFactory(std::move(checker_remote),
              bounded_factory_remote.BindNewPipeAndPassReceiver(),
              ToBucketInfo(bucket_locator));

  // Open connection 1.
  auto mojo_helper = std::make_unique<MojoConnectionHelper>(
      kDatabaseName, kDBVersion1, kTransactionId1);
  mojo_helper->OpenAndExpectUpgradeNeeded(bounded_factory_remote.get());
  EXPECT_EQ(mojo_helper->version, mojo_helper->metadata.version);
  EXPECT_EQ(mojo_helper->db_name, mojo_helper->metadata.name);

  base::MockCallback<blink::mojom::IDBTransaction::PutCallback> put_callback;

  // Add object store entry.
  std::optional<base::RunLoop> loop;
  loop.emplace();
  base::RepeatingClosure quit_closure =
      base::BarrierClosure(3, loop->QuitClosure());
  ::testing::InSequence dummy;

  EXPECT_CALL(put_callback, Run(IsSuccessKey()))
      .Times(1)
      .WillOnce(RunClosure(quit_closure));
  EXPECT_CALL(mojo_helper->connection_callbacks, Complete(kTransactionId1))
      .Times(1)
      .WillOnce(RunClosure(quit_closure));
  EXPECT_CALL(mojo_helper->open_callbacks,
              MockedOpenSuccess(IsAssociatedInterfacePtrInfoValid(false), _))
      .Times(1)
      .WillOnce(RunClosure(std::move(quit_closure)));

  mojo_helper->vc_txn->CreateObjectStore(kObjectStoreId, kObjectStoreName,
                                         blink::IndexedDBKeyPath(), false);

  auto new_value = blink::mojom::IDBValue::New();
  auto value = base::span_from_cstring("value");
  new_value->bits = mojo_base::BigBuffer(base::as_bytes(value));

  mojo_helper->vc_txn->Put(
      kObjectStoreId, std::move(new_value), IndexedDBKey(u"key"),
      blink::mojom::IDBPutMode::AddOnly, std::vector<IndexedDBIndexKeys>(),
      put_callback.Get());
  mojo_helper->vc_txn->Commit(0);

  loop->Run();

  EXPECT_EQ(2, observer.notify_list_changed_count);
  EXPECT_EQ(1, observer.notify_content_changed_count);

  // Connection need to be closed before opening another connection. Because if
  // one connection triggers a version change, it can affect other open
  // connections as well.

  // Open connection 2.
  mojo_helper = std::make_unique<MojoConnectionHelper>(
      kDatabaseName, kDBVersion2, kTransactionId2);
  mojo_helper->OpenAndExpectUpgradeNeeded(bounded_factory_remote.get(),
                                          kDBVersion1);
  EXPECT_EQ(mojo_helper->version, mojo_helper->metadata.version);
  EXPECT_EQ(mojo_helper->db_name, mojo_helper->metadata.name);

  // Clear object store.
  loop.emplace();
  quit_closure = base::BarrierClosure(3, loop->QuitClosure());

  EXPECT_CALL(mojo_helper->connection_callbacks, Complete(kTransactionId2))
      .Times(1)
      .WillOnce(RunClosure(quit_closure));
  EXPECT_CALL(mojo_helper->open_callbacks,
              MockedOpenSuccess(IsAssociatedInterfacePtrInfoValid(false), _))
      .Times(1)
      .WillOnce(RunClosure(quit_closure));

  mojo_helper->database->Clear(kTransactionId2, kObjectStoreId,
                               base::IgnoreArgs<bool>(quit_closure));
  mojo_helper->vc_txn->Commit(0);

  loop->Run();

  // +1 list changed for the transaction
  EXPECT_EQ(3, observer.notify_list_changed_count);
  EXPECT_EQ(2, observer.notify_content_changed_count);
}

// The test is flaky. See https://crbug.com/324282438
TEST_P(IndexedDBTest, DISABLED_DatabaseOperationSequencing) {
  const std::u16string kObjectStoreName1 = u"os1";
  const std::u16string kObjectStoreName2 = u"os2";
  const std::u16string kObjectStoreName3 = u"os3";

  // Bind the IDBFactory.
  mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
      checker_remote;
  mojo::Remote<blink::mojom::IDBFactory> bounded_factory_remote;
  BindFactory(std::move(checker_remote),
              bounded_factory_remote.BindNewPipeAndPassReceiver(),
              BucketInfo());

  MojoConnectionHelper mojo_helper;
  mojo_helper.OpenAndExpectUpgradeNeeded(bounded_factory_remote.get());
  EXPECT_EQ(mojo_helper.version, mojo_helper.metadata.version);
  EXPECT_EQ(mojo_helper.db_name, mojo_helper.metadata.name);
  EXPECT_EQ(0ULL, mojo_helper.metadata.object_stores.size());

  // Within the "upgrade" transaction, create/delete/create object store. This
  // should leave only one store around if everything is processed in the
  // correct order.
  IndexedDBDatabaseMetadata metadata2;
  int64_t object_store_id = 1001;

  base::RunLoop loop;
  base::RepeatingClosure quit_closure =
      base::BarrierClosure(2, loop.QuitClosure());
  ::testing::InSequence dummy;

  EXPECT_CALL(mojo_helper.connection_callbacks,
              Complete(mojo_helper.upgrade_txn_id))
      .Times(1)
      .WillOnce(RunClosure(quit_closure));
  EXPECT_CALL(mojo_helper.open_callbacks,
              MockedOpenSuccess(IsAssociatedInterfacePtrInfoValid(false), _))
      .Times(1)
      .WillOnce(testing::DoAll(testing::SaveArg<1>(&metadata2),
                               RunClosure(std::move(quit_closure))));

  // This will cause a CreateObjectStoreOperation to be queued and
  // run synchronously...
  mojo_helper.vc_txn->CreateObjectStore(++object_store_id, kObjectStoreName1,
                                        blink::IndexedDBKeyPath(),
                                        /*auto_increment=*/false);

  // The following operations will queue operations, but the
  // operations will run asynchronously.

  // First, delete the previous store. Ensure that this succeeds
  // even if the previous action completed synchronously.
  mojo_helper.vc_txn->DeleteObjectStore(object_store_id);

  // Ensure that a create/delete pair where both parts are queued
  // succeeds.
  mojo_helper.vc_txn->CreateObjectStore(++object_store_id, kObjectStoreName2,
                                        blink::IndexedDBKeyPath(),
                                        /*auto_increment=*/false);
  mojo_helper.vc_txn->DeleteObjectStore(object_store_id);

  // This store is left over, just to verify that everything
  // ran correctly.
  mojo_helper.vc_txn->CreateObjectStore(++object_store_id, kObjectStoreName3,
                                        blink::IndexedDBKeyPath(),
                                        /*auto_increment=*/false);

  mojo_helper.vc_txn->Commit(0);

  loop.Run();

  EXPECT_EQ(1ULL, metadata2.object_stores.size());
  EXPECT_EQ(metadata2.object_stores[object_store_id].name, kObjectStoreName3);
}

TEST_P(IndexedDBTest, ClearSessionOnlyDatabases) {
  SetUpStorageKeysForSessionOnlyTests();

  std::vector<storage::mojom::StoragePolicyUpdatePtr> policy_updates;
  policy_updates.emplace_back(storage::mojom::StoragePolicyUpdate::New(
      url::Origin::Create(GURL("http://subdomain.session-only.com")),
      /*should_purge_on_shutdown=*/true));
  context_->ApplyPolicyUpdates(std::move(policy_updates));

  // Create additional storage keys/buckets only needed by this test.
  blink::StorageKey session_only_subdomain_first_party_storage_key =
      blink::StorageKey::CreateFromStringForTesting(
          "http://subdomain.session-only.com/");
  BucketInfo bucket_info =
      GetOrCreateBucket(session_only_subdomain_first_party_storage_key);
  BucketLocator session_only_subdomain_first_party_bucket_locator =
      bucket_info.ToBucketLocator();

  blink::StorageKey session_only_subdomain_third_party_storage_key =
      blink::StorageKey::Create(
          url::Origin::Create(GURL("http://subdomain.session-only.com/")),
          net::SchemefulSite(GURL("http://rando.com/")),
          blink::mojom::AncestorChainBit::kCrossSite);
  bucket_info =
      GetOrCreateBucket(session_only_subdomain_third_party_storage_key);
  BucketLocator session_only_subdomain_third_party_bucket_locator =
      bucket_info.ToBucketLocator();

  blink::StorageKey inverted_normal_third_party_storage_key =
      blink::StorageKey::Create(url::Origin::Create(GURL("http://rando.com/")),
                                net::SchemefulSite(GURL("http://normal.com/")),
                                blink::mojom::AncestorChainBit::kCrossSite);
  bucket_info = GetOrCreateBucket(inverted_normal_third_party_storage_key);
  BucketLocator inverted_normal_third_party_bucket_locator =
      bucket_info.ToBucketLocator();

  blink::StorageKey inverted_session_only_third_party_storage_key =
      blink::StorageKey::Create(
          url::Origin::Create(GURL("http://rando.com/")),
          net::SchemefulSite(GURL("http://session-only.com/")),
          blink::mojom::AncestorChainBit::kCrossSite);
  bucket_info =
      GetOrCreateBucket(inverted_session_only_third_party_storage_key);
  BucketLocator inverted_session_only_third_party_bucket_locator =
      bucket_info.ToBucketLocator();

  blink::StorageKey inverted_session_only_subdomain_third_party_storage_key =
      blink::StorageKey::Create(
          url::Origin::Create(GURL("http://rando.com/")),
          net::SchemefulSite(GURL("http://subdomain.session-only.com/")),
          blink::mojom::AncestorChainBit::kCrossSite);
  bucket_info = GetOrCreateBucket(
      inverted_session_only_subdomain_third_party_storage_key);
  BucketLocator inverted_session_only_subdomain_third_party_bucket_locator =
      bucket_info.ToBucketLocator();

  base::FilePath normal_path_first_party =
      GetFilePathForTesting(kNormalFirstPartyBucketLocator);
  base::FilePath session_only_path_first_party =
      GetFilePathForTesting(kSessionOnlyFirstPartyBucketLocator);
  base::FilePath session_only_subdomain_path_first_party =
      GetFilePathForTesting(session_only_subdomain_first_party_bucket_locator);
  base::FilePath normal_path_third_party =
      GetFilePathForTesting(kNormalThirdPartyBucketLocator);
  base::FilePath session_only_path_third_party =
      GetFilePathForTesting(kSessionOnlyThirdPartyBucketLocator);
  base::FilePath session_only_subdomain_path_third_party =
      GetFilePathForTesting(session_only_subdomain_third_party_bucket_locator);
  base::FilePath inverted_normal_path_third_party =
      GetFilePathForTesting(inverted_normal_third_party_bucket_locator);
  base::FilePath inverted_session_only_path_third_party =
      GetFilePathForTesting(inverted_session_only_third_party_bucket_locator);
  base::FilePath inverted_session_only_subdomain_path_third_party =
      GetFilePathForTesting(
          inverted_session_only_subdomain_third_party_bucket_locator);
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
  SetUpStorageKeysForSessionOnlyTests();

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

TEST_P(IndexedDBTest, Bug464999826) {
  BucketInfo bucket_info = GetOrCreateBucket(GetTestStorageKey());

  quota_manager_->HoldBackResults();

  base::FilePath db_directory =
      GetFilePathForTesting(bucket_info.ToBucketLocator());
  ASSERT_TRUE(base::CreateDirectory(db_directory));
  context()->ForceInitializeFromFilesForTesting(base::DoNothing());

  scoped_refptr<base::SequencedTaskRunner> idb_task_runner =
      context_->idb_task_runner();
  IndexedDBContextImpl::Shutdown(std::move(context_));
  base::RunLoop destruction_loop;
  idb_task_runner->PostTask(FROM_HERE, destruction_loop.QuitClosure());
  destruction_loop.Run();

  quota_manager_->ReleaseResults();
}

// Verifies that the IDB connection is force closed and the directory is deleted
// when the bucket is deleted.
TEST_P(IndexedDBTestWithBucketType, ForceCloseOpenDatabasesOnDelete) {
  base::HistogramTester histograms;
  BucketInfo bucket_info;
  VerifyForcedClosedCalled(
      base::BindOnce(base::IgnoreResult(&IndexedDBTest::DeleteBucket),
                     base::Unretained(this), &bucket_info),
      &bucket_info);
  // Additionally, the directory should be deleted.
  base::FilePath test_path =
      GetFilePathForTesting(bucket_info.ToBucketLocator());
  EXPECT_FALSE(base::DirectoryExists(test_path));
  histograms.ExpectTotalCount(
      "IndexedDB.BackendDuration.CloseBackingStore.OnDisk", 1);
  histograms.ExpectUniqueSample("IndexedDB.DeleteBucketDataSuccess.OnDisk",
                                true, 1);
}

// Verifies that the IDB connection is force closed when the backing store has
// an error.
TEST_P(IndexedDBTest, ForceCloseOpenDatabasesOnDatabaseError) {
  BucketInfo bucket_info;
  VerifyForcedClosedCalled(
      base::BindOnce(
          [](IndexedDBTest* test, BucketInfo* bucket_info) {
            BucketContext* bucket = test->GetBucketContext(bucket_info->id);
            const auto& dbs = bucket->GetDatabasesForTesting();
            ASSERT_EQ(1U, dbs.size());
            for (auto& [name, db] : dbs) {
              bucket->OnDatabaseError(
                  db.get(), Status::InvalidArgument("operation not supported"),
                  std::string());
            }
          },
          this, &bucket_info),
      &bucket_info);
}

// Verifies that the IDB connection is force closed when the database is deleted
// via the mojo API.
TEST_P(IndexedDBTest, ForceCloseOpenDatabasesOnDeleteDatabase) {
  BucketInfo bucket_info;
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

// Regression test for https://crbug.com/446722008
TEST_P(IndexedDBTest, AvoidCrashAfterForceCloseDbAndThenOpen) {
  BucketInfo bucket_info = GetOrCreateBucket(GetTestStorageKey());
  BucketLocator bucket_locator = bucket_info.ToBucketLocator();
  mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
      checker_remote;
  BindFactory(std::move(checker_remote),
              factory_remote_.BindNewPipeAndPassReceiver(), bucket_info);

  // Open a database.
  base::RunLoop run_loop_for_first_open;
  MockMojoDatabaseCallbacks database_callbacks;
  EXPECT_CALL(database_callbacks, ForcedClose())
      .WillOnce(RunClosure(run_loop_for_first_open.QuitClosure()));
  MockMojoFactoryClient client;
  mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_database;
  EXPECT_CALL(client, MockedUpgradeNeeded)
      .WillOnce(MoveArgPointee<0>(&pending_database));
  mojo::AssociatedRemote<blink::mojom::IDBTransaction> transaction_remote;
  factory_remote_->Open(client.CreateInterfacePtrAndBind(),
                        database_callbacks.CreateInterfacePtrAndBind(),
                        u"opendb", blink::IndexedDBDatabaseMetadata::NO_VERSION,
                        transaction_remote.BindNewEndpointAndPassReceiver(),
                        /*host_transaction_id=*/0, /*priority=*/0);

  // Delete with force_close = true.
  MockMojoFactoryClient delete_client;
  factory_remote_->DeleteDatabase(delete_client.CreateInterfacePtrAndBind(),
                                  u"opendb",
                                  /*force_close=*/true);

  // Open the database again, without waiting for any of the previous steps to
  // finish. The timing of this is very particular, which is why this test does
  // not use `VerifyForcedClosedCalled()`. The second open succeeds because the
  // `DeleteDatabase` call synchronously destroyed the DB.
  MockMojoFactoryClient client2;
  MockMojoDatabaseCallbacks database_callbacks2;
  base::RunLoop run_loop_for_second_open;
  EXPECT_CALL(client2, MockedUpgradeNeeded)
      .WillOnce(RunClosure(run_loop_for_second_open.QuitClosure()));
  mojo::AssociatedRemote<blink::mojom::IDBTransaction> transaction_remote2;
  factory_remote_->Open(client2.CreateInterfacePtrAndBind(),
                        database_callbacks2.CreateInterfacePtrAndBind(),
                        u"opendb", blink::IndexedDBDatabaseMetadata::NO_VERSION,
                        transaction_remote2.BindNewEndpointAndPassReceiver(),
                        /*host_transaction_id=*/42, /*priority=*/0);

  // Block until expectations are satisfied.
  run_loop_for_first_open.Run();
  run_loop_for_second_open.Run();
}

TEST_P(IndexedDBTest, BasicFactoryCreationAndTearDown) {
  const blink::StorageKey storage_key_1 =
      blink::StorageKey::CreateFromStringForTesting("http://localhost:81");
  BucketInfo bucket_1 = GetOrCreateBucket(
      storage::BucketInitParams::ForDefaultBucket(storage_key_1));
  BucketLocator bucket_locator_1 = bucket_1.ToBucketLocator();
  base::FilePath file_1 =
      GetFilePathForTesting(bucket_locator_1).AppendASCII("1.json");
  ASSERT_TRUE(CreateDirectory(file_1.DirName()));
  ASSERT_TRUE(base::WriteFile(file_1, std::string(10, 'a')));

  const blink::StorageKey storage_key_2 =
      blink::StorageKey::CreateFromStringForTesting("http://localhost:82");
  BucketInfo bucket_2 = GetOrCreateBucket(
      storage::BucketInitParams::ForDefaultBucket(storage_key_2));
  BucketLocator bucket_locator_2 = bucket_2.ToBucketLocator();
  base::FilePath file_2 =
      GetFilePathForTesting(bucket_locator_2).AppendASCII("2.json");
  ASSERT_TRUE(CreateDirectory(file_2.DirName()));
  ASSERT_TRUE(base::WriteFile(file_2, std::string(100, 'a')));

  const blink::StorageKey storage_key_3 =
      blink::StorageKey::CreateFromStringForTesting("http://localhost2:82");
  BucketInfo bucket_3 = GetOrCreateBucket(
      storage::BucketInitParams::ForDefaultBucket(storage_key_3));
  BucketLocator bucket_locator_3 = bucket_3.ToBucketLocator();
  base::FilePath file_3 =
      GetFilePathForTesting(bucket_locator_3).AppendASCII("3.json");
  ASSERT_TRUE(CreateDirectory(file_3.DirName()));
  ASSERT_TRUE(base::WriteFile(file_3, std::string(1000, 'a')));

  const blink::StorageKey storage_key_4 = blink::StorageKey::Create(
      storage_key_1.origin(), net::SchemefulSite(storage_key_3.origin()),
      blink::mojom::AncestorChainBit::kCrossSite);
  BucketInfo bucket_4 = GetOrCreateBucket(
      storage::BucketInitParams::ForDefaultBucket(storage_key_4));
  BucketLocator bucket_locator_4 = bucket_4.ToBucketLocator();
  base::FilePath file_4 =
      GetFilePathForTesting(bucket_locator_4).AppendASCII("4.json");
  ASSERT_TRUE(CreateDirectory(file_4.DirName()));
  ASSERT_TRUE(base::WriteFile(file_4, std::string(10000, 'a')));

  const blink::StorageKey storage_key_5 = storage_key_1;
  storage::BucketInitParams params(storage_key_5, "inbox");
  BucketInfo bucket_5 = GetOrCreateBucket(params);
  BucketLocator bucket_locator_5 = bucket_5.ToBucketLocator();
  base::FilePath file_5 =
      GetFilePathForTesting(bucket_locator_5).AppendASCII("5.json");
  ASSERT_TRUE(CreateDirectory(file_5.DirName()));
  ASSERT_TRUE(base::WriteFile(file_5, std::string(20000, 'a')));
  EXPECT_NE(file_5.DirName(), file_1.DirName());

  InitBucketContext(bucket_1);
  InitBucketContext(bucket_2);
  InitBucketContext(bucket_3);
  InitBucketContext(bucket_4);
  InitBucketContext(bucket_5);

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
    EXPECT_EQ(5ul, context_->GetOpenBucketCountForTesting());
  } else {
    EXPECT_EQ(4ul, context_->GetOpenBucketCountForTesting());
  }
}

TEST_P(IndexedDBTest, CloseSequenceStarts) {
  base::WeakPtr<BucketContext> bucket_context = InitBucketContext();
  const BucketLocator bucket_locator = bucket_context->bucket_locator();
  NudgeBackingStoreCloseLogic(bucket_context.get());

  VerifyBucketContext(bucket_locator, /*expected_context_exists=*/true,
                      /*expected_backing_store_exists=*/true);
  EXPECT_TRUE(bucket_context->IsClosing());

  base::RunLoop run_loop;
  context_->ForceClose(bucket_locator.id, run_loop.QuitClosure());
  run_loop.Run();
  VerifyBucketContext(bucket_locator,
                      /*expected_context_exists=*/false);
  EXPECT_FALSE(bucket_context);
}

// Similar to the above, but installs a receiver which prevents the bucket
// context from being destroyed.
TEST_P(IndexedDBTest, CloseWithReceiversActive) {
  // Create bucket context.
  base::WeakPtr<BucketContext> bucket_context = InitBucketContext();
  const BucketLocator bucket_locator = bucket_context->bucket_locator();
  // Connect an IDBFactory mojo client.
  mojo::Remote<blink::mojom::IDBFactory> factory_remote;
  mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
      checker_remote;
  bucket_context->AddReceiver(storage::BucketClientInfo{},
                              std::move(checker_remote),
                              factory_remote.BindNewPipeAndPassReceiver());

  // The bucket context and the backing store should exist.
  VerifyBucketContext(bucket_locator, /*expected_context_exists=*/true,
                      /*expected_backing_store_exists=*/true);

  // Something triggers starting the close sequence and the grace period
  // elapses.
  NudgeBackingStoreCloseLogic(bucket_context.get());
  task_environment_.FastForwardBy(
      BucketContext::GetBackingStoreGracePeriodForTesting());

  // This destroys the backing store, but the bucket context itself still
  // exists...
  VerifyBucketContext(bucket_locator, /*expected_context_exists=*/true,
                      /*expected_backing_store_exists=*/false);

  // ...until the last mojo client is disconnected.
  factory_remote.reset();
  task_environment_.RunUntilIdle();

  VerifyBucketContext(bucket_locator, /*expected_context_exists=*/false);
  EXPECT_FALSE(bucket_context);
}

// Similar to the above, but reverses the order of receiver disconnection and
// backing store destruction.
TEST_P(IndexedDBTest, CloseWithReceiversInactive) {
  base::WeakPtr<BucketContext> bucket_context = InitBucketContext();
  const BucketLocator bucket_locator = bucket_context->bucket_locator();
  // Connect an IDBFactory mojo client.
  mojo::Remote<blink::mojom::IDBFactory> factory_remote;
  mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
      checker_remote;
  bucket_context->AddReceiver(storage::BucketClientInfo{},
                              std::move(checker_remote),
                              factory_remote.BindNewPipeAndPassReceiver());

  // The bucket context and the backing store should exist.
  VerifyBucketContext(bucket_locator, /*expected_context_exists=*/true,
                      /*expected_backing_store_exists=*/true);

  // The last mojo client is disconnected.
  factory_remote.reset();
  task_environment_.RunUntilIdle();

  // The bucket context and the backing store should still exist.
  VerifyBucketContext(bucket_locator, /*expected_context_exists=*/true,
                      /*expected_backing_store_exists=*/true);

  // Something triggers backing store shutdown and the grace period elapses.
  NudgeBackingStoreCloseLogic(bucket_context.get());
  task_environment_.FastForwardBy(
      BucketContext::GetBackingStoreGracePeriodForTesting());

  VerifyBucketContext(bucket_locator, /*expected_context_exists=*/false);
  EXPECT_FALSE(bucket_context);
}

TEST_P(IndexedDBTest, PreCloseTasksStart) {
  if (IsSqliteBackingStoreEnabled()) {
    // SQLite doesn't have any pre-close tasks, although it may in the future,
    // such as vacuuming. For now this test is not relevant.
    GTEST_SKIP();
  }

  {
    // Open a connection & immediately release it to cause the closing sequence
    // to start.
    base::WeakPtr<BucketContext> bucket_context = InitBucketContext();
    const BucketLocator bucket_locator = bucket_context->bucket_locator();

    mojo::Remote<blink::mojom::IDBFactory> factory_remote;
    mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
        checker_remote;
    BindFactory(std::move(checker_remote),
                factory_remote.BindNewPipeAndPassReceiver(),
                bucket_context->bucket_info());

    NudgeBackingStoreCloseLogic(bucket_context.get());

    VerifyBucketContext(bucket_locator, /*expected_context_exists=*/true,
                        /*expected_backing_store_exists=*/true);
    EXPECT_TRUE(GetBucketContext(bucket_locator.id)->IsClosing());

    EXPECT_EQ(BucketContext::ClosingState::kPreCloseGracePeriod,
              GetBucketContext(bucket_locator.id)->closing_stage());

    task_environment_.FastForwardBy(
        BucketContext::GetBackingStoreGracePeriodForTesting());

    // The factory should be closed, as the pre close tasks are delayed.
    VerifyBucketContext(bucket_locator, /*expected_context_exists=*/true,
                        /*expected_backing_store_exists=*/false);
    EXPECT_FALSE(bucket_context);
  }

  // Move the clock to run the tasks in the next close sequence.
  // NOTE: The constants rate-limiting sweeps and compaction are currently the
  // same. This test may need to be restructured if these values diverge.
  task_environment_.FastForwardBy(kMaxGlobalSweepDelay);

  // Note that once the closing sequence has started, as is the case in the next
  // block, and if the test does anything to spin the message loop, such as
  // using a future to get the bucket context, the bucket context will be
  // destroyed. For that reason, the bucket context pointer is stored in
  // `bucket_context`.
  base::WeakPtr<BucketContext> bucket_context;

  {
    bucket_context = InitBucketContext();
    NudgeBackingStoreCloseLogic(bucket_context.get());

    // Manually execute the timer so that the PreCloseTaskList task doesn't also
    // run.
    bucket_context->close_timer()->FireNow();

    // The pre-close tasks should be running now.
    EXPECT_EQ(BucketContext::ClosingState::kRunningPreCloseTasks,
              bucket_context->closing_stage());
  }

  {
    // Stop sweep by simulating a request.
    auto scoper = bucket_context->ScopedHandlingRequest();
    BucketLocator bucket_locator = bucket_context->bucket_locator();
    EXPECT_NE(BucketContext::ClosingState::kRunningPreCloseTasks,
              bucket_context->closing_stage());

    // Move clock forward to trigger next sweep, but storage key has longer
    // sweep minimum, so no tasks should execute.
    task_environment_.FastForwardBy(kMaxGlobalSweepDelay);

    scoper.RunAndReset();
    EXPECT_EQ(BucketContext::ClosingState::kPreCloseGracePeriod,
              bucket_context->closing_stage());

    // Manually execute the timer so that the PreCloseTaskList task doesn't also
    // run.
    bucket_context->close_timer()->FireNow();
    ASSERT_TRUE(context_->BucketContextExists(bucket_locator));
    EXPECT_TRUE(!!bucket_context->backing_store());

    VerifyBucketContextWaitIfNeeded(bucket_locator,
                                    /*expected_context_exists=*/false);
  }

  {
    // Finally, move the clock forward so the storage key should allow a sweep.
    task_environment_.FastForwardBy(kMaxBucketSweepDelay);
    bucket_context = InitBucketContext();
    BucketLocator bucket_locator = bucket_context->bucket_locator();
    auto scoper = bucket_context->ScopedHandlingRequest();
    scoper.RunAndReset();
    bucket_context->close_timer()->FireNow();

    ASSERT_TRUE(context_->BucketContextExists(bucket_locator));
    EXPECT_EQ(BucketContext::ClosingState::kRunningPreCloseTasks,
              bucket_context->closing_stage());
  }
}

TEST_P(IndexedDBTest, InMemoryFactoriesStay) {
  SetUpInMemoryContext();

  base::WeakPtr<BucketContext> bucket_context = InitBucketContext();
  BucketLocator bucket_locator = bucket_context->bucket_locator();

  // In-memory backing stores can't shut down before the entire profile is
  // destroyed.
  EXPECT_TRUE(bucket_context->in_memory());
  NudgeBackingStoreCloseLogic(bucket_context.get());
  EXPECT_TRUE(context_->BucketContextExists(bucket_locator));
  EXPECT_FALSE(bucket_context->IsClosing());

  // Verify the in-memory factory sticks around on ForceClose.
  {
    base::RunLoop run_loop;
    context_->ForceClose(bucket_locator.id, run_loop.QuitClosure());
    run_loop.Run();
    RunPostedTasks(bucket_locator);
    VerifyBucketContext(bucket_locator, /*expected_context_exists=*/true,
                        /*expected_backing_store_exists=*/true);
  }

  // Verify the in-memory factory does NOT stick around on DeleteBucketData.
  {
    base::test::TestFuture<blink::mojom::QuotaStatusCode> result;
    context_->DeleteBucketData(bucket_locator, result.GetCallback());
    EXPECT_EQ(result.Get(), blink::mojom::QuotaStatusCode::kOk);
    VerifyBucketContext(bucket_locator, /*expected_context_exists=*/false);
  }
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
  BucketInfo bucket_info = GetOrCreateBucket(
      storage::BucketInitParams::ForDefaultBucket(too_long_storage_key));
  BucketLocator bucket_locator = bucket_info.ToBucketLocator();

  base::WeakPtr<BucketContext> bucket_context =
      InitBucketContext(bucket_info, /*create_backing_store=*/false);
  Status s;
  std::tie(s, std::ignore, std::ignore) = bucket_context->InitBackingStore(
      /*create_if_missing=*/true);

  EXPECT_TRUE(s.IsIOError());
}

TEST_P(IndexedDBTest, FactoryForceClose) {
  base::WeakPtr<BucketContext> bucket_context = InitBucketContext();
  BucketLocator bucket_locator = bucket_context->bucket_locator();

  bucket_context->ForceClose(/*doom=*/false);
  // Weak pointer is immediately invalidated.
  EXPECT_FALSE(bucket_context);

  // The destruction task is immediately *posted* to the owning context's
  // sequence, so it doesn't run synchronously.
  ASSERT_TRUE(context_->BucketContextExists(bucket_locator));
  // A single pump of the owning context's sequence should run the destruction
  // task. If the destruction task was posted after an async delay, then this
  // would not be enough to pass the next check.
  RunPostedTasks();
  VerifyBucketContext(bucket_locator,
                      /*expected_context_exists=*/false);
}

// This test aims to verify the behavior of
// BucketContext::Delegate::on_receiver_bounced.
TEST_P(IndexedDBTest, CloseThenAddReceiver) {
  BucketInfo bucket_info = GetOrCreateBucket(GetTestStorageKey());
  BucketLocator bucket_locator = bucket_info.ToBucketLocator();

  // Trigger the bucket context to be created.
  mojo::Remote<blink::mojom::IDBFactory> factory_remote1;
  BindFactory(
      mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>(),
      factory_remote1.BindNewPipeAndPassReceiver(), bucket_info);

  ASSERT_TRUE(context()->BucketContextExists(bucket_locator));

  // Remove the factory binding, and since there is no backing store yet, this
  // should trigger the destruction of the bucket context.
  base::RunLoop loop;
  factory_remote1.reset();
  // We unfortunately can't flush the disconnected pipe, but this works as well.
  context()->FlushBucketSequenceForTesting(bucket_locator, loop.QuitClosure());
  loop.Run();

  // The bucket context still exists for now because shutdown is not
  // synchronous.
  ASSERT_TRUE(context()->BucketContextExists(bucket_locator));

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
  EXPECT_TRUE(factory_remote2.is_connected());

  // It would be nice to re-verify that the new BucketContext is not the same
  // as the old one, but there's no good way to identify them through mojo and
  // no guarantee their memory addresses are different either.
}

// Tests that the backing store is closed when the connection is closed during
// upgrade.
TEST_P(IndexedDBTest, ConnectionCloseDuringUpgrade) {
  BucketInfo bucket_info = GetOrCreateBucket(GetTestStorageKey());
  BucketLocator bucket_locator = bucket_info.ToBucketLocator();

  // Bind the IDBFactory.
  mojo::Remote<blink::mojom::IDBFactory> factory_remote;
  mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
      checker_remote;
  BindFactory(std::move(checker_remote),
              factory_remote.BindNewPipeAndPassReceiver(), bucket_info);

  // Now create a database and thus the backing store.
  MojoConnectionHelper mojo_helper;
  mojo_helper.Open(factory_remote.get());

  ASSERT_TRUE(context_->BucketContextExists(bucket_locator));
  EXPECT_FALSE(GetBucketContext(bucket_locator.id)->IsClosing());

  // Drop the connection.
  mojo_helper.database.reset();
  factory_remote.FlushForTesting();
  EXPECT_TRUE(GetBucketContext(bucket_locator.id)->IsClosing());
}

// Verifies that opening an existing database that is not currently open in the
// backing store works as expected.
TEST_P(IndexedDBTestWithBucketType, OpenExistingDatabase) {
  BucketInfo bucket_info = GetOrCreateBucket(GetTestStorageKey());
  BucketLocator bucket_locator = bucket_info.ToBucketLocator();

  // Bind the IDBFactory.
  mojo::Remote<blink::mojom::IDBFactory> factory_remote;
  mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
      checker_remote;
  BindFactory(std::move(checker_remote),
              factory_remote.BindNewPipeAndPassReceiver(), bucket_info);

  // Create a database with a valid version so that it gets persisted.
  {
    base::HistogramTester histogram_tester;
    CreateDatabase(factory_remote, kDatabaseName, /*transaction_id=*/1);

    histogram_tester.ExpectUniqueSample(
        "IndexedDB.BackingStore.CreateIfMissing.OnDisk",
        0 /*Status::Type::kOk*/, 1);
    histogram_tester.ExpectUniqueSample(
        "IndexedDB.BackingStore.CreateOrOpenDatabase.OnDisk",
        0 /*Status::Type::kOk*/, 1);
    histogram_tester.ExpectTotalCount(
        "IndexedDB.BackendDuration.CreateDatabase.OnDisk", 1);
    histogram_tester.ExpectBucketCount(
        "IndexedDB.DatabaseConnectionOpenResult.OnDisk",
        DatabaseConnectionOpenResult::kReceivedRequest, 1);
    histogram_tester.ExpectBucketCount(
        "IndexedDB.DatabaseConnectionOpenResult.OnDisk",
        DatabaseConnectionOpenResult::kSuccessUpgradeNeeded, 1);
  }

  FastForwardToCloseStore();
  VerifyBucketContext(bucket_locator, /*expected_context_exists=*/true,
                      /*expected_backing_store_exists=*/false);

  // Open the database again, which should require reopening the backing store.
  {
    base::HistogramTester histogram_tester;
    OpenDatabase(factory_remote, kDatabaseName, /*transaction_id=*/2);

    histogram_tester.ExpectTotalCount(
        "IndexedDB.BackingStore.CreateIfMissing.OnDisk", 1);
    histogram_tester.ExpectUniqueSample(
        "IndexedDB.BackingStore.CreateOrOpenDatabase.OnDisk",
        0 /*Status::Type::kOk*/, 1);
    histogram_tester.ExpectTotalCount(
        "IndexedDB.BackendDuration.OpenDatabase.OnDisk", 1);
    histogram_tester.ExpectBucketCount(
        "IndexedDB.DatabaseConnectionOpenResult.OnDisk",
        DatabaseConnectionOpenResult::kReceivedRequest, 1);
    histogram_tester.ExpectBucketCount(
        "IndexedDB.DatabaseConnectionOpenResult.OnDisk",
        DatabaseConnectionOpenResult::kSuccessDirectOpen, 1);
  }
}

TEST_P(IndexedDBTest, DeleteDatabase) {
  BucketInfo bucket_info = GetOrCreateBucket(GetTestStorageKey());
  BucketLocator bucket_locator = bucket_info.ToBucketLocator();

  // Bind the IDBFactory.
  mojo::Remote<blink::mojom::IDBFactory> factory_remote;
  mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
      checker_remote;
  BindFactory(std::move(checker_remote),
              factory_remote.BindNewPipeAndPassReceiver(), bucket_info);

  // Don't create a backing store if one doesn't exist.
  {
    // Delete db.
    MockMojoFactoryClient client;
    MockMojoDatabaseCallbacks database_callbacks;
    base::RunLoop run_loop;
    EXPECT_CALL(client, DeleteSuccess(0))
        .WillOnce(RunClosure(run_loop.QuitClosure()));
    mojo::AssociatedRemote<blink::mojom::IDBTransaction> transaction_remote;
    factory_remote->DeleteDatabase(client.CreateInterfacePtrAndBind(),
                                   kDatabaseName,
                                   /*force_close=*/false);
    run_loop.Run();

    // Backing store shouldn't exist.
    ASSERT_TRUE(context_->BucketContextExists(bucket_locator));
    EXPECT_FALSE(GetBucketContext(bucket_locator.id)->backing_store());
  }

  // Now create a database and thus the backing store.
  CreateDatabase(factory_remote, kDatabaseName, /*transaction_id=*/1);

  // Delete the database now that the backing store actually exists.
  {
    MockMojoFactoryClient client;
    base::RunLoop run_loop;
    EXPECT_CALL(client, DeleteSuccess(1))
        .WillOnce(RunClosure(run_loop.QuitClosure()));
    factory_remote->DeleteDatabase(client.CreateInterfacePtrAndBind(),
                                   kDatabaseName,
                                   /*force_close=*/false);
    run_loop.Run();

    // Since there are no more references the factory should be closing.
    ASSERT_TRUE(context_->BucketContextExists(bucket_locator));
    EXPECT_TRUE(GetBucketContext(bucket_locator.id)->IsClosing());
  }
}

// Verifies that deleting an existing database that is not currently open in the
// backing store works as expected.
TEST_P(IndexedDBTestWithBucketType, DeleteDatabase_Cold) {
  BucketInfo bucket_info = GetOrCreateBucket(GetTestStorageKey());
  BucketLocator bucket_locator = bucket_info.ToBucketLocator();

  // Bind the IDBFactory.
  mojo::Remote<blink::mojom::IDBFactory> factory_remote;
  mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
      checker_remote;
  BindFactory(std::move(checker_remote),
              factory_remote.BindNewPipeAndPassReceiver(), bucket_info);

  // Create a database with a valid version so that it gets persisted.
  {
    base::HistogramTester histogram_tester;
    CreateDatabase(factory_remote, kDatabaseName, /*transaction_id=*/1);

    histogram_tester.ExpectUniqueSample(
        "IndexedDB.BackingStore.CreateIfMissing.OnDisk",
        0 /*Status::Type::kOk*/, 1);
  }

  FastForwardToCloseStore();
  VerifyBucketContext(bucket_locator, /*expected_context_exists=*/true,
                      /*expected_backing_store_exists=*/false);

  // Delete the database now, which should require reopening the backing store
  // (and the database).
  {
    base::HistogramTester histogram_tester;
    MockMojoFactoryClient client;
    MockMojoDatabaseCallbacks database_callbacks;
    base::RunLoop run_loop;
    EXPECT_CALL(client, DeleteSuccess(1))
        .WillOnce(RunClosure(run_loop.QuitClosure()));
    mojo::AssociatedRemote<blink::mojom::IDBTransaction> transaction_remote;
    factory_remote->DeleteDatabase(client.CreateInterfacePtrAndBind(),
                                   kDatabaseName,
                                   /*force_close=*/false);
    run_loop.Run();

    // The backing store itself should not be created, just opened.
    histogram_tester.ExpectTotalCount(
        "IndexedDB.BackingStore.CreateIfMissing.OnDisk", 0);
    histogram_tester.ExpectUniqueSample(
        "IndexedDB.BackingStore.CreateOrOpenDatabase.OnDisk",
        0 /*Status::Type::kOk*/, 1);
    histogram_tester.ExpectUniqueSample(
        "IndexedDB.BackingStore.DeleteDatabase.OnDisk", 0 /*Status::Type::kOk*/,
        1);
    histogram_tester.ExpectTotalCount(
        "IndexedDB.BackendDuration.DeleteDatabase.OnDisk", 1);
  }
}

// Verifies the behavior when several delete requests for the same database are
// queued together.
TEST_P(IndexedDBTest, DeleteDatabase_DuplicateRequests) {
  BucketInfo bucket_info = GetOrCreateBucket(GetTestStorageKey());

  // Bind the IDBFactory.
  mojo::Remote<blink::mojom::IDBFactory> factory_remote;
  mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
      checker_remote;
  BindFactory(std::move(checker_remote),
              factory_remote.BindNewPipeAndPassReceiver(), bucket_info);

  // Open (create) a database and keep the connection alive.
  mojo::AssociatedRemote<blink::mojom::IDBDatabase> connection =
      CreateDatabase(factory_remote, kDatabaseName, /*transaction_id=*/1);

  // Issue two delete requests in succession. The first one should really delete
  // the database, while the second should find the database non-existent.
  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;

  MockMojoFactoryClient first_client;
  EXPECT_CALL(first_client, DeleteSuccess(1));
  MockMojoFactoryClient second_client;
  EXPECT_CALL(second_client, DeleteSuccess(0))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

  factory_remote->DeleteDatabase(first_client.CreateInterfacePtrAndBind(),
                                 kDatabaseName,
                                 /*force_close=*/false);
  factory_remote->DeleteDatabase(second_client.CreateInterfacePtrAndBind(),
                                 kDatabaseName,
                                 /*force_close=*/false);
  connection.reset();
  run_loop.Run();

  // The first delete request should find the database already open, and the
  // second one should not attempt to create it.
  histogram_tester.ExpectTotalCount(
      "IndexedDB.BackingStore.CreateOrOpenDatabase.OnDisk", 0);
  // Only the first request should call into the backing store.
  histogram_tester.ExpectUniqueSample(
      "IndexedDB.BackingStore.DeleteDatabase.OnDisk", 0 /*Status::Type::kOk*/,
      1);
  histogram_tester.ExpectTotalCount(
      "IndexedDB.BackendDuration.DeleteDatabase.OnDisk", 1);
}

TEST_P(IndexedDBTestWithBucketType, GetDatabaseNames) {
  base::HistogramTester histogram_tester;
  BucketInfo bucket_info = GetOrCreateBucket(GetTestStorageKey());
  BucketLocator bucket_locator = bucket_info.ToBucketLocator();

  // Bind the IDBFactory.
  mojo::Remote<blink::mojom::IDBFactory> factory_remote;
  mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
      checker_remote;
  BindFactory(std::move(checker_remote),
              factory_remote.BindNewPipeAndPassReceiver(), bucket_info);

  // Don't create a backing store if one doesn't exist.
  {
    base::test::TestFuture<std::vector<blink::mojom::IDBNameAndVersionPtr>,
                           blink::mojom::IDBErrorPtr>
        info_future;
    factory_remote->GetDatabaseInfo(info_future.GetCallback());
    ASSERT_TRUE(info_future.Wait());
    EXPECT_FALSE(GetBucketContext(bucket_locator.id)->backing_store());
    // The duration histogram should not be recorded since this was a trivial
    // request (the backing store was not involved).
    histogram_tester.ExpectTotalCount(
        "IndexedDB.BackendDuration.GetDatabaseInfo.OnDisk", 0);
  }

  // Now create a database and thus the backing store. It's necessary to hang
  // onto the database connection or the connection will shut itself down and
  // the backing store will close on its own.
  mojo::AssociatedRemote<blink::mojom::IDBDatabase> database_remote =
      CreateDatabase(factory_remote, kDatabaseName, /*transaction_id=*/1);

  // GetDatabaseInfo is called, it wasn't the trigger to create the backing
  // store and there's still an attached connection. Thus calling this shouldn't
  // initiate backing store shutdown.
  {
    base::test::TestFuture<std::vector<blink::mojom::IDBNameAndVersionPtr>,
                           blink::mojom::IDBErrorPtr>
        info_future;
    factory_remote->GetDatabaseInfo(info_future.GetCallback());
    ASSERT_TRUE(info_future.Wait());

    ASSERT_TRUE(context_->BucketContextExists(bucket_locator));
    EXPECT_FALSE(GetBucketContext(bucket_locator.id)->IsClosing());

    histogram_tester.ExpectUniqueSample(
        "IndexedDB.BackingStore.GetDatabaseNamesAndVersions.OnDisk",
        0 /*Status::Type::kOk*/, 1);
    histogram_tester.ExpectTotalCount(
        "IndexedDB.BackendDuration.GetDatabaseInfo.OnDisk", 1);
  }

  // Let the backing store close by disconnecting the connection and waiting for
  // the grace period to elapse.
  database_remote.reset();
  factory_remote.FlushForTesting();
  FastForwardToCloseStore();
  EXPECT_FALSE(GetBucketContext(bucket_locator.id)->backing_store());

  // GetDatabaseInfo opens the backing store, so it *should* close it.
  {
    base::test::TestFuture<std::vector<blink::mojom::IDBNameAndVersionPtr>,
                           blink::mojom::IDBErrorPtr>
        info_future;
    factory_remote->GetDatabaseInfo(info_future.GetCallback());
    EXPECT_TRUE(info_future.Wait());
    EXPECT_TRUE(GetBucketContext(bucket_locator.id)->IsClosing());
  }
}

// Regression test for crbug.com/376461709
TEST_P(IndexedDBTest, UpdatePriorityAfterForceClose) {
  BucketInfo bucket_info = GetOrCreateBucket(GetTestStorageKey());

  // Bind the IDBFactory.
  mojo::Remote<blink::mojom::IDBFactory> factory_remote;
  mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
      checker_remote;
  BindFactory(std::move(checker_remote),
              factory_remote.BindNewPipeAndPassReceiver(), bucket_info);

  // Bind a connection/database.
  MojoConnectionHelper mojo_helper;
  mojo_helper.OpenAndExpectUpgradeNeeded(factory_remote.get());
  // Simulate force closing the context while `UpdatePriority` is in flight.
  context_->ForceClose(bucket_info.id, base::DoNothing());
  // Call this second in the unit test context to simulate losing the race.
  mojo_helper.database->UpdatePriority(1);
  mojo_helper.database.FlushForTesting();

  // Not crashing indicates success.
}

TEST_P(IndexedDBTest, TransactionHistograms) {
  constexpr int64_t kObjectStoreId = 1;
  int64_t transaction_id = 0;

  BucketInfo bucket_info = GetOrCreateBucket(GetTestStorageKey());

  // Bind the IDBFactory.
  mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
      checker_remote;
  BindFactory(std::move(checker_remote),
              factory_remote_.BindNewPipeAndPassReceiver(), bucket_info);

  // Create a database with a valid version so that a version change transaction
  // is created.
  MojoConnectionHelper mojo_helper(kDatabaseName, /*version=*/1,
                                   ++transaction_id);
  {
    base::HistogramTester histogram_tester;
    mojo_helper.OpenAndExpectUpgradeNeeded(factory_remote_.get());

    // Create an object store and commit the version change transaction.
    mojo_helper.vc_txn->CreateObjectStore(kObjectStoreId, u"store",
                                          blink::IndexedDBKeyPath(),
                                          /*auto_increment=*/true);
    mojo_helper.vc_txn->Commit(0);

    // Wait for the transaction to complete.
    base::RunLoop loop;
    EXPECT_CALL(mojo_helper.connection_callbacks,
                Complete(mojo_helper.upgrade_txn_id))
        .WillOnce(base::test::RunClosure(loop.QuitClosure()));
    loop.Run();
    EXPECT_CALL(mojo_helper.open_callbacks, MockedOpenSuccess);

    histogram_tester.ExpectUniqueSample(
        "IndexedDB.BackingStore.BeginTransaction.OnDisk",
        0 /*Status::Type::kOk*/, 1);
    histogram_tester.ExpectUniqueSample(
        "IndexedDB.BackingStore.ChangeDatabaseVersion.OnDisk",
        0 /*Status::Type::kOk*/, 1);
    histogram_tester.ExpectUniqueSample(
        "IndexedDB.BackingStore.CreateObjectStore.OnDisk",
        0 /*Status::Type::kOk*/, 1);
    histogram_tester.ExpectUniqueSample(
        "IndexedDB.BackingStore.CommitPhaseOne.OnDisk", 0 /*Status::Type::kOk*/,
        1);
    histogram_tester.ExpectUniqueSample(
        "IndexedDB.BackingStore.CommitPhaseTwo.OnDisk", 0 /*Status::Type::kOk*/,
        1);
    histogram_tester.ExpectTotalCount(
        "IndexedDB.BackendDuration.BeginTransaction.OnDisk", 1);
    histogram_tester.ExpectTotalCount(
        "IndexedDB.BackendDuration.ChangeDatabaseVersion.OnDisk", 1);
    histogram_tester.ExpectTotalCount(
        "IndexedDB.BackendDuration.CreateObjectStore.OnDisk", 1);
    histogram_tester.ExpectTotalCount(
        "IndexedDB.BackendDuration.CommitTransaction.OnDisk", 1);
  }

  // Create a transaction and commit it without issuing any request.
  {
    base::HistogramTester histogram_tester;
    mojo::AssociatedRemote<blink::mojom::IDBTransaction> transaction;
    mojo_helper.database->CreateTransaction(
        transaction.BindNewEndpointAndPassReceiver(), ++transaction_id,
        {kObjectStoreId}, blink::mojom::IDBTransactionMode::ReadWrite,
        blink::mojom::IDBTransactionDurability::Relaxed);
    transaction->Commit(0);

    // Wait for the transaction to complete.
    base::RunLoop loop;
    EXPECT_CALL(mojo_helper.connection_callbacks, Complete(transaction_id))
        .WillOnce(base::test::RunClosure(loop.QuitClosure()));
    loop.Run();

    histogram_tester.ExpectUniqueSample(
        "IndexedDB.BackingStore.BeginTransaction.OnDisk",
        0 /*Status::Type::kOk*/, 1);
    // The commit does not propagate to the BackingStore since no requests were
    // issued to the transaction.
    histogram_tester.ExpectTotalCount(
        "IndexedDB.BackingStore.CommitPhaseOne.OnDisk", 0);
    histogram_tester.ExpectTotalCount(
        "IndexedDB.BackingStore.CommitPhaseTwo.OnDisk", 0);
    histogram_tester.ExpectTotalCount(
        "IndexedDB.BackendDuration.BeginTransaction.OnDisk", 1);
    histogram_tester.ExpectTotalCount(
        "IndexedDB.BackendDuration.CommitTransaction.OnDisk", 0);
  }

  // Create another transaction and issue some requests.
  {
    base::HistogramTester histogram_tester;
    mojo::AssociatedRemote<blink::mojom::IDBTransaction> transaction;
    mojo_helper.database->CreateTransaction(
        transaction.BindNewEndpointAndPassReceiver(), ++transaction_id,
        {kObjectStoreId}, blink::mojom::IDBTransactionMode::ReadWrite,
        blink::mojom::IDBTransactionDurability::Relaxed);

    transaction->Put(kObjectStoreId,
                     blink::mojom::IDBValuePtr(blink::mojom::IDBValue::New()),
                     blink::IndexedDBKey(), blink::mojom::IDBPutMode::AddOnly,
                     /*index_keys=*/{},
                     base::BindLambdaForTesting(
                         [&](blink::mojom::IDBTransactionPutResultPtr result) {
                           EXPECT_FALSE(result->is_error_result());
                         }));
    transaction->Commit(0);

    // Wait for the transaction to complete.
    base::RunLoop loop;
    EXPECT_CALL(mojo_helper.connection_callbacks, Complete(transaction_id))
        .WillOnce(base::test::RunClosure(loop.QuitClosure()));
    loop.Run();

    histogram_tester.ExpectUniqueSample(
        "IndexedDB.BackingStore.BeginTransaction.OnDisk",
        0 /*Status::Type::kOk*/, 1);
    histogram_tester.ExpectUniqueSample(
        "IndexedDB.BackingStore.PutRecord.OnDisk", 0 /*Status::Type::kOk*/, 1);
    histogram_tester.ExpectUniqueSample(
        "IndexedDB.BackingStore.CommitPhaseOne.OnDisk", 0 /*Status::Type::kOk*/,
        1);
    histogram_tester.ExpectTotalCount(
        "IndexedDB.BackingStore.WriteBlobs.OnDisk", 0);
    histogram_tester.ExpectUniqueSample(
        "IndexedDB.BackingStore.CommitPhaseTwo.OnDisk", 0 /*Status::Type::kOk*/,
        1);
    histogram_tester.ExpectTotalCount(
        "IndexedDB.BackendDuration.BeginTransaction.OnDisk", 1);
    histogram_tester.ExpectTotalCount(
        "IndexedDB.BackendDuration.PutRecord.OnDisk", 1);
    histogram_tester.ExpectTotalCount(
        "IndexedDB.BackendDuration.CommitTransaction.OnDisk", 1);
  }
}

TEST_P(IndexedDBTest, QuotaErrorOnDbOpenError) {
  base::HistogramTester histograms;
  if (IsSqliteBackingStoreEnabled()) {
    // The mechanism used to induce errors (`MakeFileUnwritable`) doesn't work
    // on Fuchsia.
#if BUILDFLAG(IS_FUCHSIA)
    GTEST_SKIP();
#endif  // BUILDFLAG(IS_FUCHSIA)
  } else {
    leveldb_env::SetDBFactoryForTesting(base::BindRepeating(
        [](const leveldb_env::Options& options, const std::string& name,
           std::unique_ptr<leveldb::DB>* dbptr) {
          return leveldb_env::MakeIOError("foobar", "disk full",
                                          leveldb_env::MethodID::kCreateDir,
                                          base::File::FILE_ERROR_NO_SPACE);
        }));
  }

  // Bind the IDBFactory.
  BucketInfo bucket_info = GetOrCreateBucket(GetTestStorageKey());
  mojo::Remote<blink::mojom::IDBFactory> factory_remote;
  mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
      checker_remote;
  BindFactory(std::move(checker_remote),
              factory_remote.BindNewPipeAndPassReceiver(), bucket_info);

  std::optional<base::FilePermissionRestorer> permission_restorer;
  if (IsSqliteBackingStoreEnabled()) {
    // First create a database successfully so that the directory exists, then
    // make the directory unwritable. This will make future attempts to open or
    // create a file fail.
    CreateDatabase(factory_remote, u"db2", /*transaction_id=*/1);

    base::FilePath data_path =
        GetFilePathForTesting(bucket_info.ToBucketLocator());
    permission_restorer.emplace(data_path);
    ASSERT_TRUE(base::MakeFileUnwritable(data_path))
        << base::File::GetLastFileError();
    histograms.ExpectTotalCount("IndexedDB.SQLite.OpenRetryResult", 0);
  }

  // Expect an error when opening.
  MockMojoFactoryClient client;
  MockMojoDatabaseCallbacks database_callbacks;
  base::RunLoop run_loop;
  EXPECT_CALL(client, Error).WillOnce(RunClosure(run_loop.QuitClosure()));
  mojo::AssociatedRemote<blink::mojom::IDBTransaction> transaction_remote;
  factory_remote->Open(
      client.CreateInterfacePtrAndBind(),
      database_callbacks.CreateInterfacePtrAndBind(), kDatabaseName,
      /*version=*/1, transaction_remote.BindNewEndpointAndPassReceiver(),
      /*transaction_id=*/2, /*priority=*/0);
  run_loop.Run();

  if (IsSqliteBackingStoreEnabled()) {
    histograms.ExpectUniqueSample("IndexedDB.SQLite.OpenRetryResult",
                                  5 /*Status::Type::kDatabaseEngine*/, 1);
  }

  histograms.ExpectBucketCount(
      "IndexedDB.DatabaseConnectionOpenResult.OnDisk",
      IsSqliteBackingStoreEnabled()
          ? DatabaseConnectionOpenResult::kErrorDatabaseOpenFailed
          : DatabaseConnectionOpenResult::kErrorBackingStoreInitFailed,
      1);

  // An error on open results in a write error reported to the quota system.
  ASSERT_EQ(1U, quota_manager_->write_error_tracker().size());
  EXPECT_EQ(GetTestStorageKey(),
            quota_manager_->write_error_tracker().begin()->first);
  EXPECT_EQ(1, quota_manager_->write_error_tracker().begin()->second);

  leveldb_env::SetDBFactoryForTesting({});
}

TEST_P(IndexedDBTest, DatabaseFailedOpen) {
  BucketInfo bucket_info = GetOrCreateBucket(GetTestStorageKey());

  // Bind the IDBFactory.
  mojo::Remote<blink::mojom::IDBFactory> factory_remote;
  mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
      checker_remote;
  BindFactory(std::move(checker_remote),
              factory_remote.BindNewPipeAndPassReceiver(), bucket_info);

  // Create at version 2.
  CreateDatabase(factory_remote, kDatabaseName, /*transaction_id=*/1,
                 blink::mojom::IDBDataLoss::None, /*version=*/2);

  // Open at version < 2, which will fail.
  {
    base::HistogramTester histogram_tester;
    const int64_t db_version = 1;
    base::RunLoop run_loop;
    MockMojoFactoryClient client;
    MockMojoDatabaseCallbacks database_callbacks;
    EXPECT_CALL(client, Error).WillOnce(RunClosure(run_loop.QuitClosure()));
    mojo::AssociatedRemote<blink::mojom::IDBTransaction> transaction_remote;
    factory_remote->Open(client.CreateInterfacePtrAndBind(),
                         database_callbacks.CreateInterfacePtrAndBind(),
                         kDatabaseName, db_version,
                         transaction_remote.BindNewEndpointAndPassReceiver(),
                         /*transaction_id=*/2, /*priority=*/0);
    run_loop.Run();
    BucketContext* bucket_context = GetBucketContext(bucket_info.id);
    ASSERT_TRUE(bucket_context);
    EXPECT_FALSE(
        bucket_context->GetDatabasesForTesting().contains(kDatabaseName));
    histogram_tester.ExpectBucketCount(
        "IndexedDB.DatabaseConnectionOpenResult.OnDisk",
        DatabaseConnectionOpenResult::kErrorVersionTooLow, 1);
  }
}

// Test for `IndexedDBDataFormatVersion`.
TEST_P(IndexedDBTestWithBucketType, DataLoss) {
  BucketInfo bucket_info = GetOrCreateBucket(GetTestStorageKey());
  BucketLocator bucket_locator = bucket_info.ToBucketLocator();
  const std::u16string db_name(u"test_db");

  // Bind the IDBFactory.
  mojo::Remote<blink::mojom::IDBFactory> factory_remote;
  mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
      checker_remote;
  BindFactory(std::move(checker_remote),
              factory_remote.BindNewPipeAndPassReceiver(), bucket_info);

  // Set a data format version and create a new database. No data loss.
  {
    base::AutoReset<IndexedDBDataFormatVersion> override_version(
        &IndexedDBDataFormatVersion::GetMutableCurrentForTesting(),
        IndexedDBDataFormatVersion(3, 4));
    CreateDatabase(factory_remote, db_name, /*transaction_id=*/1,
                   blink::mojom::IDBDataLoss::None);

    // This step is necessary to make sure the backing store is closed so that
    // the second `Open` will initialize it with the new (older) data format
    // version. Without this step, the same `BackingStore` is reused because
    // it's kept around for 2 seconds after the last connection is dropped.
    base::RunLoop run_loop;
    context_->ForceClose(bucket_locator.id, run_loop.QuitClosure());
    run_loop.Run();
  }

  // Set an older data format version and try to reopen said database. Expect
  // total data loss.
  {
    base::HistogramTester histograms;
    base::AutoReset<IndexedDBDataFormatVersion> override_version(
        &IndexedDBDataFormatVersion::GetMutableCurrentForTesting(),
        IndexedDBDataFormatVersion(3, 3));
    CreateDatabase(factory_remote, db_name, /*transaction_id=*/2,
                   blink::mojom::IDBDataLoss::Total);
    if (IsSqliteBackingStoreEnabled()) {
      histograms.ExpectUniqueSample("IndexedDB.SQLite.OpenRetryResult",
                                    0 /*Status::Type::kOk*/, 1);
    }
    histograms.ExpectBucketCount(
        "IndexedDB.DatabaseConnectionOpenResult.OnDisk",
        DatabaseConnectionOpenResult::kSuccessUpgradeNeededWithDataLoss, 1);
  }
}

#if BUILDFLAG(IS_WIN)
TEST_P(IndexedDBTest, FilePathLengthLogging) {
  base::HistogramTester histograms;

  // Open with a normal length origin; success.
  BucketInfo bucket_info = GetOrCreateBucket(GetTestStorageKey());

  {
    mojo::Remote<blink::mojom::IDBFactory> factory_remote;
    mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
        checker_remote;
    BindFactory(std::move(checker_remote),
                factory_remote.BindNewPipeAndPassReceiver(), bucket_info);

    {
      const int64_t db_version = 1;
      MockMojoFactoryClient client;
      MockMojoDatabaseCallbacks database_callbacks;
      base::RunLoop run_loop;
      EXPECT_CALL(client, MockedUpgradeNeeded)
          .WillOnce(RunClosure(run_loop.QuitClosure()));
      mojo::AssociatedRemote<blink::mojom::IDBTransaction> transaction_remote;
      factory_remote->Open(client.CreateInterfacePtrAndBind(),
                           database_callbacks.CreateInterfacePtrAndBind(),
                           kDatabaseName, db_version,
                           transaction_remote.BindNewEndpointAndPassReceiver(),
                           /*transaction_id=*/1, /*priority=*/0);
      run_loop.Run();
    }
  }

  if (IsSqliteBackingStoreEnabled()) {
    histograms.ExpectTotalCount("IndexedDB.FilePathLengthOverflow.LevelDB", 0);
  } else {
    // Normal origin: no path length issues; underflow buckets.
    histograms.ExpectUniqueSample("IndexedDB.FilePathLengthOverflow.LevelDB", 0,
                                  1);
    histograms.ExpectUniqueSample("IndexedDB.FilePathLengthOverflow.SQLite", 0,
                                  1);
  }

  // Open with a super long origin; error.
  bucket_info = GetOrCreateBucket(blink::StorageKey::CreateFromStringForTesting(
      std::string("https://") + std::string(230, 'a') + ".com:81"));
  {
    mojo::Remote<blink::mojom::IDBFactory> factory_remote;
    mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
        checker_remote;
    BindFactory(std::move(checker_remote),
                factory_remote.BindNewPipeAndPassReceiver(), bucket_info);

    {
      const int64_t db_version = 1;
      MockMojoFactoryClient client;
      MockMojoDatabaseCallbacks database_callbacks;
      base::RunLoop run_loop;
      EXPECT_CALL(client, Error).WillOnce(RunClosure(run_loop.QuitClosure()));
      mojo::AssociatedRemote<blink::mojom::IDBTransaction> transaction_remote;
      factory_remote->Open(client.CreateInterfacePtrAndBind(),
                           database_callbacks.CreateInterfacePtrAndBind(),
                           kDatabaseName, db_version,
                           transaction_remote.BindNewEndpointAndPassReceiver(),
                           /*transaction_id=*/1, /*priority=*/0);
      run_loop.Run();
    }
  }

  if (IsSqliteBackingStoreEnabled()) {
    histograms.ExpectTotalCount("IndexedDB.FilePathLengthOverflow.LevelDB", 0);
  } else {
    // Expect additional logs to both of the histograms. Note that the exact
    // bucket depends on the length of the temp dir.
    histograms.ExpectTotalCount("IndexedDB.FilePathLengthOverflow.LevelDB", 2);
    histograms.ExpectTotalCount("IndexedDB.FilePathLengthOverflow.SQLite", 2);

    // The longest SQLite file name overflows by more than the LevelDB
    // equivalent.
    EXPECT_LT(
        histograms.GetTotalSum("IndexedDB.FilePathLengthOverflow.LevelDB"),
        histograms.GetTotalSum("IndexedDB.FilePathLengthOverflow.SQLite"));
  }
}
#endif

// Regression test for crbug.com/484647042.
TEST_P(IndexedDBTest, ForceCloseWithQueuedDelete) {
  BucketInfo bucket_info = GetOrCreateBucket(GetTestStorageKey());
  BucketLocator bucket_locator = bucket_info.ToBucketLocator();

  mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
      checker_remote;
  BindFactory(std::move(checker_remote),
              factory_remote_.BindNewPipeAndPassReceiver(), bucket_info);

  // Open a database at version 1 and complete the upgrade.
  CreateDatabase(factory_remote_, kDatabaseName, /*transaction_id=*/1);

  // Wait for the Database to be destroyed. For SQLite, this starts async
  // cleanup on a background thread.
  BucketContext* bucket_context = GetBucketContext(bucket_info.id);
  ASSERT_TRUE(bucket_context);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return bucket_context->GetDatabasesForTesting().empty(); }));

  // Queue a delete, then force close.
  MockMojoFactoryClient delete_client;
  EXPECT_CALL(delete_client, Error(blink::mojom::IDBException::kAbortError, _));
  factory_remote_->DeleteDatabase(delete_client.CreateInterfacePtrAndBind(),
                                  kDatabaseName, /*force_close=*/false);

  base::RunLoop force_close_loop;
  context_->ForceClose(bucket_locator.id, force_close_loop.QuitClosure());
  force_close_loop.Run();
}

TEST_P(IndexedDBTest, IdleTasksHistograms) {
  const base::TimeDelta kTimeout = BucketContext::GetIdleTimeoutForTesting();
  BucketInfo bucket_info = GetOrCreateBucket(GetTestStorageKey());

  mojo::Remote<blink::mojom::IDBFactory> factory_remote;
  mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
      checker_remote;
  BindFactory(std::move(checker_remote),
              factory_remote.BindNewPipeAndPassReceiver(), bucket_info);

  // Create a database and fast forward by the idle timeout, which should
  // trigger idle tasks.
  mojo::AssociatedRemote<blink::mojom::IDBDatabase> connection;
  {
    base::HistogramTester histograms;
    connection =
        CreateDatabase(factory_remote, kDatabaseName, /*transaction_id=*/1);
    task_environment_.FastForwardBy(kTimeout);
    histograms.ExpectTotalCount(
        "IndexedDB.IdleTasksCompletionToNextActivity.OnDisk", 0);
    histograms.ExpectTotalCount("IndexedDB.BackendDuration.RunIdleTasks.OnDisk",
                                1);
  }

  // Create another database, which should restart (reset) the timer.
  mojo::AssociatedRemote<blink::mojom::IDBDatabase> other_connection;
  {
    base::HistogramTester histograms;
    other_connection =
        CreateDatabase(factory_remote, u"other_db", /*transaction_id=*/2);
    histograms.ExpectTotalCount(
        "IndexedDB.IdleTasksCompletionToNextActivity.OnDisk", 1);
    histograms.ExpectTotalCount("IndexedDB.BackendDuration.RunIdleTasks.OnDisk",
                                0);
  }

  // After a delay, perform some more activity.
  {
    base::HistogramTester histograms;
    const base::TimeDelta kDelay = kTimeout / 3;
    task_environment_.FastForwardBy(kDelay);

    MockMojoFactoryClient client;
    base::RunLoop delete_loop;
    EXPECT_CALL(client, DeleteSuccess)
        .WillOnce(base::test::RunClosure(delete_loop.QuitClosure()));
    factory_remote->DeleteDatabase(client.CreateInterfacePtrAndBind(),
                                   kDatabaseName, /*force_close=*/false);
    connection.reset();
    delete_loop.Run();

    // The new activity should have pushed the idle timer further by `kDelay`.
    task_environment_.FastForwardBy(kTimeout - kDelay);
    histograms.ExpectTotalCount("IndexedDB.BackendDuration.RunIdleTasks.OnDisk",
                                0);
    task_environment_.FastForwardBy(kDelay);
    histograms.ExpectTotalCount("IndexedDB.BackendDuration.RunIdleTasks.OnDisk",
                                1);
  }
}

class IndexedDBSqliteTest : public IndexedDBTestBase {
 public:
  IndexedDBSqliteTest()
      : IndexedDBTestBase(
            /*use_default_buckets=*/true,
            /*use_sqlite=*/true) {}
  ~IndexedDBSqliteTest() override = default;
};

// Makes sure that reading from a blob registers as "activity" which in turn
// defers idle maintenance tasks.
TEST_F(IndexedDBSqliteTest, BlobReadPutsOffIdleWork) {
  const int64_t kObjectStoreId = 10;
  const char16_t kObjectStoreName[] = u"os";
  const std::string kBlobData =
      base::RandBytesAsString(TestBlobConsumer::kPipeCapacity * 3);

  BucketInfo bucket_info = GetOrCreateBucket(GetTestStorageKey());
  BucketLocator bucket_locator = bucket_info.ToBucketLocator();

  // Bind the IDBFactory.
  mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
      checker_remote;
  BindFactory(std::move(checker_remote),
              factory_remote_.BindNewPipeAndPassReceiver(), bucket_info);

  const IndexedDBKey key(u"key");

  // Create a database with an object store with a record containing a blob.
  MojoConnectionHelper mojo_helper;
  mojo_helper.OpenAndExpectUpgradeNeeded(factory_remote_.get());

  mojo_helper.vc_txn->CreateObjectStore(kObjectStoreId, kObjectStoreName,
                                        blink::IndexedDBKeyPath(), false);

  auto fake_blob = std::make_unique<storage::FakeBlob>("test-uuid");
  fake_blob->set_body(kBlobData);

  std::vector<blink::mojom::IDBExternalObjectPtr> external_objects;
  external_objects.push_back(blink::mojom::IDBExternalObject::NewBlobOrFile(
      blink::mojom::IDBBlobInfo::New(fake_blob->Clone(), u"text/plain",
                                     static_cast<int64_t>(kBlobData.size()),
                                     /*file=*/nullptr)));

  auto new_value = blink::mojom::IDBValue::New();
  new_value->bits = mojo_base::BigBuffer(base::as_byte_span("value"));
  new_value->external_objects = std::move(external_objects);

  base::MockCallback<blink::mojom::IDBTransaction::PutCallback> put_callback;
  mojo_helper.vc_txn->Put(kObjectStoreId, std::move(new_value), key.Clone(),
                          blink::mojom::IDBPutMode::AddOnly,
                          std::vector<IndexedDBIndexKeys>(),
                          put_callback.Get());
  mojo_helper.vc_txn->Commit(0);

  // Open a read transaction to get the record with the blob and verify reading
  // the blob resets the idle timer.
  mojo::AssociatedRemote<blink::mojom::IDBTransaction> read_transaction;
  mojo_helper.database->CreateTransaction(
      read_transaction.BindNewEndpointAndPassReceiver(),
      /*transaction_id=*/2, {kObjectStoreId},
      blink::mojom::IDBTransactionMode::ReadOnly,
      blink::mojom::IDBTransactionDurability::Relaxed);
  base::test::TestFuture<blink::mojom::IDBDatabaseGetResultPtr> get_future;
  mojo_helper.database->Get(
      /*transaction_id=*/2, kObjectStoreId,
      blink::IndexedDBIndexMetadata::kInvalidId,
      blink::IndexedDBKeyRange(key.Clone(), key.Clone(),
                               /*lower_open=*/false,
                               /*upper_open=*/false),
      /*key_only=*/false, get_future.GetCallback());

  blink::mojom::IDBDatabaseGetResultPtr result = get_future.Take();
  ASSERT_TRUE(result->is_value());
  ASSERT_FALSE(result->get_value()->value->external_objects.empty());
  ASSERT_TRUE(
      result->get_value()->value->external_objects[0]->is_blob_or_file());

  mojo::Remote<blink::mojom::Blob> blob(
      std::move(result->get_value()
                    ->value->external_objects[0]
                    ->get_blob_or_file()
                    ->blob));
  ASSERT_TRUE(blob.is_bound());

  // Activity above should have started the idle timer.
  BucketContext* bucket_context = GetBucketContext(bucket_locator.id);
  ASSERT_TRUE(bucket_context);
  EXPECT_TRUE(bucket_context->idle_timer_.IsRunning());
  base::TimeTicks next_idle_maintenance_time_before =
      bucket_context->idle_timer_.ExpectedFiringTimeForTesting();

  // Now read the blob. This should trigger OnActivity() and reset the timer.
  task_environment_.FastForwardBy(BucketContext::GetIdleTimeoutForTesting() /
                                  3);

  base::TimeTicks next_idle_maintenance_time_after_partial_read;
  base::RunLoop partial_loop;
  auto on_partial_read = base::BindLambdaForTesting([&]() {
    next_idle_maintenance_time_after_partial_read =
        bucket_context->idle_timer_.ExpectedFiringTimeForTesting();
    // Inject artificial delay since in the test context, the operation
    // completes synchronously.
    task_environment_.FastForwardBy(BucketContext::GetIdleTimeoutForTesting() /
                                    3);
    partial_loop.Quit();
  });
  base::test::TestFuture<std::string> blob_future;
  TestBlobConsumer::ReadWholeBlob(blob, blob_future.GetCallback(),
                                  on_partial_read);
  partial_loop.Run();

  // The idle timer's desired run time should have been pushed forward
  // because beginning reading the blob triggered OnActivity().
  EXPECT_GT(next_idle_maintenance_time_after_partial_read,
            next_idle_maintenance_time_before);

  // Finishing reading the blob also counts as activity.
  EXPECT_EQ(kBlobData, blob_future.Get());
  EXPECT_GT(bucket_context->idle_timer_.ExpectedFiringTimeForTesting(),
            next_idle_maintenance_time_after_partial_read);
}

// Regression test for a compromised renderer forging the declared size of an
// IndexedDB blob to be smaller than its actual data: crbug.com/497660733.
TEST_P(IndexedDBTest, BlobWithForgedSize) {
  const int64_t kTransactionId = 1;
  const int64_t kObjectStoreId = 10;
  const char16_t kObjectStoreName[] = u"os";
  const IndexedDBKey kKey(u"key");

  const std::string kBlobData(10000, 'A');
  const int64_t kForgedBlobSize = 100;

  blob_storage_context_.SetWriteFilesToDisk(true);

  storage::BucketInfo bucket_info = GetOrCreateBucket(GetTestStorageKey());

  mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
      checker_remote;
  BindFactory(std::move(checker_remote),
              factory_remote_.BindNewPipeAndPassReceiver(), bucket_info);

  MockMojoFactoryClient client;
  MockMojoDatabaseCallbacks database_callbacks;
  mojo::AssociatedRemote<blink::mojom::IDBTransaction> transaction_remote;
  mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_database;

  // Wait for UpgradeNeeded.
  base::RunLoop upgrade_loop;
  EXPECT_CALL(client, MockedUpgradeNeeded)
      .WillOnce(
          testing::DoAll(MoveArgPointee<0>(&pending_database),
                         ::base::test::RunClosure(upgrade_loop.QuitClosure())));
  factory_remote_->Open(client.CreateInterfacePtrAndBind(),
                        database_callbacks.CreateInterfacePtrAndBind(),
                        kDatabaseName, /*version=*/1,
                        transaction_remote.BindNewEndpointAndPassReceiver(),
                        kTransactionId, /*priority=*/0);
  upgrade_loop.Run();

  mojo::AssociatedRemote<blink::mojom::IDBDatabase> database(
      std::move(pending_database));
  ASSERT_TRUE(database.is_bound());

  transaction_remote->CreateObjectStore(kObjectStoreId, kObjectStoreName,
                                        blink::IndexedDBKeyPath(), false);

  // Create a FakeBlob with a large body but declare a small (forged) size.
  auto fake_blob = std::make_unique<storage::FakeBlob>("test-uuid");
  fake_blob->set_body(kBlobData);

  std::vector<blink::mojom::IDBExternalObjectPtr> external_objects;
  external_objects.push_back(blink::mojom::IDBExternalObject::NewBlobOrFile(
      blink::mojom::IDBBlobInfo::New(fake_blob->Clone(), u"text/plain",
                                     kForgedBlobSize,
                                     /*file=*/nullptr)));

  auto new_value = blink::mojom::IDBValue::New();
  new_value->bits = mojo_base::BigBuffer(base::as_byte_span("value"));
  new_value->external_objects = std::move(external_objects);

  transaction_remote->Put(kObjectStoreId, std::move(new_value), kKey.Clone(),
                          blink::mojom::IDBPutMode::AddOnly,
                          std::vector<IndexedDBIndexKeys>(), base::DoNothing());
  transaction_remote->Commit(0);

  // The blob write should fail because the actual blob size doesn't match the
  // declared size, aborting the transaction.
  base::RunLoop error_loop;
  base::RepeatingClosure quit_closure =
      base::BarrierClosure(2, error_loop.QuitClosure());

  EXPECT_CALL(database_callbacks,
              Abort(kTransactionId, blink::mojom::IDBException::kDataError, _))
      .WillOnce(RunClosure(quit_closure));

  EXPECT_CALL(client, Error(blink::mojom::IDBException::kAbortError, _))
      .WillOnce(RunClosure(std::move(quit_closure)));

  error_loop.Run();
}

}  // namespace content::indexed_db
