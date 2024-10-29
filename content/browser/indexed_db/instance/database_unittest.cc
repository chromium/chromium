// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/database.h"

#include <stdint.h>

#include <optional>
#include <set>
#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/containers/span.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/updateable_sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_manager.h"
#include "components/services/storage/privileged/mojom/indexed_db_client_state_checker.mojom.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "content/browser/indexed_db/indexed_db_value.h"
#include "content/browser/indexed_db/instance/backing_store.h"
#include "content/browser/indexed_db/instance/bucket_context.h"
#include "content/browser/indexed_db/instance/connection.h"
#include "content/browser/indexed_db/instance/cursor.h"
#include "content/browser/indexed_db/instance/database_callbacks.h"
#include "content/browser/indexed_db/instance/factory_client.h"
#include "content/browser/indexed_db/instance/fake_transaction.h"
#include "content/browser/indexed_db/instance/mock_factory_client.h"
#include "content/browser/indexed_db/instance/transaction.h"
#include "content/browser/indexed_db/mock_mojo_indexed_db_database_callbacks.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "storage/browser/test/mock_quota_manager.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

using blink::IndexedDBDatabaseMetadata;
using blink::IndexedDBIndexKeys;
using blink::IndexedDBKey;
using blink::IndexedDBKeyPath;

namespace content::indexed_db {

namespace {
constexpr int64_t kTestObjectStoreId = 1001;
constexpr int64_t kTestIndexId = 2002;

// Contains a record's keys and value that tests use to populate the database.
struct TestIDBRecord {
  IndexedDBKey primary_key;
  IndexedDBValue value;
  // Optional. Tests may skip index creation.
  std::optional<IndexedDBKey> index_key;
};

// Contains the options used to create an object store.  Initializes members
// to reasonable defaults that tests may override.
struct TestObjectStoreParameters {
  int64_t object_store_id = 0;
  std::u16string name{u"store"};
  IndexedDBKeyPath key_path;
  bool auto_increment = false;
};

// Contains the options used to create an index.  Optional.  Test setup skips
// index creation when `index_id` is `kInvalidId`. Initializes members to
// reasonable defaults that tests may override.
struct TestIndexParameters {
  int64_t index_id = blink::IndexedDBIndexMetadata::kInvalidId;
  std::u16string name{u"index"};
  bool unique = false;
  bool multi_entry = false;
  IndexedDBKeyPath key_path;
  bool auto_increment = false;
};

// Describes how test setup should create and populate an object store and
// optionally an index.
struct TestDatabaseParameters {
  TestObjectStoreParameters object_store_parameters;
  TestIndexParameters index_parameters;
  std::vector<TestIDBRecord> records;
};

// Contains the arguments needed to call `Database::GetAllOperation`.
// Initializes members to reasonable defaults that tests may override.
struct TestGetAllParameters {
  blink::mojom::IDBGetAllResultType result_type =
      blink::mojom::IDBGetAllResultType::Keys;

  blink::IndexedDBKeyRange key_range;

  int64_t max_count = std::numeric_limits<int64_t>::max();

  blink::mojom::IDBCursorDirection direction =
      blink::mojom::IDBCursorDirection::Next;
};

// `Database::GetAllOperation` streams record results to a sink.  This fake
// implementation enables test to wait for all the results and inspect them.
class FakeGetAllResultSink final
    : public blink::mojom::IDBDatabaseGetAllResultSink {
 public:
  FakeGetAllResultSink() = default;

  ~FakeGetAllResultSink() final {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  FakeGetAllResultSink(const FakeGetAllResultSink&) = delete;
  FakeGetAllResultSink& operator=(const FakeGetAllResultSink&) = delete;

  blink::mojom::IDBError* GetError() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(is_done_);
    return error_.get();
  }

  const std::vector<blink::mojom::IDBRecordPtr>& GetResults() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(is_done_);
    return records_;
  }

  void BindReceiver(
      mojo::PendingAssociatedReceiver<blink::mojom::IDBDatabaseGetAllResultSink>
          receiver) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(!is_done_);
    receiver_.Bind(std::move(receiver));
  }

  void WaitForResults() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    run_loop_.Run();
    CHECK(is_done_);
  }

 private:
  void ReceiveResults(std::vector<blink::mojom::IDBRecordPtr> records,
                      bool done) final {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(!is_done_);

    if (records_.empty()) {
      records_ = std::move(records);
    } else {
      records_.reserve(records_.size() + records.size());
      for (auto& record : records) {
        records_.emplace_back(std::move(record));
      }
    }

    if (done) {
      is_done_ = true;
      run_loop_.Quit();
    }
  }

  void OnError(blink::mojom::IDBErrorPtr error) final {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(!is_done_);

    error_ = std::move(error);
    is_done_ = true;
    run_loop_.Quit();
  }

  SEQUENCE_CHECKER(sequence_checker_);

  mojo::AssociatedReceiver<blink::mojom::IDBDatabaseGetAllResultSink> receiver_{
      this};

  // Used to wait until all results have been received.
  base::RunLoop run_loop_;
  bool is_done_ = false;

  // Store results and error for later inspection.  Must not be accessed until
  // `is_done_` is true.
  std::vector<blink::mojom::IDBRecordPtr> records_;
  blink::mojom::IDBErrorPtr error_;
};

void ExpectEqualsIndexedDBKey(const IndexedDBKey& expected_primary_key,
                              const IndexedDBKey& actual_primary_key) {
  ASSERT_EQ(actual_primary_key.IsValid(), expected_primary_key.IsValid());

  if (expected_primary_key.IsValid()) {
    EXPECT_TRUE(actual_primary_key.Equals(expected_primary_key))
        << "Expected " << expected_primary_key.DebugString() << " but got "
        << actual_primary_key.DebugString();
  }
}

void ExpectEqualsOptionalIndexedDBKey(
    const std::optional<IndexedDBKey>& expected_primary_key,
    const std::optional<IndexedDBKey>& actual_primary_key) {
  ASSERT_EQ(actual_primary_key.has_value(), expected_primary_key.has_value());

  if (expected_primary_key.has_value()) {
    ASSERT_NO_FATAL_FAILURE(
        ExpectEqualsIndexedDBKey(*expected_primary_key, *actual_primary_key));
  }
}

void ExpectEqualsIDBReturnValuePtr(
    const blink::mojom::IDBReturnValuePtr& expected_return_value,
    const blink::mojom::IDBReturnValuePtr& actual_return_value) {
  ASSERT_EQ(actual_return_value.is_null(), expected_return_value.is_null());

  if (!expected_return_value.is_null()) {
    // Verify the value bits.
    ASSERT_EQ(actual_return_value->value.is_null(),
              expected_return_value->value.is_null());

    if (!expected_return_value->value.is_null()) {
      EXPECT_EQ(actual_return_value->value->bits,
                expected_return_value->value->bits);

      // Verify the external objects.
      EXPECT_EQ(actual_return_value->value->external_objects.size(),
                expected_return_value->value->external_objects.size());
    }

    // Verify the return value primary key and key path.
    ASSERT_NO_FATAL_FAILURE(ExpectEqualsIndexedDBKey(
        actual_return_value->primary_key, expected_return_value->primary_key));

    EXPECT_EQ(expected_return_value->key_path, actual_return_value->key_path);
  }
}

// Creates a `IDBReturnValuePtr` with the given bits. `primary_key`,
// `key_path` are optional, required only for object stores that generate keys.
// `external_objects` is not set and remains empty.
blink::mojom::IDBReturnValuePtr CreateIDBReturnValuePtr(
    const std::string& bits,
    IndexedDBKey primary_key = {},
    IndexedDBKeyPath key_path = {}) {
  blink::mojom::IDBReturnValuePtr result = blink::mojom::IDBReturnValue::New();
  result->value = blink::mojom::IDBValue::New();
  result->value->bits.assign(bits.begin(), bits.end());
  result->primary_key = std::move(primary_key);
  result->key_path = std::move(key_path);
  return result;
}

}  // namespace

class DatabaseTest : public ::testing::Test {
 public:
  DatabaseTest() = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    quota_manager_ = base::MakeRefCounted<storage::MockQuotaManager>(
        /*is_incognito=*/false, temp_dir_.GetPath(),
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        /*special_storage_policy=*/nullptr);

    quota_manager_proxy_ = base::MakeRefCounted<storage::MockQuotaManagerProxy>(
        quota_manager_.get(),
        base::SingleThreadTaskRunner::GetCurrentDefault().get());

    BucketContext::Delegate delegate;
    delegate.on_ready_for_destruction =
        base::BindOnce(&DatabaseTest::OnBucketContextReadyForDestruction,
                       weak_factory_.GetWeakPtr());

    bucket_context_ = std::make_unique<BucketContext>(
        storage::BucketInfo(), temp_dir_.GetPath(), std::move(delegate),
        scoped_refptr<base::UpdateableSequencedTaskRunner>(),
        quota_manager_proxy_,
        /*blob_storage_context=*/mojo::NullRemote(),
        /*file_system_access_context=*/mojo::NullRemote());

    bucket_context_->InitBackingStoreIfNeeded(true);
    db_ = bucket_context_->AddDatabase(
        u"db", std::make_unique<Database>(u"db", *bucket_context_,
                                          Database::Identifier()));
  }

  void TearDown() override { db_ = nullptr; }

  void OnBucketContextReadyForDestruction() { bucket_context_.reset(); }

  void RunPostedTasks() {
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

 protected:
  base::test::TaskEnvironment task_environment_;

  base::ScopedTempDir temp_dir_;
  std::unique_ptr<BucketContext> bucket_context_;
  scoped_refptr<storage::MockQuotaManager> quota_manager_;
  scoped_refptr<storage::MockQuotaManagerProxy> quota_manager_proxy_;

  // As this is owned by `bucket_context_`, tests that cause the database to
  // be destroyed must manually reset this to null to avoid triggering dangling
  // pointer warnings.
  raw_ptr<Database> db_ = nullptr;

  base::WeakPtrFactory<DatabaseTest> weak_factory_{this};
};

TEST_F(DatabaseTest, ConnectionLifecycle) {
  MockMojoDatabaseCallbacks database_callbacks;
  MockFactoryClient request1;
  const int64_t transaction_id1 = 1;
  auto connection1 = std::make_unique<PendingConnection>(
      std::make_unique<ThunkFactoryClient>(request1),
      std::make_unique<DatabaseCallbacks>(
          database_callbacks.BindNewEndpointAndPassDedicatedRemote()),
      transaction_id1, IndexedDBDatabaseMetadata::DEFAULT_VERSION,
      mojo::NullAssociatedReceiver());
  db_->ScheduleOpenConnection(std::move(connection1));
  RunPostedTasks();

  MockMojoDatabaseCallbacks database_callbacks2;
  MockFactoryClient request2;
  const int64_t transaction_id2 = 2;
  auto connection2 = std::make_unique<PendingConnection>(
      std::make_unique<ThunkFactoryClient>(request2),
      std::make_unique<DatabaseCallbacks>(
          database_callbacks2.BindNewEndpointAndPassDedicatedRemote()),
      transaction_id2, IndexedDBDatabaseMetadata::DEFAULT_VERSION,
      mojo::NullAssociatedReceiver());
  db_->ScheduleOpenConnection(std::move(connection2));
  RunPostedTasks();
  db_ = nullptr;

  EXPECT_TRUE(request1.connection());
  request1.connection()->CloseAndReportForceClose();
  EXPECT_FALSE(request1.connection()->IsConnected());

  EXPECT_TRUE(request2.connection());
  request2.connection()->CloseAndReportForceClose();
  EXPECT_FALSE(request2.connection()->IsConnected());

  RunPostedTasks();

  EXPECT_TRUE(bucket_context_->GetDatabasesForTesting().empty());
}

TEST_F(DatabaseTest, ForcedClose) {
  MockMojoDatabaseCallbacks database_callbacks;
  MockFactoryClient request;
  const int64_t upgrade_transaction_id = 3;
  auto connection = std::make_unique<PendingConnection>(
      std::make_unique<ThunkFactoryClient>(request),
      std::make_unique<DatabaseCallbacks>(
          database_callbacks.BindNewEndpointAndPassDedicatedRemote()),
      upgrade_transaction_id, IndexedDBDatabaseMetadata::DEFAULT_VERSION,
      mojo::NullAssociatedReceiver());
  db_->ScheduleOpenConnection(std::move(connection));
  RunPostedTasks();

  EXPECT_EQ(db_, request.connection()->database().get());

  request.connection()->CreateTransaction(
      mojo::NullAssociatedReceiver(), /*transaction_id=*/123,
      /*object_store_ids=*/{}, blink::mojom::IDBTransactionMode::ReadOnly,
      blink::mojom::IDBTransactionDurability::Relaxed);
  db_ = nullptr;

  base::RunLoop run_loop;
  EXPECT_CALL(database_callbacks, ForcedClose)
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  request.connection()->CloseAndReportForceClose();
  run_loop.Run();
}

namespace {

class FakeFactoryClient : public FactoryClient {
 public:
  FakeFactoryClient() : FactoryClient(mojo::NullAssociatedRemote()) {}
  ~FakeFactoryClient() override = default;

  FakeFactoryClient(const FakeFactoryClient&) = delete;
  FakeFactoryClient& operator=(const FakeFactoryClient&) = delete;

  void OnBlocked(int64_t existing_version) override { blocked_called_ = true; }
  void OnDeleteSuccess(int64_t old_version) override { success_called_ = true; }
  void OnError(const DatabaseError& error) override { error_called_ = true; }

  bool blocked_called() const { return blocked_called_; }
  bool success_called() const { return success_called_; }
  bool error_called() const { return error_called_; }

 private:
  bool blocked_called_ = false;
  bool success_called_ = false;
  bool error_called_ = false;
};

}  // namespace

TEST_F(DatabaseTest, PendingDelete) {
  MockFactoryClient request1;
  const int64_t transaction_id1 = 1;
  MockMojoDatabaseCallbacks database_callbacks1;
  auto connection = std::make_unique<PendingConnection>(
      std::make_unique<ThunkFactoryClient>(request1),
      std::make_unique<DatabaseCallbacks>(
          database_callbacks1.BindNewEndpointAndPassDedicatedRemote()),
      transaction_id1, IndexedDBDatabaseMetadata::DEFAULT_VERSION,
      mojo::NullAssociatedReceiver());
  db_->ScheduleOpenConnection(std::move(connection));
  RunPostedTasks();

  EXPECT_EQ(db_->ConnectionCount(), 1UL);
  EXPECT_EQ(db_->ActiveOpenDeleteCount(), 0UL);
  EXPECT_EQ(db_->PendingOpenDeleteCount(), 0UL);

  base::RunLoop run_loop;
  FakeFactoryClient request2;
  db_->ScheduleDeleteDatabase(std::make_unique<ThunkFactoryClient>(request2),
                              run_loop.QuitClosure());
  RunPostedTasks();
  EXPECT_EQ(db_->ConnectionCount(), 1UL);
  EXPECT_EQ(db_->ActiveOpenDeleteCount(), 1UL);
  EXPECT_EQ(db_->PendingOpenDeleteCount(), 0UL);

  EXPECT_FALSE(request2.blocked_called());
  request1.connection()->VersionChangeIgnored();
  EXPECT_TRUE(request2.blocked_called());
  EXPECT_EQ(db_->ConnectionCount(), 1UL);
  EXPECT_EQ(db_->ActiveOpenDeleteCount(), 1UL);
  EXPECT_EQ(db_->PendingOpenDeleteCount(), 0UL);

  db_->ForceCloseAndRunTasks();
  db_ = nullptr;

  run_loop.Run();
  EXPECT_FALSE(db_);

  EXPECT_TRUE(request2.success_called());
}

TEST_F(DatabaseTest, OpenDeleteClear) {
  const int64_t kDatabaseVersion = 1;

  MockFactoryClient request1(
      /*expect_connection=*/true);
  MockMojoDatabaseCallbacks database_callbacks1;
  const int64_t transaction_id1 = 1;
  auto connection1 = std::make_unique<PendingConnection>(
      std::make_unique<ThunkFactoryClient>(request1),
      std::make_unique<DatabaseCallbacks>(
          database_callbacks1.BindNewEndpointAndPassDedicatedRemote()),
      transaction_id1, kDatabaseVersion, mojo::NullAssociatedReceiver());
  db_->ScheduleOpenConnection(std::move(connection1));
  RunPostedTasks();

  EXPECT_EQ(db_->ConnectionCount(), 1UL);
  EXPECT_EQ(db_->ActiveOpenDeleteCount(), 1UL);
  EXPECT_EQ(db_->PendingOpenDeleteCount(), 0UL);

  MockFactoryClient request2(
      /*expect_connection=*/false);
  MockMojoDatabaseCallbacks database_callbacks2;
  const int64_t transaction_id2 = 2;
  auto connection2 = std::make_unique<PendingConnection>(
      std::make_unique<ThunkFactoryClient>(request2),
      std::make_unique<DatabaseCallbacks>(
          database_callbacks2.BindNewEndpointAndPassDedicatedRemote()),
      transaction_id2, kDatabaseVersion, mojo::NullAssociatedReceiver());
  db_->ScheduleOpenConnection(std::move(connection2));
  RunPostedTasks();

  EXPECT_EQ(db_->ConnectionCount(), 1UL);
  EXPECT_EQ(db_->ActiveOpenDeleteCount(), 1UL);
  EXPECT_EQ(db_->PendingOpenDeleteCount(), 1UL);

  MockFactoryClient request3(
      /*expect_connection=*/false);
  MockMojoDatabaseCallbacks database_callbacks3;
  const int64_t transaction_id3 = 3;
  auto connection3 = std::make_unique<PendingConnection>(
      std::make_unique<ThunkFactoryClient>(request3),
      std::make_unique<DatabaseCallbacks>(
          database_callbacks3.BindNewEndpointAndPassDedicatedRemote()),
      transaction_id3, kDatabaseVersion, mojo::NullAssociatedReceiver());
  db_->ScheduleOpenConnection(std::move(connection3));
  RunPostedTasks();

  EXPECT_TRUE(request1.upgrade_called());

  EXPECT_EQ(db_->ConnectionCount(), 1UL);
  EXPECT_EQ(db_->ActiveOpenDeleteCount(), 1UL);
  EXPECT_EQ(db_->PendingOpenDeleteCount(), 2UL);

  EXPECT_CALL(database_callbacks1, ForcedClose);
  EXPECT_CALL(database_callbacks2, ForcedClose);
  EXPECT_CALL(database_callbacks3, ForcedClose);

  db_->ForceCloseAndRunTasks();
  db_ = nullptr;
  database_callbacks1.FlushForTesting();

  EXPECT_TRUE(request1.error_called());
  EXPECT_TRUE(request2.error_called());
  EXPECT_TRUE(request3.error_called());
}

TEST_F(DatabaseTest, ForceDelete) {
  MockFactoryClient request1;
  MockMojoDatabaseCallbacks database_callbacks;
  const int64_t transaction_id1 = 1;
  auto connection = std::make_unique<PendingConnection>(
      std::make_unique<ThunkFactoryClient>(request1),
      std::make_unique<DatabaseCallbacks>(
          database_callbacks.BindNewEndpointAndPassDedicatedRemote()),
      transaction_id1, IndexedDBDatabaseMetadata::DEFAULT_VERSION,
      mojo::NullAssociatedReceiver());
  db_->ScheduleOpenConnection(std::move(connection));
  RunPostedTasks();

  EXPECT_EQ(db_->ConnectionCount(), 1UL);
  EXPECT_EQ(db_->ActiveOpenDeleteCount(), 0UL);
  EXPECT_EQ(db_->PendingOpenDeleteCount(), 0UL);

  base::RunLoop run_loop;
  FakeFactoryClient request2;
  db_->ScheduleDeleteDatabase(std::make_unique<ThunkFactoryClient>(request2),
                              run_loop.QuitClosure());
  RunPostedTasks();
  EXPECT_FALSE(run_loop.AnyQuitCalled());
  db_->ForceCloseAndRunTasks();
  db_ = nullptr;
  run_loop.Run();
  EXPECT_FALSE(db_);
  EXPECT_FALSE(request2.blocked_called());
  EXPECT_TRUE(request2.success_called());
}

TEST_F(DatabaseTest, ForceCloseWhileOpenPending) {
  // Verify that pending connection requests are handled correctly during a
  // ForceClose.
  MockFactoryClient request1;
  MockMojoDatabaseCallbacks database_callbacks1;
  const int64_t transaction_id1 = 1;
  auto connection1 = std::make_unique<PendingConnection>(
      std::make_unique<ThunkFactoryClient>(request1),
      std::make_unique<DatabaseCallbacks>(
          database_callbacks1.BindNewEndpointAndPassDedicatedRemote()),
      transaction_id1, IndexedDBDatabaseMetadata::DEFAULT_VERSION,
      mojo::NullAssociatedReceiver());
  db_->ScheduleOpenConnection(std::move(connection1));
  RunPostedTasks();

  EXPECT_EQ(db_->ConnectionCount(), 1UL);
  EXPECT_EQ(db_->ActiveOpenDeleteCount(), 0UL);
  EXPECT_EQ(db_->PendingOpenDeleteCount(), 0UL);

  MockFactoryClient request2(/*expect_connection=*/false);
  MockMojoDatabaseCallbacks database_callbacks2;

  const int64_t transaction_id2 = 2;
  auto connection2 = std::make_unique<PendingConnection>(
      std::make_unique<ThunkFactoryClient>(request2),
      std::make_unique<DatabaseCallbacks>(
          database_callbacks2.BindNewEndpointAndPassDedicatedRemote()),
      transaction_id2, 3, mojo::NullAssociatedReceiver());
  db_->ScheduleOpenConnection(std::move(connection2));
  RunPostedTasks();

  EXPECT_EQ(db_->ConnectionCount(), 1UL);
  EXPECT_EQ(db_->ActiveOpenDeleteCount(), 1UL);
  EXPECT_EQ(db_->PendingOpenDeleteCount(), 0UL);

  db_->ForceCloseAndRunTasks();
  db_ = nullptr;
  RunPostedTasks();
  EXPECT_FALSE(db_);
}

TEST_F(DatabaseTest, ForceCloseWhileOpenAndDeletePending) {
  // Verify that pending connection requests are handled correctly during a
  // ForceClose.
  MockFactoryClient request1;
  MockMojoDatabaseCallbacks database_callbacks1;
  const int64_t transaction_id1 = 1;
  auto connection1 = std::make_unique<PendingConnection>(
      std::make_unique<ThunkFactoryClient>(request1),
      std::make_unique<DatabaseCallbacks>(
          database_callbacks1.BindNewEndpointAndPassDedicatedRemote()),
      transaction_id1, IndexedDBDatabaseMetadata::DEFAULT_VERSION,
      mojo::NullAssociatedReceiver());
  db_->ScheduleOpenConnection(std::move(connection1));
  RunPostedTasks();

  EXPECT_EQ(db_->ConnectionCount(), 1UL);
  EXPECT_EQ(db_->ActiveOpenDeleteCount(), 0UL);
  EXPECT_EQ(db_->PendingOpenDeleteCount(), 0UL);

  MockFactoryClient request2(false);
  MockMojoDatabaseCallbacks database_callbacks2;
  const int64_t transaction_id2 = 2;
  auto connection2 = std::make_unique<PendingConnection>(
      std::make_unique<ThunkFactoryClient>(request2),
      std::make_unique<DatabaseCallbacks>(
          database_callbacks2.BindNewEndpointAndPassDedicatedRemote()),
      transaction_id2, 3, mojo::NullAssociatedReceiver());
  db_->ScheduleOpenConnection(std::move(connection2));
  RunPostedTasks();

  base::RunLoop run_loop;
  auto request3 = std::make_unique<FakeFactoryClient>();
  db_->ScheduleDeleteDatabase(std::move(request3), run_loop.QuitClosure());
  RunPostedTasks();
  EXPECT_FALSE(run_loop.AnyQuitCalled());

  EXPECT_EQ(db_->ConnectionCount(), 1UL);
  EXPECT_EQ(db_->ActiveOpenDeleteCount(), 1UL);
  EXPECT_EQ(db_->PendingOpenDeleteCount(), 1UL);

  db_->ForceCloseAndRunTasks();
  db_ = nullptr;
  run_loop.Run();
}

Status DummyOperation(Transaction* transaction) {
  return Status::OK();
}

class DatabaseOperationTest : public DatabaseTest {
 public:
  DatabaseOperationTest() = default;
  DatabaseOperationTest(const DatabaseOperationTest&) = delete;
  DatabaseOperationTest& operator=(const DatabaseOperationTest&) = delete;

  void SetUp() override {
    DatabaseTest::SetUp();

    const int64_t transaction_id = 1;
    auto connection = std::make_unique<PendingConnection>(
        std::make_unique<ThunkFactoryClient>(request_),
        std::make_unique<DatabaseCallbacks>(mojo::NullAssociatedRemote()),
        transaction_id, IndexedDBDatabaseMetadata::DEFAULT_VERSION,
        mojo::NullAssociatedReceiver());
    db_->ScheduleOpenConnection(std::move(connection));
    RunPostedTasks();
    EXPECT_EQ(IndexedDBDatabaseMetadata::NO_VERSION, db_->metadata().version);

    EXPECT_TRUE(request_.connection());
    transaction_ = request_.connection()->CreateVersionChangeTransaction(
        transaction_id, /*scope=*/std::set<int64_t>(),
        new FakeTransaction(commit_success_,
                            blink::mojom::IDBTransactionMode::VersionChange,
                            bucket_context_->backing_store()->AsWeakPtr()));

    std::vector<PartitionedLockManager::PartitionedLockRequest> lock_requests =
        {{GetDatabaseLockId(db_->metadata().name),
          PartitionedLockManager::LockType::kExclusive}};
    db_->lock_manager().AcquireLocks(
        std::move(lock_requests), *transaction_->mutable_locks_receiver(),
        base::BindOnce(&Transaction::Start, transaction_->AsWeakPtr()));

    // Add a dummy task which takes the place of the VersionChangeOperation
    // which kicks off the upgrade. This ensures that the transaction has
    // processed at least one task before the CreateObjectStore call.
    transaction_->ScheduleTask(base::BindOnce(&DummyOperation));
    // Run posted tasks to execute the dummy operation and ensure that it is
    // stored in the connection.
    RunPostedTasks();
  }

  // Populates an object store and optionally an index with `database_records`.
  // After setup, calls `Database::GetAllOperation` with `get_all_parameters`.
  // Verifies that the results match `expected_results`.
  void TestGetAll(
      const TestDatabaseParameters& database_parameters,
      const TestGetAllParameters& get_all_parameters,
      base::span<const blink::mojom::IDBRecordPtr> expected_results) {
    // Create the object store.
    ASSERT_EQ(0u, db_->metadata().object_stores.size());
    const auto& object_store_parameters =
        database_parameters.object_store_parameters;
    const int64_t store_id = object_store_parameters.object_store_id;
    Status status = db_->CreateObjectStoreOperation(
        store_id, object_store_parameters.name,
        object_store_parameters.key_path,
        object_store_parameters.auto_increment, transaction_);
    EXPECT_TRUE(status.ok()) << status.ToString();
    EXPECT_EQ(1u, db_->metadata().object_stores.size());

    // Optionally, create an index when the test provides a valid index id.
    const auto& index_parameters = database_parameters.index_parameters;
    const int64_t index_id = index_parameters.index_id;
    const bool has_index =
        (index_id != blink::IndexedDBIndexMetadata::kInvalidId);
    if (has_index) {
      status = db_->CreateIndexOperation(
          store_id, index_parameters.index_id, index_parameters.name,
          index_parameters.key_path, index_parameters.unique,
          index_parameters.multi_entry, transaction_);
    }
    EXPECT_TRUE(status.ok()) << status.ToString();

    // Populate the object store and optionally the index with the provided
    // records.
    for (const TestIDBRecord& record : database_parameters.records) {
      std::vector<IndexedDBIndexKeys> index_keys;
      ASSERT_EQ(record.index_key.has_value(), has_index);
      if (has_index) {
        IndexedDBIndexKeys index_key{index_id, {*record.index_key}};
        index_keys.emplace_back(std::move(index_key));
      }

      testing::NiceMock<
          base::MockCallback<blink::mojom::IDBTransaction::PutCallback>>
          callback;

      // Set in-flight memory to a reasonably large number to prevent underflow
      // in `PutOperation`
      transaction_->in_flight_memory() += 1000;

      auto put_params = std::make_unique<Database::PutOperationParams>();
      put_params->object_store_id = store_id;
      put_params->value = record.value;
      put_params->key = std::make_unique<IndexedDBKey>(record.primary_key);
      put_params->put_mode = blink::mojom::IDBPutMode::AddOnly;
      put_params->callback = callback.Get();
      put_params->index_keys = std::move(index_keys);
      status = db_->PutOperation(std::move(put_params), transaction_);
      EXPECT_TRUE(status.ok()) << status.ToString();
    }

    // Call `Database::GetAllOperation` with the provided parameters.
    FakeGetAllResultSink result_sink;
    blink::mojom::IDBDatabase::GetAllCallback get_all_callback = base::BindOnce(
        &FakeGetAllResultSink::BindReceiver, base::Unretained(&result_sink));

    std::unique_ptr<Database::GetAllResultSinkWrapper> result_sink_wrapper =
        std::make_unique<Database::GetAllResultSinkWrapper>(
            transaction_->AsWeakPtr(), std::move(get_all_callback));
    result_sink_wrapper->UseDedicatedReceiverForTesting();

    std::unique_ptr<blink::IndexedDBKeyRange> key_range =
        std::make_unique<blink::IndexedDBKeyRange>(
            get_all_parameters.key_range);

    status = db_->GetAllOperation(store_id, index_id, std::move(key_range),
                                  get_all_parameters.result_type,
                                  get_all_parameters.max_count,
                                  get_all_parameters.direction,
                                  std::move(result_sink_wrapper), transaction_);
    EXPECT_TRUE(status.ok()) << status.ToString();

    result_sink.WaitForResults();
    EXPECT_EQ(result_sink.GetError(), nullptr);

    // Verify that the actual results match the expected results.
    const std::vector<blink::mojom::IDBRecordPtr>& actual_results =
        result_sink.GetResults();
    ASSERT_EQ(actual_results.size(), expected_results.size());

    for (size_t i = 0u; i < expected_results.size(); ++i) {
      ASSERT_FALSE(actual_results[i].is_null());

      // Verify the primary key.
      ASSERT_NO_FATAL_FAILURE(ExpectEqualsOptionalIndexedDBKey(
          expected_results[i]->primary_key, actual_results[i]->primary_key));

      // Verify the record value.
      ASSERT_NO_FATAL_FAILURE(ExpectEqualsIDBReturnValuePtr(
          expected_results[i]->return_value, actual_results[i]->return_value));

      // Verify the index key.
      ASSERT_NO_FATAL_FAILURE(ExpectEqualsOptionalIndexedDBKey(
          expected_results[i]->index_key, actual_results[i]->index_key));
    }

    // Perform cleanup.
    transaction_->SetCommitFlag();
    transaction_ = nullptr;
    RunPostedTasks();

    // A transaction error would have resulted in a deleted db.
    EXPECT_FALSE(bucket_context_->GetDatabasesForTesting().empty());
  }

 protected:
  MockFactoryClient request_;

  // As this is owned by `Connection`, tests that cause the transaction
  // to be committed must manually reset this to null to avoid triggering
  // dangling pointer warnings.
  raw_ptr<Transaction> transaction_ = nullptr;
  Status commit_success_;
};

TEST_F(DatabaseOperationTest, CreateObjectStore) {
  EXPECT_EQ(0ULL, db_->metadata().object_stores.size());
  const int64_t store_id = 1001;
  Status s =
      db_->CreateObjectStoreOperation(store_id, u"store", IndexedDBKeyPath(),
                                      /*auto_increment=*/false, transaction_);
  EXPECT_TRUE(s.ok());
  transaction_->SetCommitFlag();
  transaction_ = nullptr;
  RunPostedTasks();
  EXPECT_TRUE(bucket_context_);
  EXPECT_EQ(1ULL, db_->metadata().object_stores.size());
}

TEST_F(DatabaseOperationTest, CreateIndex) {
  EXPECT_EQ(0ULL, db_->metadata().object_stores.size());
  const int64_t store_id = 1001;
  Status s =
      db_->CreateObjectStoreOperation(store_id, u"store", IndexedDBKeyPath(),
                                      /*auto_increment=*/false, transaction_);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(1ULL, db_->metadata().object_stores.size());
  const int64_t index_id = 2002;
  s = db_->CreateIndexOperation(store_id, index_id, u"index",
                                IndexedDBKeyPath(), /*unique=*/false,
                                /*multi_entry=*/false, transaction_);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(
      1ULL,
      db_->metadata().object_stores.find(store_id)->second.indexes.size());
  transaction_->SetCommitFlag();
  transaction_ = nullptr;
  RunPostedTasks();
  EXPECT_TRUE(bucket_context_);
  EXPECT_EQ(1ULL, db_->metadata().object_stores.size());
  EXPECT_EQ(
      1ULL,
      db_->metadata().object_stores.find(store_id)->second.indexes.size());
}

class DatabaseOperationAbortTest : public DatabaseOperationTest {
 public:
  DatabaseOperationAbortTest() {
    commit_success_ = Status::NotFound("Bummer.");
  }

  DatabaseOperationAbortTest(const DatabaseOperationAbortTest&) = delete;
  DatabaseOperationAbortTest& operator=(const DatabaseOperationAbortTest&) =
      delete;
};

TEST_F(DatabaseOperationAbortTest, CreateObjectStore) {
  EXPECT_EQ(0ULL, db_->metadata().object_stores.size());
  const int64_t store_id = 1001;
  Status s =
      db_->CreateObjectStoreOperation(store_id, u"store", IndexedDBKeyPath(),
                                      /*auto_increment=*/false, transaction_);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(1ULL, db_->metadata().object_stores.size());
  db_ = nullptr;
  transaction_->SetCommitFlag();
  RunPostedTasks();
  // A transaction error results in a deleted db.
  EXPECT_TRUE(bucket_context_->GetDatabasesForTesting().empty());
}

TEST_F(DatabaseOperationAbortTest, CreateIndex) {
  EXPECT_EQ(0ULL, db_->metadata().object_stores.size());
  const int64_t store_id = 1001;
  Status s =
      db_->CreateObjectStoreOperation(store_id, u"store", IndexedDBKeyPath(),
                                      /*auto_increment=*/false, transaction_);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(1ULL, db_->metadata().object_stores.size());
  const int64_t index_id = 2002;
  s = db_->CreateIndexOperation(store_id, index_id, u"index",
                                IndexedDBKeyPath(), /*unique=*/false,
                                /*multi_entry=*/false, transaction_);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(
      1ULL,
      db_->metadata().object_stores.find(store_id)->second.indexes.size());
  db_ = nullptr;
  transaction_->SetCommitFlag();
  RunPostedTasks();
  // A transaction error results in a deleted db.
  EXPECT_TRUE(bucket_context_->GetDatabasesForTesting().empty());
}

TEST_F(DatabaseOperationTest, CreatePutDelete) {
  EXPECT_EQ(0ULL, db_->metadata().object_stores.size());
  const int64_t store_id = 1001;

  Status s =
      db_->CreateObjectStoreOperation(store_id, u"store", IndexedDBKeyPath(),
                                      /*auto_increment=*/false, transaction_);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(1ULL, db_->metadata().object_stores.size());

  IndexedDBValue value("value1", {});
  std::unique_ptr<IndexedDBKey> key(std::make_unique<IndexedDBKey>("key"));
  std::vector<IndexedDBIndexKeys> index_keys;
  base::MockCallback<blink::mojom::IDBTransaction::PutCallback> callback;

  // Set in-flight memory to a reasonably large number to prevent underflow in
  // `PutOperation`
  transaction_->in_flight_memory() += 1000;

  auto put_params = std::make_unique<Database::PutOperationParams>();
  put_params->object_store_id = store_id;
  put_params->value = value;
  put_params->key = std::move(key);
  put_params->put_mode = blink::mojom::IDBPutMode::AddOnly;
  put_params->callback = callback.Get();
  put_params->index_keys = index_keys;
  s = db_->PutOperation(std::move(put_params), transaction_);
  EXPECT_TRUE(s.ok());

  s = db_->DeleteObjectStoreOperation(store_id, transaction_);
  EXPECT_TRUE(s.ok());

  EXPECT_EQ(0ULL, db_->metadata().object_stores.size());

  transaction_->SetCommitFlag();
  transaction_ = nullptr;
  RunPostedTasks();
  // A transaction error would have resulted in a deleted db.
  EXPECT_FALSE(bucket_context_->GetDatabasesForTesting().empty());
  EXPECT_TRUE(s.ok());
}

TEST_F(DatabaseOperationTest, ObjectStoreGetAllKeysWhenEmpty) {
  ASSERT_NO_FATAL_FAILURE(TestGetAll(
      /*database_parameters=*/
      {
          /*object_store_parameters=*/
          {
              .object_store_id = kTestObjectStoreId,
          },
          /*index_parameters=*/{},
          /*database_records=*/{},
      },
      /*get_all_parameters=*/
      {
          .result_type = blink::mojom::IDBGetAllResultType::Keys,
      },
      /*expected_results=*/{}));
}

TEST_F(DatabaseOperationTest, IndexGetAllValuesWhenEmpty) {
  ASSERT_NO_FATAL_FAILURE(TestGetAll(
      /*database_parameters=*/
      {
          /*object_store_parameters=*/
          {
              .object_store_id = kTestObjectStoreId,
          },
          /*index_parameters=*/
          {
              .index_id = kTestIndexId,
          },
          /*database_records=*/{},
      },
      /*get_all_parameters=*/
      {
          .result_type = blink::mojom::IDBGetAllResultType::Values,
      },
      /*expected_results=*/{}));
}

TEST_F(DatabaseOperationTest, ObjectStoreGetAllKeys) {
  const blink::mojom::IDBRecordPtr expected_results[] = {
      blink::mojom::IDBRecord::New(IndexedDBKey{"key1"},
                                   /*value=*/nullptr,
                                   /*index_key=*/std::nullopt),
      blink::mojom::IDBRecord::New(IndexedDBKey{"key2"},
                                   /*value=*/nullptr,
                                   /*index_key=*/std::nullopt),
      blink::mojom::IDBRecord::New(IndexedDBKey{"key3"},
                                   /*value=*/nullptr,
                                   /*index_key=*/std::nullopt),
  };

  ASSERT_NO_FATAL_FAILURE(TestGetAll(
      /*database_parameters=*/
      {
          /*object_store_parameters=*/
          {
              .object_store_id = kTestObjectStoreId,
          },
          /*index_parameters=*/{},
          /*database_records=*/
          {
              {
                  IndexedDBKey{"key1"},
                  {"value1", /*external_objects=*/{}},
                  /*index_key=*/std::nullopt,
              },
              {
                  IndexedDBKey{"key2"},
                  {"value2", /*external_objects=*/{}},
                  /*index_key=*/std::nullopt,
              },
              {
                  IndexedDBKey{"key3"},
                  {"value3", /*external_objects=*/{}},
                  /*index_key=*/std::nullopt,
              },
          },
      },
      /*get_all_parameters=*/
      {
          .result_type = blink::mojom::IDBGetAllResultType::Keys,
      },
      expected_results));
}

TEST_F(DatabaseOperationTest, ObjectStoreGetAllValues) {
  const blink::mojom::IDBRecordPtr expected_results[] = {
      blink::mojom::IDBRecord::New(
          /*primary_key=*/std::nullopt,
          /*value=*/CreateIDBReturnValuePtr("value1"),
          /*index_key=*/std::nullopt),
      blink::mojom::IDBRecord::New(
          /*primary_key=*/std::nullopt,
          /*value=*/CreateIDBReturnValuePtr("value2"),
          /*index_key=*/std::nullopt),
      blink::mojom::IDBRecord::New(
          /*primary_key=*/std::nullopt,
          /*value=*/CreateIDBReturnValuePtr("value3"),
          /*index_key=*/std::nullopt),
  };

  ASSERT_NO_FATAL_FAILURE(TestGetAll(
      /*database_parameters=*/
      {
          /*object_store_parameters=*/
          {
              .object_store_id = kTestObjectStoreId,
          },
          /*index_parameters=*/{},
          /*database_records=*/
          {
              {
                  IndexedDBKey{"key1"},
                  {"value1", /*external_objects=*/{}},
                  /*index_key=*/std::nullopt,
              },
              {
                  IndexedDBKey{"key2"},
                  {"value2", /*external_objects=*/{}},
                  /*index_key=*/std::nullopt,
              },
              {
                  IndexedDBKey{"key3"},
                  {"value3", /*external_objects=*/{}},
                  /*index_key=*/std::nullopt,
              },
          },
      },
      /*get_all_parameters=*/
      {
          .result_type = blink::mojom::IDBGetAllResultType::Values,
      },
      expected_results));
}

TEST_F(DatabaseOperationTest, ObjectStoreGetAllRecords) {
  const blink::mojom::IDBRecordPtr expected_results[] = {
      blink::mojom::IDBRecord::New(IndexedDBKey{"key1"},
                                   /*value=*/CreateIDBReturnValuePtr("value1"),
                                   /*index_key=*/std::nullopt),
      blink::mojom::IDBRecord::New(IndexedDBKey{"key2"},
                                   /*value=*/CreateIDBReturnValuePtr("value2"),
                                   /*index_key=*/std::nullopt),
      blink::mojom::IDBRecord::New(IndexedDBKey{"key3"},
                                   /*value=*/CreateIDBReturnValuePtr("value3"),
                                   /*index_key=*/std::nullopt),
  };

  ASSERT_NO_FATAL_FAILURE(TestGetAll(
      /*database_parameters=*/
      {
          /*object_store_parameters=*/
          {
              .object_store_id = kTestObjectStoreId,
          },
          /*index_parameters=*/{},
          /*database_records=*/
          {
              {
                  IndexedDBKey{"key1"},
                  {"value1", /*external_objects=*/{}},
                  /*index_key=*/std::nullopt,
              },
              {
                  IndexedDBKey{"key2"},
                  {"value2", /*external_objects=*/{}},
                  /*index_key=*/std::nullopt,
              },
              {
                  IndexedDBKey{"key3"},
                  {"value3", /*external_objects=*/{}},
                  /*index_key=*/std::nullopt,
              },
          },
      },
      /*get_all_parameters=*/
      {
          .result_type = blink::mojom::IDBGetAllResultType::Records,
      },
      expected_results));
}

TEST_F(DatabaseOperationTest, IndexGetAllKeys) {
  const blink::mojom::IDBRecordPtr expected_results[] = {
      blink::mojom::IDBRecord::New(IndexedDBKey{"key3"},
                                   /*value=*/nullptr,
                                   /*index_key=*/std::nullopt),
      blink::mojom::IDBRecord::New(IndexedDBKey{"key2"},
                                   /*value=*/nullptr,
                                   /*index_key=*/std::nullopt),
      blink::mojom::IDBRecord::New(IndexedDBKey{"key1"},
                                   /*value=*/nullptr,
                                   /*index_key=*/std::nullopt),
  };

  ASSERT_NO_FATAL_FAILURE(TestGetAll(
      /*database_parameters=*/
      {
          /*object_store_parameters=*/
          {
              .object_store_id = kTestObjectStoreId,
          },
          /*index_parameters=*/
          {
              .index_id = kTestIndexId,
          },
          /*database_records=*/
          {
              {
                  IndexedDBKey{"key1"},
                  {"value1", /*external_objects=*/{}},
                  IndexedDBKey("index_key3"),
              },
              {
                  IndexedDBKey{"key2"},
                  {"value2", /*external_objects=*/{}},
                  IndexedDBKey("index_key2"),
              },
              {
                  IndexedDBKey{"key3"},
                  {"value3", /*external_objects=*/{}},
                  IndexedDBKey("index_key1"),
              },
          },
      },
      /*get_all_parameters=*/
      {
          .result_type = blink::mojom::IDBGetAllResultType::Keys,
      },
      expected_results));
}

TEST_F(DatabaseOperationTest, IndexGetAllValues) {
  const blink::mojom::IDBRecordPtr expected_results[] = {
      blink::mojom::IDBRecord::New(
          /*primary_key=*/std::nullopt,
          /*value=*/CreateIDBReturnValuePtr("value3"),
          /*index_key=*/std::nullopt),
      blink::mojom::IDBRecord::New(
          /*primary_key=*/std::nullopt,
          /*value=*/CreateIDBReturnValuePtr("value2"),
          /*index_key=*/std::nullopt),
      blink::mojom::IDBRecord::New(
          /*primary_key=*/std::nullopt,
          /*value=*/CreateIDBReturnValuePtr("value1"),
          /*index_key=*/std::nullopt),
  };

  ASSERT_NO_FATAL_FAILURE(TestGetAll(
      /*database_parameters=*/
      {
          /*object_store_parameters=*/
          {
              .object_store_id = kTestObjectStoreId,
          },
          /*index_parameters=*/
          {
              .index_id = kTestIndexId,
          },
          /*database_records=*/
          {
              {
                  IndexedDBKey{"key1"},
                  {"value1", /*external_objects=*/{}},
                  IndexedDBKey("index_key3"),
              },
              {
                  IndexedDBKey{"key2"},
                  {"value2", /*external_objects=*/{}},
                  IndexedDBKey("index_key2"),
              },
              {
                  IndexedDBKey{"key3"},
                  {"value3", /*external_objects=*/{}},
                  IndexedDBKey("index_key1"),
              },
          },
      },
      /*get_all_parameters=*/
      {
          .result_type = blink::mojom::IDBGetAllResultType::Values,
      },
      expected_results));
}

TEST_F(DatabaseOperationTest, IndexGetAllRecords) {
  const blink::mojom::IDBRecordPtr expected_results[] = {
      blink::mojom::IDBRecord::New(IndexedDBKey{"key3"},
                                   /*value=*/CreateIDBReturnValuePtr("value3"),
                                   IndexedDBKey{"index_key1"}),
      blink::mojom::IDBRecord::New(IndexedDBKey{"key2"},
                                   /*value=*/CreateIDBReturnValuePtr("value2"),
                                   IndexedDBKey{"index_key2"}),
      blink::mojom::IDBRecord::New(IndexedDBKey{"key1"},
                                   /*value=*/CreateIDBReturnValuePtr("value1"),
                                   IndexedDBKey{"index_key3"}),
  };

  ASSERT_NO_FATAL_FAILURE(TestGetAll(
      /*database_parameters=*/
      {
          /*object_store_parameters=*/
          {
              .object_store_id = kTestObjectStoreId,
          },
          /*index_parameters=*/
          {
              .index_id = kTestIndexId,
          },
          /*database_records=*/
          {
              {
                  IndexedDBKey{"key1"},
                  {"value1", /*external_objects=*/{}},
                  IndexedDBKey("index_key3"),
              },
              {
                  IndexedDBKey{"key2"},
                  {"value2", /*external_objects=*/{}},
                  IndexedDBKey("index_key2"),
              },
              {
                  IndexedDBKey{"key3"},
                  {"value3", /*external_objects=*/{}},
                  IndexedDBKey("index_key1"),
              },
          },
      },
      /*get_all_parameters=*/
      {
          .result_type = blink::mojom::IDBGetAllResultType::Records,
      },
      expected_results));
}

TEST_F(DatabaseOperationTest, ObjectStoreGetAllKeysWithRange) {
  const blink::mojom::IDBRecordPtr expected_results[] = {
      blink::mojom::IDBRecord::New(IndexedDBKey{"key2"},
                                   /*value=*/nullptr,
                                   /*index_key=*/std::nullopt),
      blink::mojom::IDBRecord::New(IndexedDBKey{"key3"},
                                   /*value=*/nullptr,
                                   /*index_key=*/std::nullopt),
  };

  ASSERT_NO_FATAL_FAILURE(TestGetAll(
      /*database_parameters=*/
      {
          /*object_store_parameters=*/
          {
              .object_store_id = kTestObjectStoreId,
          },
          /*index_parameters=*/{},
          /*database_records=*/
          {
              {
                  IndexedDBKey{"key1"},
                  {"value1", /*external_objects=*/{}},
                  /*index_key=*/std::nullopt,
              },
              {
                  IndexedDBKey{"key2"},
                  {"value2", /*external_objects=*/{}},
                  /*index_key=*/std::nullopt,
              },
              {
                  IndexedDBKey{"key3"},
                  {"value3", /*external_objects=*/{}},
                  /*index_key=*/std::nullopt,
              },
          },
      },
      /*get_all_parameters=*/
      {
          .result_type = blink::mojom::IDBGetAllResultType::Keys,
          .key_range =
              {
                  IndexedDBKey("key2"),
                  IndexedDBKey("key9"),
                  /*lower_open=*/false,
                  /*upper_open=*/false,
              },
      },
      expected_results));
}

TEST_F(DatabaseOperationTest, ObjectStoreGetAllKeysWithRangeThatDoesNotExist) {
  ASSERT_NO_FATAL_FAILURE(TestGetAll(
      /*database_parameters=*/
      {
          /*object_store_parameters=*/
          {
              .object_store_id = kTestObjectStoreId,
          },
          /*index_parameters=*/{},
          /*database_records=*/
          {
              {
                  IndexedDBKey{"key1"},
                  {"value1", /*external_objects=*/{}},
                  /*index_key=*/std::nullopt,
              },
              {
                  IndexedDBKey{"key2"},
                  {"value2", /*external_objects=*/{}},
                  /*index_key=*/std::nullopt,
              },
              {
                  IndexedDBKey{"key3"},
                  {"value3", /*external_objects=*/{}},
                  /*index_key=*/std::nullopt,
              },
          },
      },
      /*get_all_parameters=*/
      {
          .result_type = blink::mojom::IDBGetAllResultType::Keys,
          .key_range =
              {
                  IndexedDBKey("key7"),
                  IndexedDBKey("key9"),
                  /*lower_open=*/false,
                  /*upper_open=*/false,
              },
      },
      /*expected_results=*/{}));
}

TEST_F(DatabaseOperationTest, ObjectStoreGetAllKeysWithInvalidRange) {
  ASSERT_NO_FATAL_FAILURE(TestGetAll(
      /*database_parameters=*/
      {
          /*object_store_parameters=*/
          {
              .object_store_id = kTestObjectStoreId,
          },
          /*index_parameters=*/{},
          /*database_records=*/
          {
              {
                  IndexedDBKey{"key1"},
                  {"value1", /*external_objects=*/{}},
                  /*index_key=*/std::nullopt,
              },
              {
                  IndexedDBKey{"key2"},
                  {"value2", /*external_objects=*/{}},
                  /*index_key=*/std::nullopt,
              },
              {
                  IndexedDBKey{"key3"},
                  {"value3", /*external_objects=*/{}},
                  /*index_key=*/std::nullopt,
              },
          },
      },
      /*get_all_parameters=*/
      {
          .result_type = blink::mojom::IDBGetAllResultType::Keys,
          .key_range =
              {
                  IndexedDBKey("key9"),
                  IndexedDBKey("key7"),
                  /*lower_open=*/false,
                  /*upper_open=*/false,
              },
      },
      /*expected_results=*/{}));
}

TEST_F(DatabaseOperationTest, ObjectStoreGetAllKeysWithMaxCount) {
  const blink::mojom::IDBRecordPtr expected_results[] = {
      blink::mojom::IDBRecord::New(IndexedDBKey{"key1"},
                                   /*value=*/nullptr,
                                   /*index_key=*/std::nullopt),
      blink::mojom::IDBRecord::New(IndexedDBKey{"key2"},
                                   /*value=*/nullptr,
                                   /*index_key=*/std::nullopt),
  };

  ASSERT_NO_FATAL_FAILURE(TestGetAll(
      /*database_parameters=*/
      {
          /*object_store_parameters=*/
          {
              .object_store_id = kTestObjectStoreId,
          },
          /*index_parameters=*/{},
          /*database_records=*/
          {
              {
                  IndexedDBKey{"key1"},
                  {"value1", /*external_objects=*/{}},
                  /*index_key=*/std::nullopt,
              },
              {
                  IndexedDBKey{"key2"},
                  {"value2", /*external_objects=*/{}},
                  /*index_key=*/std::nullopt,
              },
              {
                  IndexedDBKey{"key3"},
                  {"value3", /*external_objects=*/{}},
                  /*index_key=*/std::nullopt,
              },
          },
      },
      /*get_all_parameters=*/
      {
          .result_type = blink::mojom::IDBGetAllResultType::Keys,
          .max_count = 2,
      },
      expected_results));
}

TEST_F(DatabaseOperationTest, ObjectStoreGetAllRecordsWithPrevDirection) {
  const blink::mojom::IDBRecordPtr expected_results[] = {
      blink::mojom::IDBRecord::New(IndexedDBKey{"key3"},
                                   /*value=*/CreateIDBReturnValuePtr("value3"),
                                   /*index_key=*/std::nullopt),
      blink::mojom::IDBRecord::New(IndexedDBKey{"key2"},
                                   /*value=*/CreateIDBReturnValuePtr("value2"),
                                   /*index_key=*/std::nullopt),
      blink::mojom::IDBRecord::New(IndexedDBKey{"key1"},
                                   /*value=*/CreateIDBReturnValuePtr("value1"),
                                   /*index_key=*/std::nullopt),
  };

  ASSERT_NO_FATAL_FAILURE(TestGetAll(
      /*database_parameters=*/
      {
          /*object_store_parameters=*/
          {
              .object_store_id = kTestObjectStoreId,
          },
          /*index_parameters=*/{},
          /*database_records=*/
          {
              {
                  IndexedDBKey{"key1"},
                  {"value1", /*external_objects=*/{}},
                  /*index_key=*/std::nullopt,
              },
              {
                  IndexedDBKey{"key2"},
                  {"value2", /*external_objects=*/{}},
                  /*index_key=*/std::nullopt,
              },
              {
                  IndexedDBKey{"key3"},
                  {"value3", /*external_objects=*/{}},
                  /*index_key=*/std::nullopt,
              },
          },
      },
      /*get_all_parameters=*/
      {
          .result_type = blink::mojom::IDBGetAllResultType::Records,
          .direction = blink::mojom::IDBCursorDirection::Prev,
      },
      expected_results));
}

TEST_F(DatabaseOperationTest, IndexGetAllRecordsWithNextNoDuplicateDirection) {
  const blink::mojom::IDBRecordPtr expected_results[] = {
      blink::mojom::IDBRecord::New(IndexedDBKey{"key1"},
                                   /*value=*/CreateIDBReturnValuePtr("value1"),
                                   IndexedDBKey{"index_key1"}),
      blink::mojom::IDBRecord::New(IndexedDBKey{"key2"},
                                   /*value=*/CreateIDBReturnValuePtr("value2"),
                                   IndexedDBKey{"index_key2"}),
      blink::mojom::IDBRecord::New(IndexedDBKey{"key4"},
                                   /*value=*/CreateIDBReturnValuePtr("value4"),
                                   IndexedDBKey{"index_key3"}),
  };

  ASSERT_NO_FATAL_FAILURE(TestGetAll(
      /*database_parameters=*/
      {
          /*object_store_parameters=*/
          {
              .object_store_id = kTestObjectStoreId,
          },
          /*index_parameters=*/
          {
              .index_id = kTestIndexId,
          },
          /*database_records=*/
          {
              {
                  IndexedDBKey{"key1"},
                  {"value1", /*external_objects=*/{}},
                  IndexedDBKey("index_key1"),
              },
              {
                  IndexedDBKey{"key2"},
                  {"value2", /*external_objects=*/{}},
                  IndexedDBKey("index_key2"),
              },
              {
                  IndexedDBKey{"key3"},
                  {"value3", /*external_objects=*/{}},
                  IndexedDBKey("index_key2"),
              },
              {
                  IndexedDBKey{"key4"},
                  {"value4", /*external_objects=*/{}},
                  IndexedDBKey("index_key3"),
              },
              {
                  IndexedDBKey{"key5"},
                  {"value5", /*external_objects=*/{}},
                  IndexedDBKey("index_key3"),
              },
              {
                  IndexedDBKey{"key6"},
                  {"value6", /*external_objects=*/{}},
                  IndexedDBKey("index_key3"),
              },
          },
      },
      /*get_all_parameters=*/
      {
          .result_type = blink::mojom::IDBGetAllResultType::Records,
          .direction = blink::mojom::IDBCursorDirection::NextNoDuplicate,
      },
      expected_results));
}

TEST_F(DatabaseOperationTest, IndexGetAllRecordsWithPrevNoDuplicateDirection) {
  const blink::mojom::IDBRecordPtr expected_results[] = {
      blink::mojom::IDBRecord::New(IndexedDBKey{"key4"},
                                   /*value=*/CreateIDBReturnValuePtr("value4"),
                                   IndexedDBKey{"index_key3"}),
      blink::mojom::IDBRecord::New(IndexedDBKey{"key2"},
                                   /*value=*/CreateIDBReturnValuePtr("value2"),
                                   IndexedDBKey{"index_key2"}),
      blink::mojom::IDBRecord::New(IndexedDBKey{"key1"},
                                   /*value=*/CreateIDBReturnValuePtr("value1"),
                                   IndexedDBKey{"index_key1"}),
  };

  ASSERT_NO_FATAL_FAILURE(TestGetAll(
      /*database_parameters=*/
      {
          /*object_store_parameters=*/
          {
              .object_store_id = kTestObjectStoreId,
          },
          /*index_parameters=*/
          {
              .index_id = kTestIndexId,
          },
          /*database_records=*/
          {
              {
                  IndexedDBKey{"key1"},
                  {"value1", /*external_objects=*/{}},
                  IndexedDBKey("index_key1"),
              },
              {
                  IndexedDBKey{"key2"},
                  {"value2", /*external_objects=*/{}},
                  IndexedDBKey("index_key2"),
              },
              {
                  IndexedDBKey{"key3"},
                  {"value3", /*external_objects=*/{}},
                  IndexedDBKey("index_key2"),
              },
              {
                  IndexedDBKey{"key4"},
                  {"value4", /*external_objects=*/{}},
                  IndexedDBKey("index_key3"),
              },
              {
                  IndexedDBKey{"key5"},
                  {"value5", /*external_objects=*/{}},
                  IndexedDBKey("index_key3"),
              },
              {
                  IndexedDBKey{"key6"},
                  {"value6", /*external_objects=*/{}},
                  IndexedDBKey("index_key3"),
              },
          },
      },
      /*get_all_parameters=*/
      {
          .result_type = blink::mojom::IDBGetAllResultType::Records,
          .direction = blink::mojom::IDBCursorDirection::PrevNoDuplicate,
      },
      expected_results));
}

TEST_F(DatabaseOperationTest, ObjectStoreGetAllKeysWithInvalidObjectStoreId) {
  ASSERT_EQ(0u, db_->metadata().object_stores.size());

  // Call `Database::GetAllOperation` with an invalid object store id, which
  // must fail with an invalid argument status.
  FakeGetAllResultSink result_sink;
  blink::mojom::IDBDatabase::GetAllCallback get_all_callback = base::BindOnce(
      &FakeGetAllResultSink::BindReceiver, base::Unretained(&result_sink));

  std::unique_ptr<Database::GetAllResultSinkWrapper> result_sink_wrapper =
      std::make_unique<Database::GetAllResultSinkWrapper>(
          transaction_->AsWeakPtr(), std::move(get_all_callback));
  result_sink_wrapper->UseDedicatedReceiverForTesting();

  TestGetAllParameters get_all_parameters;

  std::unique_ptr<blink::IndexedDBKeyRange> key_range =
      std::make_unique<blink::IndexedDBKeyRange>(get_all_parameters.key_range);

  Status status = db_->GetAllOperation(
      kTestObjectStoreId,
      /*index_id=*/blink::IndexedDBIndexMetadata::kInvalidId,
      std::move(key_range), get_all_parameters.result_type,
      get_all_parameters.max_count, get_all_parameters.direction,
      std::move(result_sink_wrapper), transaction_);
  ASSERT_TRUE(status.IsInvalidArgument()) << status.ToString();

  // Verify that the result sink received an error.
  result_sink.WaitForResults();
  ASSERT_NE(result_sink.GetError(), nullptr);
  EXPECT_EQ(result_sink.GetError()->error_code,
            blink::mojom::IDBException::kUnknownError);

  // Perform cleanup.
  transaction_->SetCommitFlag();
  transaction_ = nullptr;
  RunPostedTasks();
}

TEST_F(DatabaseOperationTest, IndexGetAllKeysWithInvalidIndexId) {
  // Create an object store.
  ASSERT_EQ(0u, db_->metadata().object_stores.size());
  Status status = db_->CreateObjectStoreOperation(
      kTestObjectStoreId, u"store", IndexedDBKeyPath(),
      /*auto_increment=*/false, transaction_);
  ASSERT_TRUE(status.ok()) << status.ToString();
  ASSERT_EQ(1u, db_->metadata().object_stores.size());

  // Call `Database::GetAllOperation` with an invalid index id, which must fail
  // with an invalid argument status.
  FakeGetAllResultSink result_sink;
  blink::mojom::IDBDatabase::GetAllCallback get_all_callback = base::BindOnce(
      &FakeGetAllResultSink::BindReceiver, base::Unretained(&result_sink));

  std::unique_ptr<Database::GetAllResultSinkWrapper> result_sink_wrapper =
      std::make_unique<Database::GetAllResultSinkWrapper>(
          transaction_->AsWeakPtr(), std::move(get_all_callback));
  result_sink_wrapper->UseDedicatedReceiverForTesting();

  TestGetAllParameters get_all_parameters;

  std::unique_ptr<blink::IndexedDBKeyRange> key_range =
      std::make_unique<blink::IndexedDBKeyRange>(get_all_parameters.key_range);

  status = db_->GetAllOperation(
      kTestObjectStoreId, kTestIndexId, std::move(key_range),
      get_all_parameters.result_type, get_all_parameters.max_count,
      get_all_parameters.direction, std::move(result_sink_wrapper),
      transaction_);
  ASSERT_TRUE(status.IsInvalidArgument()) << status.ToString();

  // Verify that the result sink received an error.
  result_sink.WaitForResults();
  ASSERT_NE(result_sink.GetError(), nullptr);
  EXPECT_EQ(result_sink.GetError()->error_code,
            blink::mojom::IDBException::kUnknownError);

  // Perform cleanup.
  transaction_->SetCommitFlag();
  transaction_ = nullptr;
  RunPostedTasks();
}

TEST_F(DatabaseOperationTest,
       ObjectStoreGetAllRecordsWithMultipleResultChunks) {
  // Generate 2.5 chunks of results.
  const size_t record_count = (blink::mojom::kIDBGetAllChunkSize * 2) +
                              (blink::mojom::kIDBGetAllChunkSize / 2);

  std::vector<TestIDBRecord> database_records;
  std::vector<blink::mojom::IDBRecordPtr> expected_results;

  for (size_t i = 0u; i < record_count; ++i) {
    const std::string primary_key = base::StringPrintf("key%zu", i);
    const std::string value = base::StringPrintf("value%zu", i);

    database_records.push_back({IndexedDBKey{primary_key},
                                {value, /*external_objects=*/{}},
                                /*index_key=*/std::nullopt});

    expected_results.emplace_back(
        blink::mojom::IDBRecord::New(IndexedDBKey{primary_key},
                                     /*value=*/CreateIDBReturnValuePtr(value),
                                     /*index_key=*/std::nullopt));
  }

  // Sort the expected results by primary key.
  std::sort(expected_results.begin(), expected_results.end(),
            [](const blink::mojom::IDBRecordPtr& left,
               const blink::mojom::IDBRecordPtr& right) {
              return left->primary_key->IsLessThan(*right->primary_key);
            });

  ASSERT_NO_FATAL_FAILURE(TestGetAll(
      /*database_parameters=*/
      {
          /*object_store_parameters=*/
          {
              .object_store_id = kTestObjectStoreId,
          },
          /*index_parameters=*/{},
          std::move(database_records),
      },
      /*get_all_parameters=*/
      {
          .result_type = blink::mojom::IDBGetAllResultType::Records,
      },
      expected_results));
}

TEST_F(DatabaseOperationTest, IndexGetAllRecordsWithAutoIncrementingKeys) {
  const IndexedDBKeyPath object_store_key_path{u"id"};

  const IndexedDBKey expected_generated_keys[] = {
      IndexedDBKey(1.0, blink::mojom::IDBKeyType::Number),
      IndexedDBKey(2.0, blink::mojom::IDBKeyType::Number),
      IndexedDBKey(3.0, blink::mojom::IDBKeyType::Number),
  };

  const blink::mojom::IDBRecordPtr expected_results[] = {
      blink::mojom::IDBRecord::New(
          /*primary_key=*/expected_generated_keys[2],
          /*value=*/
          CreateIDBReturnValuePtr("value3", expected_generated_keys[2],
                                  object_store_key_path),
          /*index_key=*/IndexedDBKey{"index_key1"}),
      blink::mojom::IDBRecord::New(
          /*primary_key=*/expected_generated_keys[1],
          /*value=*/
          CreateIDBReturnValuePtr("value2", expected_generated_keys[1],
                                  object_store_key_path),
          /*index_key=*/IndexedDBKey{"index_key2"}),
      blink::mojom::IDBRecord::New(
          /*primary_key=*/expected_generated_keys[0],
          /*value=*/
          CreateIDBReturnValuePtr("value1", expected_generated_keys[0],
                                  object_store_key_path),
          /*index_key=*/IndexedDBKey{"index_key3"}),
  };

  ASSERT_NO_FATAL_FAILURE(TestGetAll(
      /*database_parameters=*/
      {
          /*object_store_parameters=*/
          {
              .object_store_id = kTestObjectStoreId,
              .key_path = object_store_key_path,
              .auto_increment = true,
          },
          /*index_parameters=*/
          {
              .index_id = kTestIndexId,
          },
          /*database_records=*/
          {
              {
                  /*primary_key (generated)=*/IndexedDBKey(),
                  {"value1", /*external_objects=*/{}},
                  IndexedDBKey("index_key3"),
              },
              {
                  /*primary_key (generated)=*/IndexedDBKey(),
                  {"value2", /*external_objects=*/{}},
                  IndexedDBKey("index_key2"),
              },
              {
                  /*primary_key (generated)=*/IndexedDBKey(),
                  {"value3", /*external_objects=*/{}},
                  IndexedDBKey("index_key1"),
              },
          },
      },
      /*get_all_parameters=*/
      {
          .result_type = blink::mojom::IDBGetAllResultType::Records,
      },
      expected_results));
}

}  // namespace content::indexed_db
