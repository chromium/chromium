// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_dispatcher_host.h"

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread.h"
#include "base/time/default_clock.h"
#include "build/build_config.h"
#include "content/browser/indexed_db/indexed_db_callbacks.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_database_callbacks.h"
#include "content/browser/indexed_db/indexed_db_factory.h"
#include "content/browser/indexed_db/indexed_db_leveldb_env.h"
#include "content/browser/indexed_db/indexed_db_pending_connection.h"
#include "content/browser/indexed_db/mock_mojo_indexed_db_callbacks.h"
#include "content/browser/indexed_db/mock_mojo_indexed_db_database_callbacks.h"
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

static const char kDatabaseName[] = "db";
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
                         base::string16 db_name,
                         int64_t version,
                         int64_t upgrade_txn_id)
      : task_runner(std::move(task_runner)),
        origin(std::move(origin)),
        db_name(std::move(db_name)),
        version(version),
        upgrade_txn_id(upgrade_txn_id),
        open_callbacks(new StrictMock<MockMojoIndexedDBCallbacks>()),
        connection_callbacks(
            new StrictMock<MockMojoIndexedDBDatabaseCallbacks>()) {}
  TestDatabaseConnection& operator=(TestDatabaseConnection&& other) noexcept =
      default;
  ~TestDatabaseConnection() {}

  void Open(blink::mojom::IDBFactory* factory) {
    factory->Open(
        open_callbacks->CreateInterfacePtrAndBind(),
        connection_callbacks->CreateInterfacePtrAndBind(), db_name, version,
        version_change_transaction.BindNewEndpointAndPassReceiver(task_runner),
        upgrade_txn_id);
  }

  scoped_refptr<base::SequencedTaskRunner> task_runner;
  url::Origin origin;
  base::string16 db_name;
  int64_t version;
  int64_t upgrade_txn_id;

  mojo::AssociatedRemote<blink::mojom::IDBDatabase> database;
  mojo::AssociatedRemote<blink::mojom::IDBTransaction>
      version_change_transaction;

  std::unique_ptr<MockMojoIndexedDBCallbacks> open_callbacks;
  std::unique_ptr<MockMojoIndexedDBDatabaseCallbacks> connection_callbacks;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestDatabaseConnection);
};

void TestStatusCallback(base::OnceClosure callback,
                        blink::mojom::IDBStatus* status_out,
                        blink::mojom::IDBStatus status) {
  *status_out = status;
  std::move(callback).Run();
}

class TestIndexedDBObserver : public storage::mojom::IndexedDBObserver {
 public:
  explicit TestIndexedDBObserver(
      mojo::PendingReceiver<storage::mojom::IndexedDBObserver> receiver)
      : receiver_(this, std::move(receiver)) {}

  void OnIndexedDBListChanged(const url::Origin& origin) override {
    ++notify_list_changed_count;
  }

  void OnIndexedDBContentChanged(
      const url::Origin& origin,
      const base::string16& database_name,
      const base::string16& object_store_name) override {
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
            false /*is_incognito*/,
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

  void TearDown() override {
    // Cycle the IndexedDBTaskQueue to remove all IDB tasks.
    {
      base::RunLoop loop;
      context_impl_->IDBTaskRunner()->PostTask(FROM_HERE, loop.QuitClosure());
      loop.Run();
    }
    base::RunLoop loop;
    context_impl_->IDBTaskRunner()->PostTask(FROM_HERE,
                                             base::BindLambdaForTesting([&]() {
                                               idb_mojo_factory_.reset();
                                               loop.Quit();
                                             }));
    loop.Run();
    context_impl_ = nullptr;
    quota_manager_ = nullptr;
    task_environment_.RunUntilIdle();
    // File are leaked if this doesn't return true.
    ASSERT_TRUE(temp_dir_.Delete());
  }

  void SetUp() override {
    base::RunLoop loop;
    context_impl_->IDBTaskRunner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          context_impl_->BindIndexedDB(
              url::Origin::Create(GURL(kOrigin)),
              idb_mojo_factory_.BindNewPipeAndPassReceiver());
          loop.Quit();
        }));
    loop.Run();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<storage::MockSpecialStoragePolicy> special_storage_policy_;
  scoped_refptr<storage::MockQuotaManager> quota_manager_;
  scoped_refptr<IndexedDBContextImpl> context_impl_;
  mojo::Remote<blink::mojom::IDBFactory> idb_mojo_factory_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBDispatcherHostTest);
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
            base::UTF8ToUTF16(kDatabaseName), kDBVersion, kTransactionId);
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

// Flaky on multiple platforms.  http://crbug.com/1001265
TEST_F(IndexedDBDispatcherHostTest, DISABLED_CloseAfterUpgrade) {
  const int64_t kDBVersion = 1;
  const int64_t kTransactionId = 1;
  const int64_t kObjectStoreId = 10;
  const char kObjectStoreName[] = "os";
  std::unique_ptr<TestDatabaseConnection> connection;
  IndexedDBDatabaseMetadata metadata;
  mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_database;

  base::RunLoop loop;
  context_impl_->IDBTaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        // Open connection.
        connection = std::make_unique<TestDatabaseConnection>(
            context_impl_->IDBTaskRunner(), ToOrigin(kOrigin),
            base::UTF8ToUTF16(kDatabaseName), kDBVersion, kTransactionId);

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
            MockedSuccessDatabase(IsAssociatedInterfacePtrInfoValid(false), _))
            .Times(1)
            .WillOnce(RunClosure(std::move(quit_closure2)));

        connection->database.Bind(std::move(pending_database));
        ASSERT_TRUE(connection->database.is_bound());
        ASSERT_TRUE(connection->version_change_transaction.is_bound());
        connection->version_change_transaction->CreateObjectStore(
            kObjectStoreId, base::UTF8ToUTF16(kObjectStoreName),
            blink::IndexedDBKeyPath(), false);
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

// TODO(https://crbug.com/995716) Test is flaky on multiple platforms.
TEST_F(IndexedDBDispatcherHostTest, DISABLED_OpenNewConnectionWhileUpgrading) {
  const int64_t kDBVersion = 1;
  const int64_t kTransactionId = 1;
  const int64_t kObjectStoreId = 10;
  const char kObjectStoreName[] = "os";
  std::unique_ptr<TestDatabaseConnection> connection1;
  mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_database1;
  IndexedDBDatabaseMetadata metadata1;

  base::RunLoop loop;
  context_impl_->IDBTaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        // Open connection 1, and expect the upgrade needed.
        connection1 = std::make_unique<TestDatabaseConnection>(
            context_impl_->IDBTaskRunner(), url::Origin::Create(GURL(kOrigin)),
            base::UTF8ToUTF16(kDatabaseName), kDBVersion, kTransactionId);

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
            context_impl_->IDBTaskRunner(), ToOrigin(kOrigin),
            base::UTF8ToUTF16(kDatabaseName), kDBVersion, 0);

        // Check that we're called in order and the second connection gets it's
        // database after the first connection completes.
        ::testing::InSequence dummy;
        EXPECT_CALL(*connection1->connection_callbacks,
                    Complete(kTransactionId))
            .Times(1)
            .WillOnce(RunClosure(quit_closure2));
        EXPECT_CALL(
            *connection1->open_callbacks,
            MockedSuccessDatabase(IsAssociatedInterfacePtrInfoValid(false), _))
            .Times(1)
            .WillOnce(RunClosure(quit_closure2));
        EXPECT_CALL(
            *connection2->open_callbacks,
            MockedSuccessDatabase(IsAssociatedInterfacePtrInfoValid(true), _))
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
            kObjectStoreId, base::UTF8ToUTF16(kObjectStoreName),
            blink::IndexedDBKeyPath(), false);
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
  const char kObjectStoreName[] = "os";
  std::unique_ptr<TestDatabaseConnection> connection;
  IndexedDBDatabaseMetadata metadata;
  mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_database;

  base::RunLoop loop;
  context_impl_->IDBTaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        // Open connection.
        connection = std::make_unique<TestDatabaseConnection>(
            context_impl_->IDBTaskRunner(), url::Origin::Create(GURL(kOrigin)),
            base::UTF8ToUTF16(kDatabaseName), kDBVersion, kTransactionId);

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
            kObjectStoreId, base::UTF8ToUTF16(kObjectStoreName),
            blink::IndexedDBKeyPath(), false);
        // Call Put with an invalid blob.
        std::vector<blink::mojom::IDBExternalObjectPtr> external_objects;
        mojo::PendingRemote<blink::mojom::Blob> blob;
        // Ignore the result of InitWithNewPipeAndPassReceiver, to end up with
        // an invalid blob.
        ignore_result(blob.InitWithNewPipeAndPassReceiver());
        external_objects.push_back(
            blink::mojom::IDBExternalObject::NewBlobOrFile(
                blink::mojom::IDBBlobInfo::New(std::move(blob), "fakeUUID",
                                               base::string16(), 100,
                                               nullptr)));

        std::string value = "hello";
        const char* value_data = value.data();
        std::vector<uint8_t> value_vector(value_data,
                                          value_data + value.length());

        auto new_value = blink::mojom::IDBValue::New();
        new_value->bits = std::move(value_vector);
        new_value->external_objects = std::move(external_objects);

        connection->version_change_transaction->Put(
            kObjectStoreId, std::move(new_value),
            IndexedDBKey(base::UTF8ToUTF16("hello")),
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

// Disabled for crbug.com/945627.
TEST_F(IndexedDBDispatcherHostTest, DISABLED_CompactDatabaseWithConnection) {
  const int64_t kDBVersion = 1;
  const int64_t kTransactionId = 1;
  std::unique_ptr<TestDatabaseConnection> connection;
  IndexedDBDatabaseMetadata metadata;
  mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_database;

  base::RunLoop loop;
  context_impl_->IDBTaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        // Open connection.
        connection = std::make_unique<TestDatabaseConnection>(
            context_impl_->IDBTaskRunner(), ToOrigin(kOrigin),
            base::UTF8ToUTF16(kDatabaseName), kDBVersion, kTransactionId);
        EXPECT_CALL(*connection->open_callbacks,
                    MockedUpgradeNeeded(IsAssociatedInterfacePtrInfoValid(true),
                                        IndexedDBDatabaseMetadata::NO_VERSION,
                                        blink::mojom::IDBDataLoss::None,
                                        std::string(), _))
            .WillOnce(testing::DoAll(MoveArgPointee<0>(&pending_database),
                                     testing::SaveArg<4>(&metadata),
                                     QuitLoop(&loop)));

        // Queue open request message.
        connection->Open(idb_mojo_factory_.get());
      }));
  loop.Run();

  EXPECT_TRUE(pending_database.is_valid());
  EXPECT_EQ(connection->version, metadata.version);
  EXPECT_EQ(connection->db_name, metadata.name);

  blink::mojom::IDBStatus callback_result = blink::mojom::IDBStatus::IOError;

  base::RunLoop loop2;
  base::RepeatingClosure quit_closure2 =
      base::BarrierClosure(3, loop2.QuitClosure());
  context_impl_->IDBTaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        ::testing::InSequence dummy;

        EXPECT_CALL(*connection->connection_callbacks, Complete(kTransactionId))
            .Times(1)
            .WillOnce(RunClosure(quit_closure2));
        EXPECT_CALL(
            *connection->open_callbacks,
            MockedSuccessDatabase(IsAssociatedInterfacePtrInfoValid(false), _))
            .Times(1)
            .WillOnce(RunClosure(quit_closure2));

        connection->database.Bind(std::move(pending_database));
        ASSERT_TRUE(connection->database.is_bound());
        ASSERT_TRUE(connection->version_change_transaction.is_bound());

        connection->version_change_transaction->Commit(0);
        idb_mojo_factory_->AbortTransactionsAndCompactDatabase(base::BindOnce(
            &TestStatusCallback, std::move(quit_closure2), &callback_result));
      }));
  loop2.Run();
  EXPECT_EQ(blink::mojom::IDBStatus::OK, callback_result);

  base::RunLoop loop3;
  context_impl_->IDBTaskRunner()->PostTask(FROM_HERE,
                                           base::BindLambdaForTesting([&]() {
                                             connection.reset();
                                             loop3.Quit();
                                           }));
  loop3.Run();
}

TEST_F(IndexedDBDispatcherHostTest, CompactDatabaseWhileDoingTransaction) {
  const int64_t kDBVersion = 1;
  const int64_t kTransactionId = 1;
  const int64_t kObjectStoreId = 10;
  const char kObjectStoreName[] = "os";
  std::unique_ptr<TestDatabaseConnection> connection;
  IndexedDBDatabaseMetadata metadata;
  mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_database;

  base::RunLoop loop;
  context_impl_->IDBTaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        // Open connection.
        connection = std::make_unique<TestDatabaseConnection>(
            context_impl_->IDBTaskRunner(), ToOrigin(kOrigin),
            base::UTF8ToUTF16(kDatabaseName), kDBVersion, kTransactionId);
        EXPECT_CALL(*connection->open_callbacks,
                    MockedUpgradeNeeded(IsAssociatedInterfacePtrInfoValid(true),
                                        IndexedDBDatabaseMetadata::NO_VERSION,
                                        blink::mojom::IDBDataLoss::None,
                                        std::string(), _))
            .WillOnce(testing::DoAll(MoveArgPointee<0>(&pending_database),
                                     testing::SaveArg<4>(&metadata),
                                     QuitLoop(&loop)));

        // Queue open request message.
        connection->Open(idb_mojo_factory_.get());
      }));
  loop.Run();

  EXPECT_TRUE(pending_database.is_valid());
  EXPECT_EQ(connection->version, metadata.version);
  EXPECT_EQ(connection->db_name, metadata.name);

  blink::mojom::IDBStatus callback_result = blink::mojom::IDBStatus::IOError;

  base::RunLoop loop2;
  base::RepeatingClosure quit_closure2 =
      base::BarrierClosure(4, loop2.QuitClosure());
  context_impl_->IDBTaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        ::testing::InSequence dummy;

        EXPECT_CALL(
            *connection->connection_callbacks,
            Abort(kTransactionId, blink::mojom::IDBException::kUnknownError, _))
            .Times(1)
            .WillOnce(RunClosure(quit_closure2));
        EXPECT_CALL(*connection->open_callbacks,
                    Error(blink::mojom::IDBException::kAbortError, _))
            .Times(1)
            .WillOnce(RunClosure(quit_closure2));
        EXPECT_CALL(*connection->connection_callbacks, ForcedClose())
            .Times(1)
            .WillOnce(RunClosure(quit_closure2));

        connection->database.Bind(std::move(pending_database));
        ASSERT_TRUE(connection->database.is_bound());
        ASSERT_TRUE(connection->version_change_transaction.is_bound());
        connection->version_change_transaction->CreateObjectStore(
            kObjectStoreId, base::UTF8ToUTF16(kObjectStoreName),
            blink::IndexedDBKeyPath(), false);
        idb_mojo_factory_->AbortTransactionsAndCompactDatabase(base::BindOnce(
            &TestStatusCallback, std::move(quit_closure2), &callback_result));
      }));
  loop2.Run();

  EXPECT_EQ(blink::mojom::IDBStatus::OK, callback_result);

  base::RunLoop loop3;
  context_impl_->IDBTaskRunner()->PostTask(FROM_HERE,
                                           base::BindLambdaForTesting([&]() {
                                             connection.reset();
                                             loop3.Quit();
                                           }));
  loop3.Run();
}

TEST_F(IndexedDBDispatcherHostTest, CompactDatabaseWhileUpgrading) {
  const int64_t kDBVersion = 1;
  const int64_t kTransactionId = 1;
  std::unique_ptr<TestDatabaseConnection> connection;
  IndexedDBDatabaseMetadata metadata;
  mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_database;

  base::RunLoop loop;
  context_impl_->IDBTaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        // Open connection.
        connection = std::make_unique<TestDatabaseConnection>(
            context_impl_->IDBTaskRunner(), ToOrigin(kOrigin),
            base::UTF8ToUTF16(kDatabaseName), kDBVersion, kTransactionId);
        EXPECT_CALL(*connection->open_callbacks,
                    MockedUpgradeNeeded(IsAssociatedInterfacePtrInfoValid(true),
                                        IndexedDBDatabaseMetadata::NO_VERSION,
                                        blink::mojom::IDBDataLoss::None,
                                        std::string(), _))
            .WillOnce(testing::DoAll(MoveArgPointee<0>(&pending_database),
                                     testing::SaveArg<4>(&metadata),
                                     QuitLoop(&loop)));

        // Queue open request message.
        connection->Open(idb_mojo_factory_.get());
      }));
  loop.Run();

  EXPECT_TRUE(pending_database.is_valid());
  EXPECT_EQ(connection->version, metadata.version);
  EXPECT_EQ(connection->db_name, metadata.name);

  blink::mojom::IDBStatus callback_result = blink::mojom::IDBStatus::IOError;

  base::RunLoop loop2;
  base::RepeatingClosure quit_closure2 =
      base::BarrierClosure(4, loop2.QuitClosure());
  context_impl_->IDBTaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        ::testing::InSequence dummy;

        EXPECT_CALL(
            *connection->connection_callbacks,
            Abort(kTransactionId, blink::mojom::IDBException::kUnknownError, _))
            .Times(1)
            .WillOnce(RunClosure(quit_closure2));
        EXPECT_CALL(*connection->open_callbacks,
                    Error(blink::mojom::IDBException::kAbortError, _))
            .Times(1)
            .WillOnce(RunClosure(quit_closure2));
        EXPECT_CALL(*connection->connection_callbacks, ForcedClose())
            .Times(1)
            .WillOnce(RunClosure(quit_closure2));

        connection->database.Bind(std::move(pending_database));
        ASSERT_TRUE(connection->database.is_bound());
        ASSERT_TRUE(connection->version_change_transaction.is_bound());
        idb_mojo_factory_->AbortTransactionsAndCompactDatabase(base::BindOnce(
            &TestStatusCallback, std::move(quit_closure2), &callback_result));
      }));
  loop2.Run();

  EXPECT_EQ(blink::mojom::IDBStatus::OK, callback_result);

  base::RunLoop loop3;
  context_impl_->IDBTaskRunner()->PostTask(FROM_HERE,
                                           base::BindLambdaForTesting([&]() {
                                             connection.reset();
                                             loop3.Quit();
                                           }));
  loop3.Run();
}

TEST_F(IndexedDBDispatcherHostTest,
       AbortTransactionsAfterCompletingTransaction) {
  const int64_t kDBVersion = 1;
  const int64_t kTransactionId = 1;
  std::unique_ptr<TestDatabaseConnection> connection;
  IndexedDBDatabaseMetadata metadata;
  mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_database;

  base::RunLoop loop;
  context_impl_->IDBTaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        // Open connection.
        connection = std::make_unique<TestDatabaseConnection>(
            context_impl_->IDBTaskRunner(), ToOrigin(kOrigin),
            base::UTF8ToUTF16(kDatabaseName), kDBVersion, kTransactionId);
        {
          EXPECT_CALL(*connection->open_callbacks,
                      MockedUpgradeNeeded(
                          IsAssociatedInterfacePtrInfoValid(true),
                          IndexedDBDatabaseMetadata::NO_VERSION,
                          blink::mojom::IDBDataLoss::None, std::string(), _))
              .WillOnce(testing::DoAll(MoveArgPointee<0>(&pending_database),
                                       testing::SaveArg<4>(&metadata),
                                       QuitLoop(&loop)));

          // Queue open request message.
          connection->Open(idb_mojo_factory_.get());
        }
      }));
  loop.Run();

  EXPECT_TRUE(pending_database.is_valid());
  EXPECT_EQ(connection->version, metadata.version);
  EXPECT_EQ(connection->db_name, metadata.name);

  blink::mojom::IDBStatus callback_result = blink::mojom::IDBStatus::IOError;

  base::RunLoop loop2;
  base::RepeatingClosure quit_closure2 =
      base::BarrierClosure(4, loop2.QuitClosure());
  context_impl_->IDBTaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        ::testing::InSequence dummy;
        EXPECT_CALL(*connection->connection_callbacks, Complete(kTransactionId))
            .Times(1)
            .WillOnce(RunClosure(quit_closure2));
        EXPECT_CALL(
            *connection->open_callbacks,
            MockedSuccessDatabase(IsAssociatedInterfacePtrInfoValid(false), _))
            .Times(1)
            .WillOnce(RunClosure(quit_closure2));
        EXPECT_CALL(*connection->connection_callbacks, ForcedClose())
            .Times(1)
            .WillOnce(RunClosure(quit_closure2));

        connection->database.Bind(std::move(pending_database));
        ASSERT_TRUE(connection->database.is_bound());
        ASSERT_TRUE(connection->version_change_transaction.is_bound());
        connection->version_change_transaction->Commit(0);
        idb_mojo_factory_->AbortTransactionsForDatabase(base::BindOnce(
            &TestStatusCallback, std::move(quit_closure2), &callback_result));
      }));
  loop2.Run();

  EXPECT_EQ(blink::mojom::IDBStatus::OK, callback_result);

  base::RunLoop loop3;
  context_impl_->IDBTaskRunner()->PostTask(FROM_HERE,
                                           base::BindLambdaForTesting([&]() {
                                             connection.reset();
                                             loop3.Quit();
                                           }));
  loop3.Run();
}

TEST_F(IndexedDBDispatcherHostTest, AbortTransactionsWhileDoingTransaction) {
  const int64_t kDBVersion = 1;
  const int64_t kTransactionId = 1;
  const int64_t kObjectStoreId = 10;
  const char kObjectStoreName[] = "os";
  std::unique_ptr<TestDatabaseConnection> connection;
  IndexedDBDatabaseMetadata metadata;
  mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_database;

  base::RunLoop loop;
  context_impl_->IDBTaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        // Open connection.
        connection = std::make_unique<TestDatabaseConnection>(
            context_impl_->IDBTaskRunner(), ToOrigin(kOrigin),
            base::UTF8ToUTF16(kDatabaseName), kDBVersion, kTransactionId);

        EXPECT_CALL(*connection->open_callbacks,
                    MockedUpgradeNeeded(IsAssociatedInterfacePtrInfoValid(true),
                                        IndexedDBDatabaseMetadata::NO_VERSION,
                                        blink::mojom::IDBDataLoss::None,
                                        std::string(), _))
            .WillOnce(testing::DoAll(MoveArgPointee<0>(&pending_database),
                                     testing::SaveArg<4>(&metadata),
                                     QuitLoop(&loop)));

        // Queue open request message.
        connection->Open(idb_mojo_factory_.get());
      }));
  loop.Run();

  EXPECT_TRUE(pending_database.is_valid());
  EXPECT_EQ(connection->version, metadata.version);
  EXPECT_EQ(connection->db_name, metadata.name);

  blink::mojom::IDBStatus callback_result = blink::mojom::IDBStatus::IOError;

  base::RunLoop loop2;
  base::RepeatingClosure quit_closure2 =
      base::BarrierClosure(4, loop2.QuitClosure());
  context_impl_->IDBTaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        ::testing::InSequence dummy;

        EXPECT_CALL(
            *connection->connection_callbacks,
            Abort(kTransactionId, blink::mojom::IDBException::kUnknownError, _))
            .Times(1)
            .WillOnce(RunClosure(quit_closure2));
        EXPECT_CALL(*connection->open_callbacks,
                    Error(blink::mojom::IDBException::kAbortError, _))
            .Times(1)
            .WillOnce(RunClosure(quit_closure2));
        EXPECT_CALL(*connection->connection_callbacks, ForcedClose())
            .Times(1)
            .WillOnce(RunClosure(quit_closure2));

        connection->database.Bind(std::move(pending_database));
        ASSERT_TRUE(connection->database.is_bound());
        ASSERT_TRUE(connection->version_change_transaction.is_bound());
        connection->version_change_transaction->CreateObjectStore(
            kObjectStoreId, base::UTF8ToUTF16(kObjectStoreName),
            blink::IndexedDBKeyPath(), false);
        idb_mojo_factory_->AbortTransactionsForDatabase(base::BindOnce(
            &TestStatusCallback, std::move(quit_closure2), &callback_result));
      }));
  loop2.Run();

  EXPECT_EQ(blink::mojom::IDBStatus::OK, callback_result);

  base::RunLoop loop3;
  context_impl_->IDBTaskRunner()->PostTask(FROM_HERE,
                                           base::BindLambdaForTesting([&]() {
                                             connection.reset();
                                             loop3.Quit();
                                           }));
  loop3.Run();
}

TEST_F(IndexedDBDispatcherHostTest, AbortTransactionsWhileUpgrading) {
  const int64_t kDBVersion = 1;
  const int64_t kTransactionId = 1;
  std::unique_ptr<TestDatabaseConnection> connection;
  IndexedDBDatabaseMetadata metadata;
  mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_database;

  base::RunLoop loop;
  context_impl_->IDBTaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        // Open connection.
        connection = std::make_unique<TestDatabaseConnection>(
            context_impl_->IDBTaskRunner(), ToOrigin(kOrigin),
            base::UTF8ToUTF16(kDatabaseName), kDBVersion, kTransactionId);

        EXPECT_CALL(*connection->open_callbacks,
                    MockedUpgradeNeeded(IsAssociatedInterfacePtrInfoValid(true),
                                        IndexedDBDatabaseMetadata::NO_VERSION,
                                        blink::mojom::IDBDataLoss::None,
                                        std::string(), _))
            .WillOnce(testing::DoAll(MoveArgPointee<0>(&pending_database),
                                     testing::SaveArg<4>(&metadata),
                                     QuitLoop(&loop)));

        // Queue open request message.
        connection->Open(idb_mojo_factory_.get());
      }));
  loop.Run();

  EXPECT_TRUE(pending_database.is_valid());
  EXPECT_EQ(connection->version, metadata.version);
  EXPECT_EQ(connection->db_name, metadata.name);

  blink::mojom::IDBStatus callback_result = blink::mojom::IDBStatus::IOError;

  base::RunLoop loop2;
  base::RepeatingClosure quit_closure2 =
      base::BarrierClosure(4, loop2.QuitClosure());
  context_impl_->IDBTaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        ::testing::InSequence dummy;

        EXPECT_CALL(
            *connection->connection_callbacks,
            Abort(kTransactionId, blink::mojom::IDBException::kUnknownError, _))
            .Times(1)
            .WillOnce(RunClosure(quit_closure2));
        EXPECT_CALL(*connection->open_callbacks,
                    Error(blink::mojom::IDBException::kAbortError, _))
            .Times(1)
            .WillOnce(RunClosure(quit_closure2));
        EXPECT_CALL(*connection->connection_callbacks, ForcedClose())
            .Times(1)
            .WillOnce(RunClosure(quit_closure2));

        connection->database.Bind(std::move(pending_database));
        ASSERT_TRUE(connection->database.is_bound());
        ASSERT_TRUE(connection->version_change_transaction.is_bound());
        idb_mojo_factory_->AbortTransactionsForDatabase(base::BindOnce(
            &TestStatusCallback, std::move(quit_closure2), &callback_result));
      }));
  loop2.Run();

  EXPECT_EQ(blink::mojom::IDBStatus::OK, callback_result);

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
  const char kObjectStoreName[] = "os";
  const char kIndexName[] = "index";

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
              context_impl_->IDBTaskRunner(), ToOrigin(kOrigin),
              base::UTF8ToUTF16(kDatabaseName), kDBVersion1, kTransactionId1);

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
          EXPECT_CALL(*connection1->open_callbacks,
                      MockedSuccessDatabase(
                          IsAssociatedInterfacePtrInfoValid(false), _))
              .Times(1)
              .WillOnce(RunClosure(std::move(quit_closure)));

          ASSERT_TRUE(connection1->database.is_bound());
          connection1->version_change_transaction->CreateObjectStore(
              kObjectStoreId, base::UTF8ToUTF16(kObjectStoreName),
              blink::IndexedDBKeyPath(), false);
          connection1->database->CreateIndex(
              kTransactionId1, kObjectStoreId, kIndexId,
              base::UTF8ToUTF16(kIndexName), blink::IndexedDBKeyPath(), false,
              false);
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
              url::Origin::Create(GURL(kOrigin)),
              base::UTF8ToUTF16(kDatabaseName), kDBVersion2, kTransactionId2);

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
          EXPECT_CALL(*connection2->open_callbacks,
                      MockedSuccessDatabase(
                          IsAssociatedInterfacePtrInfoValid(false), _))
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
              context_impl_->IDBTaskRunner(), ToOrigin(kOrigin),
              base::UTF8ToUTF16(kDatabaseName), kDBVersion3, kTransactionId3);

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
          EXPECT_CALL(*connection3->open_callbacks,
                      MockedSuccessDatabase(
                          IsAssociatedInterfacePtrInfoValid(false), _))
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
  const char kObjectStoreName[] = "os";

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
            base::UTF8ToUTF16(kDatabaseName), kDBVersion1, kTransactionId1);

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
            MockedSuccessDatabase(IsAssociatedInterfacePtrInfoValid(false), _))
            .Times(1)
            .WillOnce(RunClosure(std::move(quit_closure2)));

        connection1->database.Bind(std::move(pending_database1));
        ASSERT_TRUE(connection1->database.is_bound());
        ASSERT_TRUE(connection1->version_change_transaction.is_bound());
        connection1->version_change_transaction->CreateObjectStore(
            kObjectStoreId, base::UTF8ToUTF16(kObjectStoreName),
            blink::IndexedDBKeyPath(), false);

        std::string value = "value";
        const char* value_data = value.data();
        std::vector<uint8_t> value_vector(value_data,
                                          value_data + value.length());

        auto new_value = blink::mojom::IDBValue::New();
        new_value->bits = std::move(value_vector);

        connection1->version_change_transaction->Put(
            kObjectStoreId, std::move(new_value),
            IndexedDBKey(base::UTF8ToUTF16("key")),
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
            context_impl_->IDBTaskRunner(), ToOrigin(kOrigin),
            base::UTF8ToUTF16(kDatabaseName), kDBVersion2, kTransactionId2);

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

  std::unique_ptr<StrictMock<MockMojoIndexedDBCallbacks>> clear_callbacks;

  // Clear object store.
  base::RunLoop loop5;
  base::RepeatingClosure quit_closure5 =
      base::BarrierClosure(3, loop5.QuitClosure());
  context_impl_->IDBTaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        ::testing::InSequence dummy;

        clear_callbacks =
            std::make_unique<StrictMock<MockMojoIndexedDBCallbacks>>();

        EXPECT_CALL(*clear_callbacks, Success())
            .Times(1)
            .WillOnce(RunClosure(quit_closure5));
        EXPECT_CALL(*connection2->connection_callbacks,
                    Complete(kTransactionId2))
            .Times(1)
            .WillOnce(RunClosure(quit_closure5));
        EXPECT_CALL(
            *connection2->open_callbacks,
            MockedSuccessDatabase(IsAssociatedInterfacePtrInfoValid(false), _))
            .Times(1)
            .WillOnce(RunClosure(std::move(quit_closure5)));

        connection2->database.Bind(std::move(pending_database2));
        ASSERT_TRUE(connection2->database.is_bound());
        ASSERT_TRUE(connection2->version_change_transaction.is_bound());
        connection2->database->Clear(
            kTransactionId2, kObjectStoreId,
            clear_callbacks->CreateInterfacePtrAndBind());
        connection2->version_change_transaction->Commit(0);
      }));
  loop5.Run();

  // +2 list changed, one for the transaction, the other for the ~DatabaseImpl
  EXPECT_EQ(4, observer.notify_list_changed_count);
  EXPECT_EQ(2, observer.notify_content_changed_count);

  base::RunLoop loop6;
  context_impl_->IDBTaskRunner()->PostTask(FROM_HERE,
                                           base::BindLambdaForTesting([&]() {
                                             clear_callbacks.reset();
                                             connection2.reset();
                                             loop6.Quit();
                                           }));
  loop6.Run();
}

}  // namespace content
