// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/843764): `IndexedDBDispatcherHost` has been removed, but
// surprisingly this file did not actually rely on it. These tests, many of
// which are disabled, should be cleaned up and merged into other unit tests,
// such as `IndexedDBTest`.

// #include "content/browser/indexed_db/indexed_db_dispatcher_host.h"

#include <tuple>

#include "base/barrier_closure.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread.h"
#include "base/time/default_clock.h"
#include "build/build_config.h"
#include "components/services/storage/privileged/mojom/indexed_db_client_state_checker.mojom.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_database_callbacks.h"
#include "content/browser/indexed_db/indexed_db_factory.h"
#include "content/browser/indexed_db/indexed_db_factory_client.h"
#include "content/browser/indexed_db/indexed_db_pending_connection.h"
#include "content/browser/indexed_db/mock_mojo_indexed_db_database_callbacks.h"
#include "content/browser/indexed_db/mock_mojo_indexed_db_factory_client.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "storage/browser/test/mock_quota_manager.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"
#include "url/origin.h"

using base::test::RunClosure;
using blink::IndexedDBDatabaseMetadata;
using blink::IndexedDBIndexKeys;
using blink::IndexedDBKey;
using blink::mojom::IDBValue;
using blink::mojom::IDBValuePtr;
using testing::_;
using testing::StrictMock;

namespace content {
namespace {

// TODO(crbug.com/889590): Replace with common converter.
url::Origin ToOrigin(const std::string& url) {
  return url::Origin::Create(GURL(url));
}

ACTION_TEMPLATE(MoveArgPointee,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(out)) {
  *out = std::move(*::testing::get<k>(args));
}

ACTION_P(QuitLoop, run_loop) {
  run_loop->Quit();
}

MATCHER_P(IsAssociatedInterfacePtrInfoValid,
          tf,
          std::string(negation ? "isn't" : "is") + " " +
              std::string(tf ? "valid" : "invalid")) {
  return tf == arg->is_valid();
}

MATCHER_P(MatchesIDBKey, key, "") {
  return arg.Equals(key);
}

static const char16_t kDatabaseName[] = u"db";
static const char kOrigin[] = "https://www.example.com";

base::FilePath CreateAndReturnTempDir(base::ScopedTempDir* temp_dir) {
  CHECK(temp_dir->CreateUniqueTempDir());
  return temp_dir->GetPath();
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
        open_callbacks(
            std::make_unique<StrictMock<MockMojoIndexedDBFactoryClient>>()),
        connection_callbacks(
            std::make_unique<
                StrictMock<MockMojoIndexedDBDatabaseCallbacks>>()) {}

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
        upgrade_txn_id);
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

  std::unique_ptr<MockMojoIndexedDBFactoryClient> open_callbacks;
  std::unique_ptr<MockMojoIndexedDBDatabaseCallbacks> connection_callbacks;
};

class TestIndexedDBObserver : public storage::mojom::IndexedDBObserver {
 public:
  explicit TestIndexedDBObserver(
      mojo::PendingReceiver<storage::mojom::IndexedDBObserver> receiver)
      : receiver_(this, std::move(receiver)) {}

  void OnIndexedDBListChanged(
      const storage::BucketLocator& bucket_locator) override {
    ++notify_list_changed_count;
  }

  void OnIndexedDBContentChanged(
      const storage::BucketLocator& bucket_locator,
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

class IndexedDBDispatcherHostTest : public testing::Test {
 public:
  IndexedDBDispatcherHostTest()
      : special_storage_policy_(
            base::MakeRefCounted<storage::MockSpecialStoragePolicy>()),
        quota_manager_(base::MakeRefCounted<storage::MockQuotaManager>(
            /*is_incognito=*/false,
            CreateAndReturnTempDir(&temp_dir_),
            task_environment_.GetMainThreadTaskRunner(),
            special_storage_policy_)),
        context_impl_(base::MakeRefCounted<IndexedDBContextImpl>(
            temp_dir_.GetPath(),
            quota_manager_->proxy(),
            base::DefaultClock::GetInstance(),
            mojo::NullRemote(),
            mojo::NullRemote(),
            task_environment_.GetMainThreadTaskRunner(),
            nullptr)) {}

  IndexedDBDispatcherHostTest(const IndexedDBDispatcherHostTest&) = delete;
  IndexedDBDispatcherHostTest& operator=(const IndexedDBDispatcherHostTest&) =
      delete;

  void SetUp() override {
    base::RunLoop loop;
    context_impl_->IDBTaskRunner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          context_impl_->BindIndexedDB(
              blink::StorageKey::CreateFromStringForTesting(kOrigin),
              mojo::PendingAssociatedRemote<
                  storage::mojom::IndexedDBClientStateChecker>(),
              idb_mojo_factory_.BindNewPipeAndPassReceiver());
          loop.Quit();
        }));
    loop.Run();
  }

  void TearDown() override {
    // Cycle the IndexedDBTaskQueue to remove all IDB tasks.
    {
      base::RunLoop loop;
      context_impl_->IDBTaskRunner()->PostTask(
          FROM_HERE, base::BindLambdaForTesting([&]() {
            idb_mojo_factory_.reset();
            loop.Quit();
          }));
      loop.Run();
    }

    // IndexedDBContextImpl must be released on the IDB sequence.
    {
      scoped_refptr<base::SequencedTaskRunner> idb_task_runner =
          context_impl_->IDBTaskRunner();
      context_impl_->ReleaseOnIDBSequence(std::move(context_impl_));
      base::RunLoop loop;
      idb_task_runner->PostTask(FROM_HERE, loop.QuitClosure());
      loop.Run();
    }

    quota_manager_ = nullptr;
    task_environment_.RunUntilIdle();
    // File are leaked if this doesn't return true.
    ASSERT_TRUE(temp_dir_.Delete());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<storage::MockSpecialStoragePolicy> special_storage_policy_;
  scoped_refptr<storage::MockQuotaManager> quota_manager_;
  scoped_refptr<IndexedDBContextImpl> context_impl_;
  mojo::Remote<blink::mojom::IDBFactory> idb_mojo_factory_;
};

TEST_F(IndexedDBDispatcherHostTest, CloseConnectionBeforeUpgrade) {
  const int64_t kDBVersion = 1;
  const int64_t kTransactionId = 1;
  std::unique_ptr<TestDatabaseConnection> connection;
  IndexedDBDatabaseMetadata metadata;
  mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_database;

  base::RunLoop loop;
  context_impl_->IDBTaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        connection = std::make_unique<TestDatabaseConnection>(
            context_impl_->IDBTaskRunner(), url::Origin::Create(GURL(kOrigin)),
            kDatabaseName, kDBVersion, kTransactionId);
        EXPECT_CALL(*connection->open_callbacks,
                    MockedUpgradeNeeded(IsAssociatedInterfacePtrInfoValid(true),
                                        IndexedDBDatabaseMetadata::NO_VERSION,
                                        blink::mojom::IDBDataLoss::None,
                                        std::string(""), _))
            .WillOnce(testing::DoAll(MoveArgPointee<0>(&pending_database),
                                     testing::SaveArg<4>(&metadata),
                                     QuitLoop(&loop)));

        connection->Open(idb_mojo_factory_.get());
      }));
  loop.Run();

  EXPECT_TRUE(pending_database.is_valid());
  EXPECT_EQ(connection->version, metadata.version);
  EXPECT_EQ(connection->db_name, metadata.name);

  base::RunLoop loop2;
  context_impl_->IDBTaskRunner()->PostTask(FROM_HERE,
                                           base::BindLambdaForTesting([&]() {
                                             connection.reset();
                                             loop2.Quit();
                                           }));
  loop2.Run();
}

TEST_F(IndexedDBDispatcherHostTest, CloseAfterUpgrade) {
  const int64_t kDBVersion = 1;
  const int64_t kTransactionId = 1;
  const int64_t kObjectStoreId = 10;
  const char16_t kObjectStoreName[] = u"os";
  std::unique_ptr<TestDatabaseConnection> connection;
  IndexedDBDatabaseMetadata metadata;
  mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_database;

  base::RunLoop loop;
  context_impl_->IDBTaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        // Open connection.
        connection = std::make_unique<TestDatabaseConnection>(
            context_impl_->IDBTaskRunner(), ToOrigin(kOrigin), kDatabaseName,
            kDBVersion, kTransactionId);

        EXPECT_CALL(*connection->open_callbacks,
                    MockedUpgradeNeeded(IsAssociatedInterfacePtrInfoValid(true),
                                        IndexedDBDatabaseMetadata::NO_VERSION,
                                        blink::mojom::IDBDataLoss::None,
                                        std::string(""), _))
            .WillOnce(testing::DoAll(MoveArgPointee<0>(&pending_database),
                                     testing::SaveArg<4>(&metadata),
                                     QuitLoop(&loop)));

        // Queue open request message.
        connection->Open(idb_mojo_factory_.get());
      }));
  loop.Run();

  ASSERT_TRUE(pending_database.is_valid());
  EXPECT_EQ(connection->version, metadata.version);
  EXPECT_EQ(connection->db_name, metadata.name);

  base::RunLoop loop2;
  base::RepeatingClosure quit_closure2 =
      base::BarrierClosure(2, loop2.QuitClosure());
  context_impl_->IDBTaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        ::testing::InSequence dummy;
        EXPECT_CALL(*connection->connection_callbacks, Complete(kTransactionId))
            .Times(1)
            .WillOnce(RunClosure(quit_closure2));
        EXPECT_CALL(
            *connection->open_callbacks,
            MockedOpenSuccess(IsAssociatedInterfacePtrInfoValid(false), _))
            .Times(1)
            .WillOnce(RunClosure(std::move(quit_closure2)));

        connection->database.Bind(std::move(pending_database));
        ASSERT_TRUE(connection->database.is_bound());
        ASSERT_TRUE(connection->version_change_transaction.is_bound());
        connection->version_change_transaction->CreateObjectStore(
            kObjectStoreId, kObjectStoreName, blink::IndexedDBKeyPath(), false);
        connection->version_change_transaction->Commit(0);
      }));
  loop2.Run();

  base::RunLoop loop3;
  context_impl_->IDBTaskRunner()->PostTask(FROM_HERE,
                                           base::BindLambdaForTesting([&]() {
                                             connection.reset();
                                             loop3.Quit();
                                           }));
  loop3.Run();
}

// TODO(crbug.com/1282613): Test is flaky on Mac in debug.
#if BUILDFLAG(IS_MAC) && !defined(NDEBUG)
#define MAYBE_OpenNewConnectionWhileUpgrading \
  DISABLED_OpenNewConnectionWhileUpgrading
#else
#define MAYBE_OpenNewConnectionWhileUpgrading OpenNewConnectionWhileUpgrading
#endif
TEST_F(IndexedDBDispatcherHostTest, MAYBE_OpenNewConnectionWhileUpgrading) {
  const int64_t kDBVersion = 1;
  const int64_t kTransactionId = 1;
  const int64_t kObjectStoreId = 10;
  const char16_t kObjectStoreName[] = u"os";
  std::unique_ptr<TestDatabaseConnection> connection1;
  mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_database1;
  IndexedDBDatabaseMetadata metadata1;

  base::RunLoop loop;
  context_impl_->IDBTaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        // Open connection 1, and expect the upgrade needed.
        connection1 = std::make_unique<TestDatabaseConnection>(
            context_impl_->IDBTaskRunner(), url::Origin::Create(GURL(kOrigin)),
            kDatabaseName, kDBVersion, kTransactionId);

        EXPECT_CALL(*connection1->open_callbacks,
                    MockedUpgradeNeeded(IsAssociatedInterfacePtrInfoValid(true),
                                        IndexedDBDatabaseMetadata::NO_VERSION,
                                        blink::mojom::IDBDataLoss::None,
                                        std::string(""), _))
            .WillOnce(testing::DoAll(MoveArgPointee<0>(&pending_database1),
                                     testing::SaveArg<4>(&metadata1),
                                     QuitLoop(&loop)));

        // Queue open request message.
        connection1->Open(idb_mojo_factory_.get());
      }));
  loop.Run();

  std::unique_ptr<TestDatabaseConnection> connection2;
  mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_database2;
  IndexedDBDatabaseMetadata metadata2;

  base::RunLoop loop2;
  base::RepeatingClosure quit_closure2 =
      base::BarrierClosure(3, loop2.QuitClosure());
  context_impl_->IDBTaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        connection2 = std::make_unique<TestDatabaseConnection>(
            context_impl_->IDBTaskRunner(), ToOrigin(kOrigin), kDatabaseName,
            kDBVersion, 0);

        // Check that we're called in order and the second connection gets it's
        // database after the first connection completes.
        ::testing::InSequence dummy;
        EXPECT_CALL(*connection1->connection_callbacks,
                    Complete(kTransactionId))
            .Times(1)
            .WillOnce(RunClosure(quit_closure2));
        EXPECT_CALL(
            *connection1->open_callbacks,
            MockedOpenSuccess(IsAssociatedInterfacePtrInfoValid(false), _))
            .Times(1)
            .WillOnce(RunClosure(quit_closure2));
        EXPECT_CALL(
            *connection2->open_callbacks,
            MockedOpenSuccess(IsAssociatedInterfacePtrInfoValid(true), _))
            .WillOnce(testing::DoAll(MoveArgPointee<0>(&pending_database2),
                                     testing::SaveArg<1>(&metadata2),
                                     RunClosure(std::move(quit_closure2))));

        connection1->database.Bind(std::move(pending_database1));
        ASSERT_TRUE(connection1->database.is_bound());
        ASSERT_TRUE(connection1->version_change_transaction.is_bound());

        // Open connection 2, but expect that we won't be called back.
        connection2->Open(idb_mojo_factory_.get());

        // Create object store.
        connection1->version_change_transaction->CreateObjectStore(
            kObjectStoreId, kObjectStoreName, blink::IndexedDBKeyPath(), false);
        connection1->version_change_transaction->Commit(0);
      }));
  loop2.Run();

  EXPECT_TRUE(pending_database2.is_valid());
  EXPECT_EQ(connection2->version, metadata2.version);
  EXPECT_EQ(connection2->db_name, metadata2.name);

  base::RunLoop loop3;
  context_impl_->IDBTaskRunner()->PostTask(FROM_HERE,
                                           base::BindLambdaForTesting([&]() {
                                             connection1.reset();
                                             connection2.reset();
                                             loop3.Quit();
                                           }));
  loop3.Run();
}

MATCHER_P(IsCallbackError, error_code, "") {
  if (arg->is_error_result() &&
      arg->get_error_result()->error_code == error_code)
    return true;
  return false;
}

// See https://crbug.com/989723 for more context, this test seems to flake.
TEST_F(IndexedDBDispatcherHostTest, DISABLED_PutWithInvalidBlob) {
  const int64_t kDBVersion = 1;
  const int64_t kTransactionId = 1;
  const int64_t kObjectStoreId = 10;
  const char16_t kObjectStoreName[] = u"os";
  std::unique_ptr<TestDatabaseConnection> connection;
  IndexedDBDatabaseMetadata metadata;
  mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_database;

  base::RunLoop loop;
  context_impl_->IDBTaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        // Open connection.
        connection = std::make_unique<TestDatabaseConnection>(
            context_impl_->IDBTaskRunner(), url::Origin::Create(GURL(kOrigin)),
            kDatabaseName, kDBVersion, kTransactionId);

        EXPECT_CALL(*connection->open_callbacks,
                    MockedUpgradeNeeded(IsAssociatedInterfacePtrInfoValid(true),
                                        IndexedDBDatabaseMetadata::NO_VERSION,
                                        blink::mojom::IDBDataLoss::None,
                                        std::string(""), _))
            .WillOnce(testing::DoAll(MoveArgPointee<0>(&pending_database),
                                     testing::SaveArg<4>(&metadata),
                                     QuitLoop(&loop)));

        // Queue open request message.
        connection->Open(idb_mojo_factory_.get());
      }));
  loop.Run();

  ASSERT_TRUE(pending_database.is_valid());
  EXPECT_EQ(connection->version, metadata.version);
  EXPECT_EQ(connection->db_name, metadata.name);

  base::MockCallback<blink::mojom::IDBTransaction::PutCallback> put_callback;

  base::RunLoop loop2;
  base::RepeatingClosure quit_closure2 =
      base::BarrierClosure(3, loop2.QuitClosure());
  context_impl_->IDBTaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        ::testing::InSequence dummy;

        EXPECT_CALL(
            put_callback,
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
        external_objects.push_back(
            blink::mojom::IDBExternalObject::NewBlobOrFile(
                blink::mojom::IDBBlobInfo::New(std::move(blob), "fakeUUID",
                                               std::u16string(), 100,
                                               nullptr)));

        std::string value = "hello";
        const char* value_data = value.data();
        std::vector<uint8_t> value_vector(value_data,
                                          value_data + value.length());

        auto new_value = blink::mojom::IDBValue::New();
        new_value->bits = std::move(value_vector);
        new_value->external_objects = std::move(external_objects);

        connection->version_change_transaction->Put(
            kObjectStoreId, std::move(new_value), IndexedDBKey(u"hello"),
            blink::mojom::IDBPutMode::AddOnly,
            std::vector<IndexedDBIndexKeys>(), put_callback.Get());
        connection->version_change_transaction->Commit(0);
      }));
  loop2.Run();

  base::RunLoop loop3;
  context_impl_->IDBTaskRunner()->PostTask(FROM_HERE,
                                           base::BindLambdaForTesting([&]() {
                                             connection.reset();
                                             loop3.Quit();
                                           }));
  loop3.Run();
}

// Flaky: crbug.com/772067
TEST_F(IndexedDBDispatcherHostTest, DISABLED_NotifyIndexedDBListChanged) {
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
  context_impl_->AddObserver(std::move(remote));

  // Open connection 1.
  std::unique_ptr<TestDatabaseConnection> connection1;

  IndexedDBDatabaseMetadata metadata1;
  mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_database1;
  EXPECT_EQ(0, observer.notify_list_changed_count);
  {
    base::RunLoop loop;
    context_impl_->IDBTaskRunner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          connection1 = std::make_unique<TestDatabaseConnection>(
              context_impl_->IDBTaskRunner(), ToOrigin(kOrigin), kDatabaseName,
              kDBVersion1, kTransactionId1);

          EXPECT_CALL(*connection1->open_callbacks,
                      MockedUpgradeNeeded(
                          IsAssociatedInterfacePtrInfoValid(true),
                          IndexedDBDatabaseMetadata::NO_VERSION,
                          blink::mojom::IDBDataLoss::None, std::string(), _))
              .WillOnce(testing::DoAll(MoveArgPointee<0>(&pending_database1),
                                       testing::SaveArg<4>(&metadata1),
                                       QuitLoop(&loop)));

          // Queue open request message.
          connection1->Open(idb_mojo_factory_.get());
        }));
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
    context_impl_->IDBTaskRunner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          // Create object store and index.
          connection1->database.Bind(std::move(pending_database1));
          ASSERT_TRUE(connection1->database.is_bound());
          ASSERT_TRUE(connection1->version_change_transaction.is_bound());

          EXPECT_CALL(*connection1->connection_callbacks,
                      Complete(kTransactionId1))
              .Times(1)
              .WillOnce(RunClosure(quit_closure));
          EXPECT_CALL(
              *connection1->open_callbacks,
              MockedOpenSuccess(IsAssociatedInterfacePtrInfoValid(false), _))
              .Times(1)
              .WillOnce(RunClosure(std::move(quit_closure)));

          ASSERT_TRUE(connection1->database.is_bound());
          connection1->version_change_transaction->CreateObjectStore(
              kObjectStoreId, kObjectStoreName, blink::IndexedDBKeyPath(),
              false);
          connection1->database->CreateIndex(
              kTransactionId1, kObjectStoreId, kIndexId, kIndexName,
              blink::IndexedDBKeyPath(), false, false);
          connection1->version_change_transaction->Commit(0);
        }));
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
    context_impl_->IDBTaskRunner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          connection1->database->Close();

          connection2 = std::make_unique<TestDatabaseConnection>(
              context_impl_->IDBTaskRunner(),
              url::Origin::Create(GURL(kOrigin)), kDatabaseName, kDBVersion2,
              kTransactionId2);

          EXPECT_CALL(*connection2->open_callbacks,
                      MockedUpgradeNeeded(
                          IsAssociatedInterfacePtrInfoValid(true), kDBVersion1,
                          blink::mojom::IDBDataLoss::None, std::string(), _))
              .WillOnce(testing::DoAll(MoveArgPointee<0>(&pending_database2),
                                       testing::SaveArg<4>(&metadata2),
                                       QuitLoop(&loop)));

          // Queue open request message.
          connection2->Open(idb_mojo_factory_.get());
        }));
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
    context_impl_->IDBTaskRunner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          // Delete index.
          connection2->database.Bind(std::move(pending_database2));
          ASSERT_TRUE(connection2->database.is_bound());
          ASSERT_TRUE(connection2->version_change_transaction.is_bound());

          EXPECT_CALL(*connection2->connection_callbacks,
                      Complete(kTransactionId2))
              .Times(1)
              .WillOnce(RunClosure(quit_closure));
          EXPECT_CALL(
              *connection2->open_callbacks,
              MockedOpenSuccess(IsAssociatedInterfacePtrInfoValid(false), _))
              .Times(1)
              .WillOnce(RunClosure(std::move(quit_closure)));

          ASSERT_TRUE(connection2->database.is_bound());
          connection2->database->DeleteIndex(kTransactionId2, kObjectStoreId,
                                             kIndexId);
          connection2->version_change_transaction->Commit(0);
        }));
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
    context_impl_->IDBTaskRunner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          connection2->database->Close();
          connection3 = std::make_unique<TestDatabaseConnection>(
              context_impl_->IDBTaskRunner(), ToOrigin(kOrigin), kDatabaseName,
              kDBVersion3, kTransactionId3);

          EXPECT_CALL(*connection3->open_callbacks,
                      MockedUpgradeNeeded(
                          IsAssociatedInterfacePtrInfoValid(true), kDBVersion2,
                          blink::mojom::IDBDataLoss::None, std::string(), _))
              .WillOnce(testing::DoAll(MoveArgPointee<0>(&pending_database3),
                                       testing::SaveArg<4>(&metadata3),
                                       QuitLoop(&loop)));

          // Queue open request message.
          connection3->Open(idb_mojo_factory_.get());
        }));
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
    context_impl_->IDBTaskRunner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          // Delete object store.
          connection3->database.Bind(std::move(pending_database3));
          ASSERT_TRUE(connection3->database.is_bound());
          ASSERT_TRUE(connection3->version_change_transaction.is_bound());

          EXPECT_CALL(*connection3->connection_callbacks,
                      Complete(kTransactionId3))
              .Times(1)
              .WillOnce(RunClosure(quit_closure));
          EXPECT_CALL(
              *connection3->open_callbacks,
              MockedOpenSuccess(IsAssociatedInterfacePtrInfoValid(false), _))
              .Times(1)
              .WillOnce(RunClosure(std::move(quit_closure)));

          ASSERT_TRUE(connection3->database.is_bound());
          connection3->version_change_transaction->DeleteObjectStore(
              kObjectStoreId);
          connection3->version_change_transaction->Commit(0);
        }));
    loop.Run();
  }
  EXPECT_EQ(4, observer.notify_list_changed_count);

  {
    base::RunLoop loop;
    context_impl_->IDBTaskRunner()->PostTask(FROM_HERE,
                                             base::BindLambdaForTesting([&]() {
                                               connection1.reset();
                                               connection2.reset();
                                               connection3.reset();
                                               loop.Quit();
                                             }));
    loop.Run();
  }
}

MATCHER(IsSuccessKey, "") {
  return arg->is_key();
}

// The test is flaky. See https://crbug.com/879213
TEST_F(IndexedDBDispatcherHostTest, DISABLED_NotifyIndexedDBContentChanged) {
  const int64_t kDBVersion1 = 1;
  const int64_t kDBVersion2 = 2;
  const int64_t kTransactionId1 = 1;
  const int64_t kTransactionId2 = 2;
  const int64_t kObjectStoreId = 10;
  const char16_t kObjectStoreName[] = u"os";

  mojo::PendingReceiver<storage::mojom::IndexedDBObserver> receiver;
  mojo::PendingRemote<storage::mojom::IndexedDBObserver> remote;
  TestIndexedDBObserver observer(remote.InitWithNewPipeAndPassReceiver());
  context_impl_->AddObserver(std::move(remote));
  EXPECT_EQ(0, observer.notify_list_changed_count);
  EXPECT_EQ(0, observer.notify_content_changed_count);

  std::unique_ptr<TestDatabaseConnection> connection1;
  IndexedDBDatabaseMetadata metadata1;
  mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_database1;

  base::RunLoop loop;
  context_impl_->IDBTaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        // Open connection 1.
        connection1 = std::make_unique<TestDatabaseConnection>(
            context_impl_->IDBTaskRunner(), url::Origin::Create(GURL(kOrigin)),
            kDatabaseName, kDBVersion1, kTransactionId1);

        EXPECT_CALL(*connection1->open_callbacks,
                    MockedUpgradeNeeded(IsAssociatedInterfacePtrInfoValid(true),
                                        IndexedDBDatabaseMetadata::NO_VERSION,
                                        blink::mojom::IDBDataLoss::None,
                                        std::string(), _))
            .WillOnce(testing::DoAll(MoveArgPointee<0>(&pending_database1),
                                     testing::SaveArg<4>(&metadata1),
                                     QuitLoop(&loop)));

        // Queue open request message.
        connection1->Open(idb_mojo_factory_.get());
      }));
  loop.Run();

  EXPECT_TRUE(pending_database1.is_valid());
  EXPECT_EQ(connection1->version, metadata1.version);
  EXPECT_EQ(connection1->db_name, metadata1.name);

  base::MockCallback<blink::mojom::IDBTransaction::PutCallback> put_callback;

  // Add object store entry.
  base::RunLoop loop2;
  base::RepeatingClosure quit_closure2 =
      base::BarrierClosure(3, loop2.QuitClosure());
  context_impl_->IDBTaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        ::testing::InSequence dummy;

        EXPECT_CALL(put_callback, Run(IsSuccessKey()))
            .Times(1)
            .WillOnce(RunClosure(quit_closure2));
        EXPECT_CALL(*connection1->connection_callbacks,
                    Complete(kTransactionId1))
            .Times(1)
            .WillOnce(RunClosure(quit_closure2));
        EXPECT_CALL(
            *connection1->open_callbacks,
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
        std::vector<uint8_t> value_vector(value_data,
                                          value_data + value.length());

        auto new_value = blink::mojom::IDBValue::New();
        new_value->bits = std::move(value_vector);

        connection1->version_change_transaction->Put(
            kObjectStoreId, std::move(new_value), IndexedDBKey(u"key"),
            blink::mojom::IDBPutMode::AddOnly,
            std::vector<IndexedDBIndexKeys>(), put_callback.Get());
        connection1->version_change_transaction->Commit(0);
      }));
  loop2.Run();

  EXPECT_EQ(2, observer.notify_list_changed_count);
  EXPECT_EQ(1, observer.notify_content_changed_count);

  base::RunLoop loop3;
  context_impl_->IDBTaskRunner()->PostTask(FROM_HERE,
                                           base::BindLambdaForTesting([&]() {
                                             connection1->database->Close();
                                             connection1.reset();
                                             loop3.Quit();
                                           }));
  loop3.Run();

  std::unique_ptr<TestDatabaseConnection> connection2;
  IndexedDBDatabaseMetadata metadata2;
  mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_database2;

  // Open connection 2.
  base::RunLoop loop4;
  context_impl_->IDBTaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        ::testing::InSequence dummy;

        connection2 = std::make_unique<TestDatabaseConnection>(
            context_impl_->IDBTaskRunner(), ToOrigin(kOrigin), kDatabaseName,
            kDBVersion2, kTransactionId2);

        EXPECT_CALL(*connection2->open_callbacks,
                    MockedUpgradeNeeded(
                        IsAssociatedInterfacePtrInfoValid(true), kDBVersion1,
                        blink::mojom::IDBDataLoss::None, std::string(), _))
            .WillOnce(testing::DoAll(MoveArgPointee<0>(&pending_database2),
                                     testing::SaveArg<4>(&metadata2),
                                     QuitLoop(&loop4)));

        // Queue open request message.
        connection2->Open(idb_mojo_factory_.get());
      }));
  loop4.Run();

  EXPECT_TRUE(pending_database2.is_valid());
  EXPECT_EQ(connection2->version, metadata2.version);
  EXPECT_EQ(connection2->db_name, metadata2.name);

  // Clear object store.
  base::RunLoop loop5;
  base::RepeatingClosure quit_closure5 =
      base::BarrierClosure(3, loop5.QuitClosure());
  context_impl_->IDBTaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        ::testing::InSequence dummy;

        EXPECT_CALL(*connection2->connection_callbacks,
                    Complete(kTransactionId2))
            .Times(1)
            .WillOnce(RunClosure(quit_closure5));
        EXPECT_CALL(
            *connection2->open_callbacks,
            MockedOpenSuccess(IsAssociatedInterfacePtrInfoValid(false), _))
            .Times(1)
            .WillOnce(RunClosure(quit_closure5));

        connection2->database.Bind(std::move(pending_database2));
        ASSERT_TRUE(connection2->database.is_bound());
        ASSERT_TRUE(connection2->version_change_transaction.is_bound());
        connection2->database->Clear(kTransactionId2, kObjectStoreId,
                                     base::IgnoreArgs<bool>(quit_closure5));
        connection2->version_change_transaction->Commit(0);
      }));
  loop5.Run();

  // +2 list changed, one for the transaction, the other for the ~DatabaseImpl
  EXPECT_EQ(4, observer.notify_list_changed_count);
  EXPECT_EQ(2, observer.notify_content_changed_count);

  base::RunLoop loop6;
  context_impl_->IDBTaskRunner()->PostTask(FROM_HERE,
                                           base::BindLambdaForTesting([&]() {
                                             connection2.reset();
                                             loop6.Quit();
                                           }));
  loop6.Run();
}

TEST_F(IndexedDBDispatcherHostTest, DISABLED_DatabaseOperationSequencing) {
  const int64_t kDBVersion = 1;
  const int64_t kTransactionId = 1;
  const std::u16string kObjectStoreName1 = u"os1";
  const std::u16string kObjectStoreName2 = u"os2";
  const std::u16string kObjectStoreName3 = u"os3";

  std::unique_ptr<TestDatabaseConnection> connection;
  IndexedDBDatabaseMetadata metadata;
  mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_database;

  // Open the connection, which will initiate the "upgrade" transaction.
  base::RunLoop loop;
  context_impl_->IDBTaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        // Open connection.
        connection = std::make_unique<TestDatabaseConnection>(
            context_impl_->IDBTaskRunner(), ToOrigin(kOrigin), kDatabaseName,
            kDBVersion, kTransactionId);

        EXPECT_CALL(*connection->open_callbacks,
                    MockedUpgradeNeeded(IsAssociatedInterfacePtrInfoValid(true),
                                        IndexedDBDatabaseMetadata::NO_VERSION,
                                        blink::mojom::IDBDataLoss::None,
                                        std::string(""), _))
            .WillOnce(testing::DoAll(MoveArgPointee<0>(&pending_database),
                                     testing::SaveArg<4>(&metadata),
                                     QuitLoop(&loop)));

        // Queue open request message.
        connection->Open(idb_mojo_factory_.get());
      }));
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
  context_impl_->IDBTaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        ::testing::InSequence dummy;
        EXPECT_CALL(*connection->connection_callbacks, Complete(kTransactionId))
            .Times(1)
            .WillOnce(RunClosure(quit_closure2));
        EXPECT_CALL(
            *connection->open_callbacks,
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
        connection->version_change_transaction->DeleteObjectStore(
            object_store_id);

        // Ensure that a create/delete pair where both parts are queued
        // succeeds.
        connection->version_change_transaction->CreateObjectStore(
            ++object_store_id, kObjectStoreName2, blink::IndexedDBKeyPath(),
            /*auto_increment=*/false);
        connection->version_change_transaction->DeleteObjectStore(
            object_store_id);

        // This store is left over, just to verify that everything
        // ran correctly.
        connection->version_change_transaction->CreateObjectStore(
            ++object_store_id, kObjectStoreName3, blink::IndexedDBKeyPath(),
            /*auto_increment=*/false);

        connection->version_change_transaction->Commit(0);
      }));
  loop2.Run();

  EXPECT_EQ(1ULL, metadata2.object_stores.size());
  EXPECT_EQ(metadata2.object_stores[object_store_id].name, kObjectStoreName3);

  // Close the connection to finish the test nicely.
  base::RunLoop loop3;
  context_impl_->IDBTaskRunner()->PostTask(FROM_HERE,
                                           base::BindLambdaForTesting([&]() {
                                             connection.reset();
                                             loop3.Quit();
                                           }));
  loop3.Run();
}

}  // namespace content
